/*
 * atimer.cpp
 */

/*
 * Copyright (C) 2009 Adam Kropelin
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

#include "atimer.h"
#include "autil.h"
#include <errno.h>

atimer::atimer(client &cli, int id)
   : _client(cli),
     _id(id),
     _started(false)
{
   pthread_mutex_init(&_mutex, NULL);
   pthread_cond_init(&_condvar, NULL);
}

atimer::~atimer()
{
   stop();
   pthread_cond_destroy(&_condvar);
   pthread_mutex_destroy(&_mutex);
}

void atimer::start(unsigned long timeout)
{
   pthread_mutex_lock(&_mutex);

   if (!_started)
   {
      calc_abstimeout(timeout, &_abstimeout);
      _started = true;
      run();
   }

   pthread_mutex_unlock(&_mutex);
}

void atimer::stop()
{
   pthread_mutex_lock(&_mutex);
   if (_started)
   {
      _started = false;
      pthread_cond_signal(&_condvar);
      pthread_mutex_unlock(&_mutex);
      join();
   }
   else
      pthread_mutex_unlock(&_mutex);
}

void atimer::body()
{
   int rc;

   pthread_mutex_lock(&_mutex);

   while (_started)
   {
      rc = pthread_cond_timedwait(&_condvar, &_mutex, &_abstimeout);
      if (rc == ETIMEDOUT)
      {
         // Timeout ocurred, invoke callback...

         // No longer running
         _started = false;

         // Unlock around callback to allow restart
         pthread_mutex_unlock(&_mutex);
         _client.HandleTimeout(_id);
         pthread_mutex_lock(&_mutex);
      }
      else if (rc != 0)
      {
         // Error, exit timer loop
         _started = false;
      }
      else
      {
         // Condvar was signaled or we got a spurrious wakeup
         // In either case, we just ignore it and loop around
      }
   }

   pthread_mutex_unlock(&_mutex);
}
