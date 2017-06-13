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
#include <openssl/bn.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include "types.h"
#include "misc.h"
#include "ec.h"

static EC_GROUP *AnodeEC_group = (EC_GROUP *)0;

static void *AnodeEC_KDF(const void *in,size_t inlen,void *out,size_t *outlen)
{
  unsigned long i,longest_length;

  if (!*outlen)
    return out;

  for(i=0;i<(unsigned long)*outlen;++i)
    ((unsigned char *)out)[i] = (unsigned char)0;

  longest_length = inlen;
  if (longest_length < *outlen)
    longest_length = *outlen;
  for(i=0;i<longest_length;++i)
    ((unsigned char *)out)[i % (unsigned long)*outlen] ^= ((const unsigned char *)in)[i % (unsigned long)inlen];

  return out;
}

int AnodeECKeyPair_generate(struct AnodeECKeyPair *pair)
{
  EC_KEY *key;
  int len;

#ifdef HAS_DEV_URANDOM
  char buf[128];
  FILE *f = fopen("/dev/urandom","r");
  if (f) {
    if (fread(buf,1,sizeof(buf),f) == sizeof(buf))
      RAND_add(buf,sizeof(buf),sizeof(buf)/2);
    fclose(f);
  }
#endif

  if (!AnodeEC_group) {
    AnodeEC_group = EC_GROUP_new_by_curve_name(ANODE_EC_GROUP);
    if (!AnodeEC_group) return 0;
  }

  key = EC_KEY_new();
  if (!key) return 0;

  if (!EC_KEY_set_group(key,AnodeEC_group)) {
    EC_KEY_free(key);
    return 0;
  }

  if (!EC_KEY_generate_key(key)) {
    EC_KEY_free(key);
    return 0;
  }

  Anode_zero(pair,sizeof(struct AnodeECKeyPair));

  /* Stuff the private key into priv.key */
  len = BN_num_bytes(EC_KEY_get0_private_key(key));
  if ((len > ANODE_EC_PRIME_BYTES)||(len < 0)) {
    EC_KEY_free(key);
    return 0;
  }
  BN_bn2bin(EC_KEY_get0_private_key(key),&(pair->priv.key[ANODE_EC_PRIME_BYTES - len]));
  pair->priv.bytes = ANODE_EC_PRIME_BYTES;

  len = EC_POINT_point2oct(AnodeEC_group,EC_KEY_get0_public_key(key),POINT_CONVERSION_COMPRESSED,pair->pub.key,sizeof(pair->pub.key),0);
  if (len != ANODE_EC_PUBLIC_KEY_BYTES) {
    EC_KEY_free(key);
    return 0;
  }
  pair->pub.bytes = ANODE_EC_PUBLIC_KEY_BYTES;

  /* Keep a copy of OpenSSL's structure around so we don't have to re-init
   * it every time we use our key pair structure. */
  pair->internal_key = key;

  return 1;
}

int AnodeECKeyPair_init(struct AnodeECKeyPair *pair,const struct AnodeECKey *pub,const struct AnodeECKey *priv)
{
  EC_KEY *key;
  EC_POINT *kxy;
  BIGNUM *pn;

  if (!AnodeEC_group) {
    AnodeEC_group = EC_GROUP_new_by_curve_name(ANODE_EC_GROUP);
    if (!AnodeEC_group) return 0;
  }

  key = EC_KEY_new();
  if (!key)
    return 0;

  if (!EC_KEY_set_group(key,AnodeEC_group)) {
    EC_KEY_free(key);
    return 0;
  }

  /* Grab the private key */
  if (priv->bytes != ANODE_EC_PRIME_BYTES) {
    EC_KEY_free(key);
    return 0;
  }
  pn = BN_new();
  if (!pn) {
    EC_KEY_free(key);
    return 0;
  }
  if (!BN_bin2bn(priv->key,ANODE_EC_PRIME_BYTES,pn)) {
    BN_free(pn);
    EC_KEY_free(key);
    return 0;
  }
  if (!EC_KEY_set_private_key(key,pn)) {
    BN_free(pn);
    EC_KEY_free(key);
    return 0;
  }
  BN_free(pn);

  /* Set the public key */
  if (pub->bytes != ANODE_EC_PUBLIC_KEY_BYTES) {
    EC_KEY_free(key);
    return 0;
  }
  kxy = EC_POINT_new(AnodeEC_group);
  if (!kxy) {
    EC_KEY_free(key);
    return 0;
  }
  EC_POINT_oct2point(AnodeEC_group,kxy,pub->key,ANODE_EC_PUBLIC_KEY_BYTES,0);
  if (!EC_KEY_set_public_key(key,kxy)) {
    EC_POINT_free(kxy);
    EC_KEY_free(key);
    return 0;
  }
  EC_POINT_free(kxy);

  Anode_zero(pair,sizeof(struct AnodeECKeyPair));
  Anode_memcpy((void *)&(pair->pub),(const void *)pub,sizeof(struct AnodeECKey));
  Anode_memcpy((void *)&(pair->priv),(const void *)priv,sizeof(struct AnodeECKey));
  pair->internal_key = key;

  return 1;
}

void AnodeECKeyPair_destroy(struct AnodeECKeyPair *pair)
{
  if (pair) {
    if (pair->internal_key)
      EC_KEY_free((EC_KEY *)pair->internal_key);
  }
}

int AnodeECKeyPair_agree(const struct AnodeECKeyPair *my_key_pair,const struct AnodeECKey *their_pub_key,unsigned char *key_buf,unsigned int key_len)
{
  EC_POINT *pub;
  int i;

  if (!AnodeEC_group) {
    AnodeEC_group = EC_GROUP_new_by_curve_name(ANODE_EC_GROUP);
    if (!AnodeEC_group) return 0;
  }

  if (!my_key_pair->internal_key)
    return 0;

  if (their_pub_key->bytes != ANODE_EC_PUBLIC_KEY_BYTES)
    return 0;
  pub = EC_POINT_new(AnodeEC_group);
  if (!pub)
    return 0;
  EC_POINT_oct2point(AnodeEC_group,pub,their_pub_key->key,ANODE_EC_PUBLIC_KEY_BYTES,0);

  i = ECDH_compute_key(key_buf,key_len,pub,(EC_KEY *)my_key_pair->internal_key,&AnodeEC_KDF);
  if (i != (int)key_len) {
    EC_POINT_free(pub);
    return 0;
  }

  EC_POINT_free(pub);

  return 1;
}

void AnodeEC_random(unsigned char *buf,unsigned int len)
{
  RAND_pseudo_bytes(buf,len);
}
