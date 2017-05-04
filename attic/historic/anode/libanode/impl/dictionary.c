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
#include "dictionary.h"

static const char *EMPTY_STR = "";

void AnodeDictionary_clear(struct AnodeDictionary *d)
{
  struct AnodeDictionaryEntry *e,*ne;
  int oldcs;
  unsigned int i;

  oldcs = d->case_sensitive;

  for(i=0;i<ANODE_DICTIONARY_FIXED_HASH_TABLE_SIZE;++i) {
    e = d->ht[i];
    while (e) {
      ne = e->next;
      if ((e->key)&&(e->key != EMPTY_STR)) free((void *)e->key);
      if ((e->value)&&(e->value != EMPTY_STR)) free((void *)e->value);
      free((void *)e);
      e = ne;
    }
  }

  Anode_zero((void *)d,sizeof(struct AnodeDictionary));

  d->case_sensitive = oldcs;
}

void AnodeDictionary_put(struct AnodeDictionary *d,const char *key,const char *value)
{
  struct AnodeDictionaryEntry *e;
  char *p1;
  const char *p2;
  unsigned int bucket = (d->case_sensitive) ? AnodeDictionary__get_bucket(key) : AnodeDictionary__get_bucket_ci(key);
  unsigned int len,i;

  e = d->ht[bucket];
  while (e) {
    if (((d->case_sensitive) ? Anode_streq(key,e->key) : Anode_strcaseeq(key,e->key))) {
      if (!d->case_sensitive) {
        p1 = e->key;
        p2 = key;
        while (*p2) *(p1++) = *(p2++);
      }

      len = 0;
      while (value[len]) ++len;
      if (len) {
        if ((e->value)&&(e->value != EMPTY_STR))
          e->value = (char *)realloc((void *)e->value,len + 1);
        else e->value = (char *)malloc(len + 1);
        for(i=0;i<len;++i) e->value[i] = value[i];
        e->value[i] = (char)0;
      } else {
        if ((e->value)&&(e->value != EMPTY_STR)) free((void *)e->value);
        e->value = (char *)EMPTY_STR;
      }
      return;
    }
    e = e->next;
  }

  e = (struct AnodeDictionaryEntry *)malloc(sizeof(struct AnodeDictionaryEntry));

  len = 0;
  while (key[len]) ++len;
  if (len) {
    e->key = (char *)malloc(len + 1);
    for(i=0;i<len;++i) e->key[i] = key[i];
    e->key[i] = (char)0;
  } else e->key = (char *)EMPTY_STR;

  len = 0;
  while (value[len]) ++len;
  if (len) {
    e->value = (char *)malloc(len + 1);
    for(i=0;i<len;++i) e->value[i] = value[i];
    e->value[i] = (char)0;
  } else e->value = (char *)EMPTY_STR;

  e->next = d->ht[bucket];
  d->ht[bucket] = e;

  ++d->size;
}

void AnodeDictionary_read(
  struct AnodeDictionary *d,
  char *in,
  const char *line_breaks,
  const char *kv_breaks,
  const char *comment_chars,
  char escape_char,
  int trim_whitespace_from_keys,
  int trim_whitespace_from_values)
{
  char *line = in;
  char *key;
  char *value;
  char *p1,*p2,*p3;
  char last = ~escape_char;
  int eof_state = 0;

  for(;;) {
    if ((!*in)||((Anode_strchr(line_breaks,*in))&&((last != escape_char)||(!escape_char)))) {
      if (!*in)
        eof_state = 1;
      else *in = (char)0;

      if ((*line)&&((comment_chars)&&(!Anode_strchr(comment_chars,*line)))) {
        key = line;

        while (*line) {
          if ((Anode_strchr(kv_breaks,*line))&&((last != escape_char)||(!escape_char))) {
            *(line++) = (char)0;
            break;
          } else last = *(line++);
        }
        while ((*line)&&(Anode_strchr(kv_breaks,*line))&&((last != escape_char)||(!escape_char)))
          last = *(line++);
        value = line;

        if (escape_char) {
          p1 = key;
          while (*p1) {
            if (*p1 == escape_char) {
              p2 = p1;
              p3 = p1 + 1;
              while (*p3)
                *(p2++) = *(p3++);
              *p2 = (char)0;
            }
            ++p1;
          }
          p1 = value;
          while (*p1) {
            if (*p1 == escape_char) {
              p2 = p1;
              p3 = p1 + 1;
              while (*p3)
                *(p2++) = *(p3++);
              *p2 = (char)0;
            }
            ++p1;
          }
        }

        if (trim_whitespace_from_keys)
          Anode_trim(key);
        if (trim_whitespace_from_values)
          Anode_trim(value);

        AnodeDictionary_put(d,key,value);
      }

      if (eof_state)
        break;
      else line = in + 1;
    }
    last = *(in++);
  }
}

long AnodeDictionary_write(
  struct AnodeDictionary *d,
  char *out,
  long out_size,
  const char *line_break,
  const char *kv_break)
{
  struct AnodeDictionaryEntry *e;
  const char *tmp;
  long ptr = 0;
  unsigned int bucket;

  if (out_size <= 0)
    return -1;

  for(bucket=0;bucket<ANODE_DICTIONARY_FIXED_HASH_TABLE_SIZE;++bucket) {
    e = d->ht[bucket];
    while (e) {
      tmp = e->key;
      if (tmp) {
        while (*tmp) {
          out[ptr++] = *tmp++;
          if (ptr >= (out_size - 1)) return -1;
        }
      }

      tmp = kv_break;
      if (tmp) {
        while (*tmp) {
          out[ptr++] = *tmp++;
          if (ptr >= (out_size - 1)) return -1;
        }
      }

      tmp = e->value;
      if (tmp) {
        while (*tmp) {
          out[ptr++] = *tmp++;
          if (ptr >= (out_size - 1)) return -1;
        }
      }

      tmp = line_break;
      if (tmp) {
        while (*tmp) {
          out[ptr++] = *tmp++;
          if (ptr >= (out_size - 1)) return -1;
        }
      }

      e = e->next;
    }
  }

  out[ptr] = (char)0;

  return ptr;
}
