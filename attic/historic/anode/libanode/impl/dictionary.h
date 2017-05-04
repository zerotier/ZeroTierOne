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

/* This is a simple string hash table suitable for small tables such as zone
 * files or HTTP header lists. */

#ifndef _ANODE_DICTIONARY_H
#define _ANODE_DICTIONARY_H

#include "misc.h"

/* This is a fixed hash table and is designed for relatively small numbers
 * of keys for things like zone files. */
#define ANODE_DICTIONARY_FIXED_HASH_TABLE_SIZE 16
#define ANODE_DICTIONARY_FIXED_HASH_TABLE_MASK 15

/* Computes a hash code for a string and returns the hash bucket */
static inline unsigned int AnodeDictionary__get_bucket(const char *s)
{
  unsigned int hc = 3;
  while (*s)
    hc = ((hc << 4) + hc) + (unsigned int)*(s++);
  return ((hc ^ (hc >> 4)) & ANODE_DICTIONARY_FIXED_HASH_TABLE_MASK);
}
/* Case insensitive version of get_bucket */
static inline unsigned int AnodeDictionary__get_bucket_ci(const char *s)
{
  unsigned int hc = 3;
  while (*s)
    hc = ((hc << 4) + hc) + (unsigned int)Anode_tolower(*(s++));
  return ((hc ^ (hc >> 4)) & ANODE_DICTIONARY_FIXED_HASH_TABLE_MASK);
}

struct AnodeDictionaryEntry
{
  char *key;
  char *value;
  struct AnodeDictionaryEntry *next;
};

struct AnodeDictionary
{
  struct AnodeDictionaryEntry *ht[ANODE_DICTIONARY_FIXED_HASH_TABLE_SIZE];
  unsigned int size;
  int case_sensitive;
};

static inline void AnodeDictionary_init(struct AnodeDictionary *d,int case_sensitive)
{
  Anode_zero((void *)d,sizeof(struct AnodeDictionary));
  d->case_sensitive = case_sensitive;
}

void AnodeDictionary_clear(struct AnodeDictionary *d);

static inline void AnodeDictionary_destroy(struct AnodeDictionary *d)
{
  AnodeDictionary_clear(d);
}

void AnodeDictionary_put(struct AnodeDictionary *d,const char *key,const char *value);

static inline const char *AnodeDictionary_get(struct AnodeDictionary *d,const char *key)
{
  struct AnodeDictionaryEntry *e;
  unsigned int bucket = (d->case_sensitive) ? AnodeDictionary__get_bucket(key) : AnodeDictionary__get_bucket_ci(key);

  e = d->ht[bucket];
  while (e) {
    if ((d->case_sensitive ? Anode_streq(key,e->key) : Anode_strcaseeq(key,e->key)))
      return e->value;
    e = e->next;
  }

  return (const char *)0;
}

static inline void AnodeDictionary_iterate(
  struct AnodeDictionary *d,
  void *arg,
  int (*func)(void *,const char *,const char *))
{
  struct AnodeDictionaryEntry *e;
  unsigned int bucket;

  for(bucket=0;bucket<ANODE_DICTIONARY_FIXED_HASH_TABLE_SIZE;++bucket) {
    e = d->ht[bucket];
    while (e) {
      if (!func(arg,e->key,e->value))
        return;
      e = e->next;
    }
  }
}

void AnodeDictionary_read(
  struct AnodeDictionary *d,
  char *in,
  const char *line_breaks,
  const char *kv_breaks,
  const char *comment_chars,
  char escape_char,
  int trim_whitespace_from_keys,
  int trim_whitespace_from_values);

long AnodeDictionary_write(
  struct AnodeDictionary *d,
  char *out,
  long out_size,
  const char *line_break,
  const char *kv_break);

#endif
