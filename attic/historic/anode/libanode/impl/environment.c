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

#include <stdio.h>
#include <stdlib.h>
#include "environment.h"

#ifdef WINDOWS
#include <windows.h>
#else
#include <sys/stat.h>
#include <string.h>
#endif

static char Anode_cache_base[1024] = { 0 };

const char *Anode_get_cache()
{
  if (Anode_cache_base[0])
    return Anode_cache_base;

#ifdef WINDOWS
#else
  char tmp[1024];
  char home[1024];
  unsigned int i;
  struct stat st;
  const char *_home = getenv("HOME");

  if (!_home)
    return (const char *)0;
  for(i=0;i<sizeof(home);++i) {
    home[i] = _home[i];
    if (!home[i]) {
      if (i == 0)
        return (const char *)0;
      else if (home[i-1] == ANODE_PATH_SEPARATOR)
        home[i-1] = (char)0;
      break;
    }
  }
  if (i == sizeof(home))
    return (const char *)0;

#ifdef __APPLE__
  snprintf(tmp,sizeof(tmp),"%s%cLibrary",home,ANODE_PATH_SEPARATOR);
  tmp[sizeof(tmp)-1] = (char)0;
  if (!stat(tmp,&st)) {
    sprintf(tmp,"%s%cLibrary%cCaches",home,ANODE_PATH_SEPARATOR,ANODE_PATH_SEPARATOR);
    if (stat(tmp,&st)) {
      if (mkdir(tmp,0700))
        return (const char *)0;
    }
    snprintf(Anode_cache_base,sizeof(Anode_cache_base),"%s%ccom.zerotier.anode",tmp,ANODE_PATH_SEPARATOR);
    Anode_cache_base[sizeof(Anode_cache_base)-1] = (char)0;
    if (stat(Anode_cache_base,&st)) {
      if (mkdir(Anode_cache_base,0700)) {
        Anode_cache_base[0] = (char)0;
        return (const char *)0;
      }
    }
    return Anode_cache_base;
  }
#endif

  snprintf(tmp,sizeof(tmp),"%s%c.anode",home,ANODE_PATH_SEPARATOR);
  tmp[sizeof(tmp)-1] = (char)0;
  if (stat(tmp,&st)) {
    if (mkdir(tmp,0700)) {
      Anode_cache_base[0] = (char)0;
      return (const char *)0;
    }
  }
  snprintf(Anode_cache_base,sizeof(Anode_cache_base),"%s%ccaches",tmp,ANODE_PATH_SEPARATOR);
  Anode_cache_base[sizeof(Anode_cache_base)-1] = (char)0;
  if (stat(Anode_cache_base,&st)) {
    if (mkdir(Anode_cache_base,0700)) {
      Anode_cache_base[0] = (char)0;
      return (const char *)0;
    }
  }

  return Anode_cache_base;
#endif
}

char *Anode_get_cache_sub(const char *cache_subdir,char *buf,unsigned int len)
{
  struct stat st;
  const char *cache_base = Anode_get_cache();

  if (!len)
    return (char *)0;
  if (!cache_base)
    return (char *)0;

  snprintf(buf,len,"%s%c%s",cache_base,ANODE_PATH_SEPARATOR,cache_subdir);
  buf[len-1] = (char)0;
  if (stat(buf,&st)) {
    if (mkdir(buf,0700))
      return (char *)0;
  }

  return buf;
}
