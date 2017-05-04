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

/* This contains miscellaneous functions, including some re-implementations
 * of some functions from string.h. This is to help us port to some platforms
 * (cough Windows Mobile cough) that lack a lot of the basic C library. */

#ifndef _ANODE_MISC_H
#define _ANODE_MISC_H

#include <time.h>
#include <sys/time.h>
#include "types.h"

#ifndef ANODE_NO_STRING_H
#include <string.h>
#include <stdlib.h>
#endif

/* Table mapping ASCII characters to themselves or their lower case */
extern const unsigned char Anode_ascii_tolower_table[256];

/* Get the lower case version of an ASCII char */
#define Anode_tolower(c) ((char)Anode_ascii_tolower_table[((unsigned long)((unsigned char)(c)))])

/* Test strings for equality, return nonzero if equal */
static inline unsigned int Anode_streq(const char *restrict a,const char *restrict b)
{
  if ((!a)||(!b))
    return 0;
  while (*a == *(b++)) {
    if (!*(a++))
      return 1;
  }
  return 0;
}

/* Equality test ignoring (ASCII) case */
static inline unsigned int Anode_strcaseeq(const char *restrict a,const char *restrict b)
{
  if ((!a)||(!b))
    return 0;
  while (Anode_tolower(*a) == Anode_tolower(*(b++))) {
    if (!*(a++))
      return 1;
  }
  return 0;
}

/* Safe c-string copy, ensuring that dest[] always ends with zero */
static inline void Anode_str_copy(char *restrict dest,const char *restrict src,unsigned int dest_size)
{
  char *restrict dest_end = dest + (dest_size - 1);
  while ((*src)&&(dest != dest_end))
    *(dest++) = *(src++);
  *dest = (char)0;
}

/* Simple memcpy() */
#ifdef ANODE_NO_STRING_H
static inline void Anode_memcpy(void *restrict dest,const void *restrict src,unsigned int len)
{
  unsigned int i;
  for(i=0;i<len;++i)
    ((unsigned char *restrict)dest)[i] = ((const unsigned char *restrict)src)[i];
}
#else
#define Anode_memcpy(d,s,l) memcpy((d),(s),(l))
#endif

/* Memory test for equality */
#ifdef ANODE_NO_STRING_H
static inline unsigned int Anode_mem_eq(const void *restrict a,const void *restrict b,unsigned int len)
{
  unsigned int i;
  for(i=0;i<len;++i) {
    if (((const unsigned char *restrict)a)[i] != ((const unsigned char *restrict)b)[i])
      return 0;
  }
  return 1;
}
#else
#define Anode_mem_eq(a,b,l) (!memcmp((a),(b),(l)))
#endif

/* Zero memory */
#ifdef ANODE_NO_STRING_H
static inline void Anode_zero(void *restrict ptr,unsigned int len)
{
  unsigned int i;
  for(i=0;i<len;++i)
    ((unsigned char *restrict)ptr)[i] = (unsigned char)0;
}
#else
#define Anode_zero(p,l) memset((p),0,(l))
#endif

/* Get a pointer to the first occurrance of a character in a string */
#ifdef ANODE_NO_STRING_H
static inline const char *Anode_strchr(const char *s,char c)
{
  while (*s) {
    if (*s == c)
      return s;
    ++s;
  }
  return (char *)0;
}
#else
#define Anode_strchr(s,c) strchr((s),(c))
#endif

static inline unsigned int Anode_count_char(const char *s,char c)
{
  unsigned int cnt = 0;
  while (s) {
    if (*s == c)
      ++cnt;
    ++s;
  }
  return cnt;
}

/* Strip all of a given set of characters from a string */
static inline void Anode_strip_all(char *s,const char *restrict schars)
{
  char *d = s;

  while (*s) {
    if (!Anode_strchr(schars,*s))
      *(d++) = *s;
    ++s;
  }
  *d = (char)0;
}

/* Trim whitespace from beginning and end of string */
void Anode_trim(char *s);

/* Get the length of a string */
#ifdef ANODE_NO_STRING_H
static inline unsigned int Anode_strlen(const char *s)
{
  const char *ptr = s;
  while (*ptr) ++ptr;
  return (unsigned int)(ptr - s);
}
#else
#define Anode_strlen(s) strlen((s))
#endif

/* Returns number of milliseconds since the epoch (Java-style) */
static inline uint64_t Anode_time64()
{
  struct timeval tv;
  gettimeofday(&tv,(void *)0);
  return ( (((uint64_t)tv.tv_sec) / 1000ULL) + ((uint64_t)(tv.tv_usec / 1000ULL)) );
}

/* Returns number of seconds since the epoch (*nix style) */
static inline unsigned long Anode_time()
{
  struct timeval tv;
  gettimeofday(&tv,(void *)0);
  return (unsigned long)tv.tv_sec;
}

/* Simple random function, not cryptographically safe */
unsigned int Anode_rand();

/* Fast hex/ascii conversion */
void Anode_to_hex(const unsigned char *b,unsigned int len,char *h,unsigned int hlen);
void Anode_from_hex(const char *h,unsigned char *b,unsigned int blen);

/* Convert back and forth from base32 encoding */
/* 5 bytes -> 8 base32 characters and vice versa */
void Anode_base32_5_to_8(const unsigned char *in,char *out);
void Anode_base32_8_to_5(const char *in,unsigned char *out);

#endif
