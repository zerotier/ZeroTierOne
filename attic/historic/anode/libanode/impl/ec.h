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

/* Elliptic curve glue -- hides OpenSSL code behind this source module */

#ifndef _ANODE_EC_H
#define _ANODE_EC_H

#include "misc.h"

/* Right now, only one mode is supported: NIST-P-256. This is the only mode
 * supported in the spec as well, and should be good for quite some time.
 * If other modes are needed this code will need to be refactored. */

/* NIST-P-256 prime size in bytes */
#define ANODE_EC_PRIME_BYTES 32

/* Sizes of key fields */
#define ANODE_EC_GROUP NID_X9_62_prime256v1
#define ANODE_EC_PUBLIC_KEY_BYTES (ANODE_EC_PRIME_BYTES + 1)
#define ANODE_EC_PRIVATE_KEY_BYTES ANODE_EC_PRIME_BYTES

/* Larger of public or private key bytes, used for buffers */
#define ANODE_EC_MAX_BYTES ANODE_EC_PUBLIC_KEY_BYTES

struct AnodeECKey
{
  unsigned char key[ANODE_EC_MAX_BYTES];
  unsigned int bytes;
};

struct AnodeECKeyPair
{
  struct AnodeECKey pub;
  struct AnodeECKey priv;
  void *internal_key;
};

/* Key management functions */
int AnodeECKeyPair_generate(struct AnodeECKeyPair *pair);
int AnodeECKeyPair_init(struct AnodeECKeyPair *pair,const struct AnodeECKey *pub,const struct AnodeECKey *priv);
void AnodeECKeyPair_destroy(struct AnodeECKeyPair *pair);
int AnodeECKeyPair_agree(const struct AnodeECKeyPair *my_key_pair,const struct AnodeECKey *their_pub_key,unsigned char *key_buf,unsigned int key_len);

/* Provides access to the secure PRNG used to generate keys */
void AnodeEC_random(unsigned char *buf,unsigned int len);

#endif
