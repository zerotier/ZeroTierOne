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
#include <string.h>
#include "misc.h"
#include "types.h"

static const char Anode_hex_chars[16] = {
  '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};

static const char Anode_base32_chars[32] = {
  'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q',
  'r','s','t','u','v','w','x','y','z','2','3','4','5','6','7'
};
static const unsigned char Anode_base32_bits[256] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,26,27,28,29,30,31,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,
  6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,0,0,1,2,
  3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* Table for converting ASCII chars to lower case */
const unsigned char Anode_ascii_tolower_table[256] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
  0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
  0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
  0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
  0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
  0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
  0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
  0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
  0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

void Anode_trim(char *s)
{
  char *dest = s;
  char *last;
  while ((*s)&&((*s == ' ')||(*s == '\t')||(*s == '\r')||(*s == '\n')))
    ++s;
  last = s;
  while ((*dest = *s)) {
    if ((*dest != ' ')&&(*dest != '\t')&&(*dest != '\r')&&(*dest != '\n'))
      last = dest;
    ++dest;
    ++s;
  }
  if (*last)
    *(++last) = (char)0;
}

unsigned int Anode_rand()
{
  static volatile int need_seed = 1;

  if (need_seed) {
    need_seed = 0;
    srandom((unsigned long)Anode_time64());
  }

  return (unsigned int)random();
}

void Anode_to_hex(const unsigned char *b,unsigned int len,char *h,unsigned int hlen)
{
  unsigned int i;

  if ((len * 2) >= hlen)
    len = (hlen - 1) / 2;

  for(i=0;i<len;++i) {
    *(h++) = Anode_hex_chars[b[i] >> 4];
    *(h++) = Anode_hex_chars[b[i] & 0xf];
  }
  *h = (char)0;
}

void Anode_from_hex(const char *h,unsigned char *b,unsigned int blen)
{
  unsigned char *end = b + blen;
  unsigned char v = (unsigned char)0;

  while (b != end) {
    switch(*(h++)) {
      case '0': v = 0x00; break;
      case '1': v = 0x10; break;
      case '2': v = 0x20; break;
      case '3': v = 0x30; break;
      case '4': v = 0x40; break;
      case '5': v = 0x50; break;
      case '6': v = 0x60; break;
      case '7': v = 0x70; break;
      case '8': v = 0x80; break;
      case '9': v = 0x90; break;
      case 'a': v = 0xa0; break;
      case 'b': v = 0xb0; break;
      case 'c': v = 0xc0; break;
      case 'd': v = 0xd0; break;
      case 'e': v = 0xe0; break;
      case 'f': v = 0xf0; break;
      default: return;
    }

    switch(*(h++)) {
      case '0': v |= 0x00; break;
      case '1': v |= 0x01; break;
      case '2': v |= 0x02; break;
      case '3': v |= 0x03; break;
      case '4': v |= 0x04; break;
      case '5': v |= 0x05; break;
      case '6': v |= 0x06; break;
      case '7': v |= 0x07; break;
      case '8': v |= 0x08; break;
      case '9': v |= 0x09; break;
      case 'a': v |= 0x0a; break;
      case 'b': v |= 0x0b; break;
      case 'c': v |= 0x0c; break;
      case 'd': v |= 0x0d; break;
      case 'e': v |= 0x0e; break;
      case 'f': v |= 0x0f; break;
      default: return;
    }

    *(b++) = v;
  }
}

void Anode_base32_5_to_8(const unsigned char *in,char *out)
{
  out[0] = Anode_base32_chars[(in[0]) >> 3];
  out[1] = Anode_base32_chars[(in[0] & 0x07) << 2 | (in[1] & 0xc0) >> 6];
  out[2] = Anode_base32_chars[(in[1] & 0x3e) >> 1];
  out[3] = Anode_base32_chars[(in[1] & 0x01) << 4 | (in[2] & 0xf0) >> 4];
  out[4] = Anode_base32_chars[(in[2] & 0x0f) << 1 | (in[3] & 0x80) >> 7];
  out[5] = Anode_base32_chars[(in[3] & 0x7c) >> 2];
  out[6] = Anode_base32_chars[(in[3] & 0x03) << 3 | (in[4] & 0xe0) >> 5];
  out[7] = Anode_base32_chars[(in[4] & 0x1f)];
}

void Anode_base32_8_to_5(const char *in,unsigned char *out)
{
  out[0] = ((Anode_base32_bits[(unsigned int)in[0]]) << 3) | (Anode_base32_bits[(unsigned int)in[1]] & 0x1C) >> 2;
  out[1] = ((Anode_base32_bits[(unsigned int)in[1]] & 0x03) << 6) | (Anode_base32_bits[(unsigned int)in[2]]) << 1 | (Anode_base32_bits[(unsigned int)in[3]] & 0x10) >> 4;
  out[2] = ((Anode_base32_bits[(unsigned int)in[3]] & 0x0F) << 4) | (Anode_base32_bits[(unsigned int)in[4]] & 0x1E) >> 1;
  out[3] = ((Anode_base32_bits[(unsigned int)in[4]] & 0x01) << 7) | (Anode_base32_bits[(unsigned int)in[5]]) << 2 | (Anode_base32_bits[(unsigned int)in[6]] & 0x18) >> 3;
  out[4] = ((Anode_base32_bits[(unsigned int)in[6]] & 0x07) << 5) | (Anode_base32_bits[(unsigned int)in[7]]);
}
