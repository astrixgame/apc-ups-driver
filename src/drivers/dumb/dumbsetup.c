/*
 * dumbsetup.c
 *
 * Functions to open/setup/close the device
 */

/*
 * Copyright (C) 1999-2001 Riccardo Facchetti <riccardo@apcupsd.org>
 * Copyright (C) 1996-99 Andre M. Hedrick <andre@suse.com>
 * Copyright (C) 1999-2001 Riccardo Facchetti <riccardo@apcupsd.org>
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
#include "dumb.h"

DumbUpsDriver::DumbUpsDriver(UPSINFO *ups) :
   UpsDriver(ups),
   _sp_flags(0)
{
   memset(&_oldtio, 0, sizeof(_oldtio));
   memset(&_newtio, 0, sizeof(_newtio));
}

/*
 * This is the first routine called in the driver, and it is only
 * called once.
 */
bool DumbUpsDriver::Open()
{
   char *opendev = _ups->device;

#ifdef HAVE_MINGW
   // On Win32 add \\.\ UNC prefix to COMx in order to correctly address
   // ports >= COM10.
   char device[MAXSTRING];
   if (!strnicmp(_ups->device, "COM", 3)) {
      snprintf(device, sizeof(device), "\\\\.\\%s", _ups->device);
      opendev = device;
   }
#endif

   if ((_ups->fd = open(opendev, O_RDWR | O_NOCTTY | O_NDELAY | O_CLOEXEC)) < 0)
      Error_abort("Cannot open UPS port %s: %s\n", opendev, strerror(errno));

   /* Cancel the no delay we just set */
   int cmd = fcntl(_ups->fd, F_GETFL, 0);
   fcntl(_ups->fd, F_SETFL, cmd & ~O_NDELAY);

   /* Save old settings */
   tcgetattr(_ups->fd, &_oldtio);

   _newtio.c_cflag = DEFAULT_SPEED | CS8 | CLOCAL | CREAD;
   _newtio.c_iflag = IGNPAR;    /* Ignore errors, raw input */
   _newtio.c_oflag = 0;         /* Raw output */
   _newtio.c_lflag = 0;         /* No local echo */

#if defined(HAVE_OPENBSD_OS) || \
    defined(HAVE_FREEBSD_OS) || \
    defined(HAVE_NETBSD_OS)
   _newtio.c_ispeed = DEFAULT_SPEED;    /* Set input speed */
   _newtio.c_ospeed = DEFAULT_SPEED;    /* Set output speed */
#endif   /* __openbsd__ || __freebsd__ || __netbsd__  */

   /* This makes a non.blocking read() with TIMER_READ (10) sec. timeout */
   _newtio.c_cc[VMIN] = 0;
   _newtio.c_cc[VTIME] = TIMER_READ * 10;

#if defined(HAVE_OSF1_OS) || defined(HAVE_LINUX_OS)
   (void)cfsetospeed(&_newtio, DEFAULT_SPEED);
   (void)cfsetispeed(&_newtio, DEFAULT_SPEED);
#endif   /* do it the POSIX way */

   tcflush(_ups->fd, TCIFLUSH);
   tcsetattr(_ups->fd, TCSANOW, &_newtio);
   tcflush(_ups->fd, TCIFLUSH);

   _ups->clear_slave();

   return 1;
}

/*
 * This is the last routine called in the driver
 */
bool DumbUpsDriver::Close()
{
   int rts_bit = TIOCM_RTS;
   int st_bit = TIOCM_ST;
   int dtr_bit = TIOCM_DTR;

   /*
    * Do NOT reset the old values here as it causes the kill
    * power to trigger on some systems.
    *                 
    * On the other hand do clear any kill_power bit previously
    * set so that it doesn't remain set in the serial port and
    * trigger a problem later.
    *
    * Note, we assume that the previous code has done a 
    * sleep() for at least 5 seconds.
    */

   switch (_ups->cable.type) {
   case CUSTOM_SIMPLE:
   case APC_940_0095A:
   case APC_940_0095B:
   case APC_940_0095C:            /* clear killpwr_bit */
      (void)ioctl(_ups->fd, TIOCMBIC, &rts_bit);
      (void)ioctl(_ups->fd, TIOCMBIC, &rts_bit);
      (void)ioctl(_ups->fd, TIOCMBIC, &st_bit);
      break;

   case APC_940_0119A:
   case APC_940_0127A:
   case APC_940_0128A:
   case APC_940_0020B:            /* clear killpwr_bit */
   case APC_940_0020C:
      (void)ioctl(_ups->fd, TIOCMBIC, &dtr_bit);
      (void)ioctl(_ups->fd, TIOCMBIC, &dtr_bit);
      (void)ioctl(_ups->fd, TIOCMBIC, &dtr_bit);
      break;

   default:
      break;
   }

   close(_ups->fd);
   _ups->fd = -1;

   return 1;
}

bool DumbUpsDriver::setup()
{
   int serial_bits = 0;

   switch (_ups->cable.type) {
   case CUSTOM_SIMPLE:
      /* Clear killpwr bits */
      serial_bits = TIOCM_RTS;
      (void)ioctl(_ups->fd, TIOCMBIC, &serial_bits);

      /* Set bit for detecting Low Battery */
      serial_bits = TIOCM_DTR;
      (void)ioctl(_ups->fd, TIOCMBIS, &serial_bits);
      break;

   case APC_940_0119A:
   case APC_940_0127A:
   case APC_940_0128A:
   case APC_940_0020B:
   case APC_940_0020C:
   case MAM_CABLE:                /* DTR=>enable CD & CTS RTS=>killpower */
      /* Clear DTR bit (shutdown) and set RTS bit (tell we are ready) */
      serial_bits = TIOCM_DTR;
      (void)ioctl(_ups->fd, TIOCMBIC, &serial_bits);
      serial_bits = TIOCM_RTS;
      (void)ioctl(_ups->fd, TIOCMBIS, &serial_bits);
      break;

   case APC_940_0095A:
   case APC_940_0095B:
   case APC_940_0095C:
      /* Have to clear RTS line to access the serial cable mode PnP */
      serial_bits = TIOCM_RTS;
      (void)ioctl(_ups->fd, TIOCMBIC, &serial_bits);

      /* Clear killpwr, lowbatt and again killpwr bits. */
      serial_bits = TIOCM_RTS | TIOCM_CD;
      (void)ioctl(_ups->fd, TIOCMBIC, &serial_bits);
      serial_bits = TIOCM_RTS;
      (void)ioctl(_ups->fd, TIOCMBIC, &serial_bits);
      break;

   default:
      break;
   }

   return 1;
}
