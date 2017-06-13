/* libanode: the Anode C reference implementation
 * Copyright (C) 2009-2010 Adam Ierymenko <adam.ierymenko@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "thread.h"
#include <stdlib.h>

#ifdef WINDOWS

#else /* not WINDOWS */

struct _AnodeThread
{
  void (*func)(void *);
  void *arg;
  int wait_for_join;
  pthread_t thread;
};

static void *_AnodeThread_main(void *arg)
{
  ((struct _AnodeThread *)arg)->func(((struct _AnodeThread *)arg)->arg);
  if (!((struct _AnodeThread *)arg)->wait_for_join)
    free(arg);
  return (void *)0;
}

AnodeThread *AnodeThread_create(void (*func)(void *),void *arg,int wait_for_join)
{
  struct _AnodeThread *t = malloc(sizeof(struct _AnodeThread));
  t->func = func;
  t->arg = arg;
  t->wait_for_join = wait_for_join;
  pthread_create(&t->thread,(const pthread_attr_t *)0,&_AnodeThread_main,(void *)t);
  if (!wait_for_join)
    pthread_detach(t->thread);
  return (AnodeThread *)t;
}

void AnodeThread_join(AnodeThread *thread)
{
  pthread_join(((struct _AnodeThread *)thread)->thread,(void **)0);
  free((void *)thread);
}

#endif /* WINDOWS / not WINDOWS */
