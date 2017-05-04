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
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "impl/types.h"
#include "impl/misc.h"
#include "impl/dictionary.h"
#include "impl/environment.h"
#include "impl/http_client.h"
#include "anode.h"

static const char *_MONTHS[12] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
static const char *_DAYS_OF_WEEK[7] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
static inline unsigned long get_file_time_for_http(const char *path,char *buf,unsigned int len)
{
  struct stat st;
  struct tm *gmt;

  if (!stat(path,(struct stat *)&st)) {
    gmt = gmtime(&st.st_mtime);
    if (gmt) {
      snprintf(buf,len,"%s, %d %s %d %d:%d:%d GMT",
        _DAYS_OF_WEEK[gmt->tm_wday],
        gmt->tm_mday,
        _MONTHS[gmt->tm_mon],
        (1900 + gmt->tm_year),
        gmt->tm_hour,
        gmt->tm_min,
        gmt->tm_sec);
      buf[len - 1] = (char)0;
      return (unsigned long)st.st_size;
    }
  }

  return 0;
}

struct AnodeZoneLookupJob
{
  char cached_zone_file[2048];
  struct AnodeDictionary *zone_dict;
  AnodeZone zone;
  void *ptr;
  void (*zone_lookup_handler)(void *,const AnodeZone *,AnodeZoneFile *);
  int had_cached_zone;
};

static void AnodeZone_lookup_http_handler(struct AnodeHttpClient *client)
{
  char *data_tmp;
  struct AnodeZoneLookupJob *job = (struct AnodeZoneLookupJob *)client->ptr[0];
  FILE *zf;

  if ((client->response.code == 200)&&(client->response.data_length > 0)) {
    zf = fopen(job->cached_zone_file,"w");
    if (zf) {
      fwrite(client->response.data,1,client->response.data_length,zf);
      fclose(zf);
    }

    data_tmp = (char *)malloc(client->response.data_length + 1);
    Anode_memcpy((void *)data_tmp,client->response.data,client->response.data_length);
    data_tmp[client->response.data_length] = (char)0;

    AnodeDictionary_clear(job->zone_dict);
    AnodeDictionary_read(
      job->zone_dict,
      data_tmp,
      "\r\n",
      "=",
      ";",
      '\\',
      1,1);

    free((void *)data_tmp);

    job->zone_lookup_handler(job->ptr,&job->zone,(AnodeZoneFile *)job->zone_dict);
  } else if (job->had_cached_zone)
    job->zone_lookup_handler(job->ptr,&job->zone,(AnodeZoneFile *)job->zone_dict);
  else {
    AnodeDictionary_destroy(job->zone_dict);
    free((void *)job->zone_dict);
    job->zone_lookup_handler(job->ptr,&job->zone,(AnodeZoneFile *)0);
  }

  free((void *)job);
  AnodeHttpClient_free(client);
}

void AnodeZone_lookup(
  AnodeTransportEngine *transport,
  const AnodeZone *zone,
  void *ptr,
  void (*zone_lookup_handler)(void *,const AnodeZone *,AnodeZone *))
{
  char cached_zones_folder[2048];
  char cached_zone_file[2048];
  char if_modified_since[256];
  unsigned long file_size;
  struct AnodeZoneLookupJob *job;
  struct AnodeHttpClient *client;
  char *file_data;
  FILE *zf;

  if (Anode_get_cache_sub("zones",cached_zones_folder,sizeof(cached_zones_folder))) {
    snprintf(cached_zone_file,sizeof(cached_zone_file),"%s%c%.2x%.2x%.2x%.2x.z",cached_zones_folder,ANODE_PATH_SEPARATOR,(unsigned int)zone->bits[0],(unsigned int)zone->bits[1],(unsigned int)zone->bits[2],(unsigned int)zone->bits[3]);
    cached_zone_file[sizeof(cached_zone_file)-1] = (char)0;

    job = (struct AnodeZoneLookupJob *)malloc(sizeof(struct AnodeZoneLookupJob));
    Anode_str_copy(job->cached_zone_file,cached_zone_file,sizeof(job->cached_zone_file));
    job->zone_dict = (struct AnodeDictionary *)malloc(sizeof(struct AnodeDictionary));
    AnodeDictionary_init(job->zone_dict,0);
    job->zone.bits[0] = zone->bits[0];
    job->zone.bits[1] = zone->bits[1];
    job->zone.bits[2] = zone->bits[2];
    job->zone.bits[3] = zone->bits[3];
    job->ptr = ptr;
    job->zone_lookup_handler = zone_lookup_handler;
    job->had_cached_zone = 0;

    client = AnodeHttpClient_new(transport);

    Anode_str_copy(client->uri.scheme,"http",sizeof(client->uri.scheme));
    snprintf(client->uri.host,sizeof(client->uri.host),"a--%.2x%.2x%.2x%.2x.net",(unsigned int)zone->bits[0],(unsigned int)zone->bits[1],(unsigned int)zone->bits[2],(unsigned int)zone->bits[3]);
    client->uri.host[sizeof(client->uri.host)-1] = (char)0;
    Anode_str_copy(client->uri.path,"/z",sizeof(client->uri.path));

    client->handler = &AnodeZone_lookup_http_handler;
    client->ptr[0] = job;

    if ((file_size = get_file_time_for_http(cached_zone_file,if_modified_since,sizeof(if_modified_since)))) {
      zf = fopen(cached_zone_file,"r");
      if (zf) {
        AnodeDictionary_put(&client->headers,"If-Modified-Since",if_modified_since);
        file_data = (char *)malloc(file_size + 1);
        if (fread((void *)file_data,1,file_size,zf)) {
          file_data[file_size] = (char)0;
          AnodeDictionary_read(
            job->zone_dict,
            file_data,
            "\r\n",
            "=",
            ";",
            '\\',
            1,1);
          job->had_cached_zone = 1;
        }
        free((void *)file_data);
        fclose(zf);
      }
    }

    AnodeHttpClient_send(client);
  } else zone_lookup_handler(ptr,zone,(AnodeZone *)0);
}

const char *AnodeZoneFile_get(AnodeZoneFile *zone,const char *key)
{
  return AnodeDictionary_get((struct AnodeDictionary *)zone,key);
}

void AnodeZoneFile_free(AnodeZoneFile *zone)
{
  AnodeDictionary_destroy((struct AnodeDictionary *)zone);
  free((void *)zone);
}
