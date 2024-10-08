/*
 * apcnis.c
 *
 * Network server for apcupsd.
 */

/*
 * Copyright (C) 1999-2006 Kern Sibbald
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General
 * Public License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1335, USA.
 */

#include "apc.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


static char largebuf[4096];
static int stat_recs;

struct s_arg {
   UPSINFO *ups;
   int newsockfd;
};

void *handle_client_request(void *arg);

static void status_open(UPSINFO *ups) {
   P(mutex);
   largebuf[0] = 0;
   stat_recs = 0;
}

#define STAT_REV 1

/*
 * Send the status lines across the network one line
 * at a time (to prevent sending too large a buffer).
 *
 * Returns -1 on error or EOF
 *          0 OK
 */
static int status_close(UPSINFO *ups, int nsockfd) {
   int i;
   char buf[MAXSTRING];
   char *sptr, *eptr;

   i = strlen(largebuf);
   asnprintf(buf, sizeof(buf), "APC      : %03d,%03d,%04d\n",
      STAT_REV, stat_recs, i);

   if(net_send(nsockfd, buf, strlen(buf)) <= 0) {
      V(mutex);
      return -1;
   }

   sptr = eptr = largebuf;
   for(; i > 0; i--) {
      if(*eptr == '\n') {
         eptr++;
         if(net_send(nsockfd, sptr, eptr - sptr) <= 0)
            break;
         sptr = eptr;
      } else {
         eptr++;
      }
   }

   if(net_send(nsockfd, NULL, 0) < 0) {
      V(mutex);
      return -1;
   }

   V(mutex);
   return 0;
}

/*
 * Buffer up the status messages so that they can be sent
 * by the status_close() routine over the network.
 */
static void status_write(UPSINFO *ups, const char *fmt, ...) {
   va_list ap;
   int i;
   char buf[MAXSTRING];

   va_start(ap, fmt);
   avsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);

   if((i = (strlen(largebuf) + strlen(buf))) < (int)(sizeof(largebuf) - 1)) {
      strlcat(largebuf, buf, sizeof(largebuf));
      stat_recs++;
   } else {
      log_event(ups, LOG_ERR,
         "apcserver.c: Status buffer overflow %d bytes\n", i - sizeof(largebuf));
   }
}


void do_server(UPSINFO *ups) {
   int newsockfd, sockfd;
   struct sockaddr_in cli_addr;
   struct sockaddr_in serv_addr;
   int tlog;
   struct s_arg *arg;
   struct in_addr local_ip;
#ifndef HAVE_MINGW
   int turnon = 1;
#endif

   for(tlog = 0; (ups = attach_ups(ups)) == NULL; tlog -= 5 * 60) {
      if(tlog <= 0) {
         tlog = 60 * 60;
         log_event(ups, LOG_ERR, "apcserver: Cannot attach SYSV IPC.\n");
      }
      sleep(5 * 60);
   }

   local_ip.s_addr = INADDR_ANY;

   if(ups->nisip[0]) {
      if(inet_pton(AF_INET, ups->nisip, &local_ip) != 1) {
         log_event(ups, LOG_WARNING, "Invalid NISIP specified: '%s'", ups->nisip);
         local_ip.s_addr = INADDR_ANY;
      }
   }

   /* Open a TCP socket */
   for(tlog = 0; (sockfd = socket_cloexec(AF_INET, SOCK_STREAM, 0)) < 0; tlog -= 5 * 60) {
      if(tlog <= 0) {
         tlog = 60 * 60;
         log_event(ups, LOG_ERR, "apcserver: cannot open stream socket");
      }
      sleep(5 * 60);
   }

   /* Reuse old sockets */
#ifndef HAVE_MINGW
   if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&turnon, sizeof(turnon)) < 0) {
      log_event(ups, LOG_WARNING, "Cannot set SO_REUSEADDR on socket: %s\n",
         strerror(errno));
   }
#endif
   
   /* Bind our local address so that the client can send to us. */
   memset((char *)&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr = local_ip;
   serv_addr.sin_port = htons(ups->statusport);

   for(tlog = 0; bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0;
      tlog -= 5 * 60) {
      if(tlog <= 0) {
         tlog = 60 * 60;
         log_event(ups, LOG_ERR, "apcserver: cannot bind port %d. ERR=%s",
            ups->statusport, strerror(errno));
      }
      sleep(5 * 60);
   }
   listen(sockfd, 5);              /* tell system we are ready */

   log_event(ups, LOG_INFO, "NIS server startup succeeded");

   for(;;) {
      /* Wait for a connection from a client process. */
      for(tlog = 0; (newsockfd = net_accept(sockfd, &cli_addr)) < 0; tlog -= 5 * 60) {
         if(tlog <= 0) {
            tlog = 60 * 60;
            log_event(ups, LOG_ERR, "apcserver: accept error. ERR=%s",
               strerror(errno));
         }
         sleep(5 * 60);
      }

#ifdef HAVE_LIBWRAP
      /*
       * This function checks the incoming client and if it's not
       * allowed closes the connection.
       */
      if(check_wrappers(argvalue, newsockfd) == FAILURE) {
         net_close(newsockfd);
         continue;
      }
#endif

      arg = (struct s_arg *)malloc(sizeof(struct s_arg));
      arg->newsockfd = newsockfd;
      arg->ups = ups;

      pthread_t tid;
      pthread_create(&tid, NULL, handle_client_request, arg);

   }
}

/* 
 * Accept requests from client.  Send output one line
 * at a time followed by a zero length transmission.
 *
 * Return when the connection is terminated or there
 * is an error.
 */
void *handle_client_request(void *arg) {
   int len;
   char line[MAXSTRING];
   const char errmsg[] = "Invalid command\n";
   const char notrun[] = "Apcupsd internal error\n";
   int nsockfd = ((struct s_arg *)arg)->newsockfd;
   UPSINFO *ups = ((struct s_arg *)arg)->ups;
   free(arg);

   pthread_detach(pthread_self());

   if((ups = attach_ups(ups)) == NULL) {
      net_send(nsockfd, notrun, sizeof(notrun));
      net_send(nsockfd, NULL, 0);
      net_close(nsockfd);
      return NULL;
   }

   for(;;) {
      if((len = net_recv(nsockfd, line, sizeof(line))) <= 0)
         break;

      if(len == 6 && strncmp("status", line, 6) == 0) {
         if(output_status(ups, nsockfd, status_open, status_write, status_close) < 0)
            break;
      } else {
         net_send(nsockfd, errmsg, sizeof(errmsg));
         if(net_send(nsockfd, NULL, 0) < 0)
            break;
      }
   }

   net_close(nsockfd);

   detach_ups(ups);
   return NULL;
}