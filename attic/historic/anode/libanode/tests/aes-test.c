/* libanode: the Anode C reference implementation
 * Copyright (C) 2009 Adam Ierymenko <adam.ierymenko@gmail.com>
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

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../impl/aes.h"
#include "../anode.h"

static const unsigned char AES_TEST_KEY[32] = {
  0x08,0x09,0x0A,0x0B,0x0D,0x0E,0x0F,0x10,0x12,0x13,0x14,0x15,0x17,0x18,0x19,0x1A,
  0x1C,0x1D,0x1E,0x1F,0x21,0x22,0x23,0x24,0x26,0x27,0x28,0x29,0x2B,0x2C,0x2D,0x2E
};
static const unsigned char AES_TEST_IN[16] = {
  0x06,0x9A,0x00,0x7F,0xC7,0x6A,0x45,0x9F,0x98,0xBA,0xF9,0x17,0xFE,0xDF,0x95,0x21
};
static const unsigned char AES_TEST_OUT[16] = {
  0x08,0x0e,0x95,0x17,0xeb,0x16,0x77,0x71,0x9a,0xcf,0x72,0x80,0x86,0x04,0x0a,0xe3
};

static const unsigned char CMAC_TEST_KEY[32] = {
  0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
  0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4
};

static const unsigned char CMAC_TEST1_OUT[16] = {
  0x02,0x89,0x62,0xf6,0x1b,0x7b,0xf8,0x9e,0xfc,0x6b,0x55,0x1f,0x46,0x67,0xd9,0x83
};

static const unsigned char CMAC_TEST2_IN[16] = {
  0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
};
static const unsigned char CMAC_TEST2_OUT[16] = {
  0x28,0xa7,0x02,0x3f,0x45,0x2e,0x8f,0x82,0xbd,0x4b,0xf2,0x8d,0x8c,0x37,0xc3,0x5c
};

static const unsigned char CMAC_TEST3_IN[40] = {
  0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
  0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51,
  0x30,0xc8,0x1c,0x46,0xa3,0x5c,0xe4,0x11  
};
static const unsigned char CMAC_TEST3_OUT[16] = {
  0xaa,0xf3,0xd8,0xf1,0xde,0x56,0x40,0xc2,0x32,0xf5,0xb1,0x69,0xb9,0xc9,0x11,0xe6
};

static const unsigned char CMAC_TEST4_IN[64] = {
  0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
  0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51,
  0x30,0xc8,0x1c,0x46,0xa3,0x5c,0xe4,0x11,0xe5,0xfb,0xc1,0x19,0x1a,0x0a,0x52,0xef,
  0xf6,0x9f,0x24,0x45,0xdf,0x4f,0x9b,0x17,0xad,0x2b,0x41,0x7b,0xe6,0x6c,0x37,0x10
};
static const unsigned char CMAC_TEST4_OUT[16] = {
  0xe1,0x99,0x21,0x90,0x54,0x9f,0x6e,0xd5,0x69,0x6a,0x2c,0x05,0x6c,0x31,0x54,0x10
};

static void test_cmac(const AnodeAesExpandedKey *expkey,const unsigned char *in,unsigned int inlen,const unsigned char *expected)
{
  unsigned int i;
  unsigned char out[16];

  printf("Testing CMAC with %u byte input:\n",inlen);
  printf("  IN:  ");
  for(i=0;i<inlen;++i)
    printf("%.2x",(int)in[i]);
  printf("\n");
  printf("  EXP: ");
  for(i=0;i<16;++i)
    printf("%.2x",(int)expected[i]);
  printf("\n");
  Anode_cmac_aes256(expkey,in,inlen,out);
  printf("  OUT: ");
  for(i=0;i<16;++i)
    printf("%.2x",(int)out[i]);
  printf("\n");
  if (memcmp(expected,out,16)) {
    printf("FAILED!\n");
    exit(1);
  } else printf("Passed.\n");
}

static void test_cfb(const AnodeAesExpandedKey *expkey,const unsigned char *in,unsigned int inlen,unsigned char *iv,const unsigned char *expected)
{
  unsigned char tmp[131072];
  unsigned char tmp2[131072];
  unsigned char tmpiv[16];

  printf("Testing AES-256 CFB mode with %u bytes: ",inlen);
  fflush(stdout);

  memcpy(tmpiv,iv,16);
  Anode_aes256_cfb_encrypt(expkey,in,tmp,tmpiv,inlen);
  if (!memcmp(tmp,expected,inlen)) {
    printf("FAILED (didn't encrypt)!\n");
    exit(1);
  }
  memcpy(tmpiv,iv,16);
  Anode_aes256_cfb_decrypt(expkey,tmp,tmp2,tmpiv,inlen);
  if (memcmp(tmp2,expected,inlen)) {
    printf("FAILED (didn't encrypt)!\n");
    exit(1);
  } else printf("Passed.\n");
}

static const char *AES_DIGEST_TEST_1 = "test";
static const char *AES_DIGEST_TEST_2 = "supercalifragilisticexpealidocious";
static const char *AES_DIGEST_TEST_3 = "12345678";
static const char *AES_DIGEST_TEST_4 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

int main(int argc,char **argv)
{
  AnodeAesExpandedKey expkey;
  unsigned int i;
  unsigned char aestestbuf[16];
  unsigned char cfbin[131072];
  unsigned char iv[16];

  printf("Testing AES-256:");
  Anode_aes256_expand_key(AES_TEST_KEY,&expkey);
  printf("  IN:  ");
  for(i=0;i<16;++i)
    printf("%.2x",(int)AES_TEST_IN[i]);
  printf("\n");
  printf("  EXP: ");
  for(i=0;i<16;++i)
    printf("%.2x",(int)AES_TEST_OUT[i]);
  printf("\n");
  Anode_aes256_encrypt(&expkey,AES_TEST_IN,aestestbuf);
  printf("  OUT: ");
  for(i=0;i<16;++i)
    printf("%.2x",(int)aestestbuf[i]);
  printf("\n");
  if (memcmp(AES_TEST_OUT,aestestbuf,16)) {
    printf("FAILED!\n");
    return 1;
  } else printf("Passed.\n");
  printf("\n");

  Anode_aes256_expand_key(CMAC_TEST_KEY,&expkey);
  test_cmac(&expkey,(unsigned char *)0,0,CMAC_TEST1_OUT);
  test_cmac(&expkey,CMAC_TEST2_IN,16,CMAC_TEST2_OUT);
  test_cmac(&expkey,CMAC_TEST3_IN,40,CMAC_TEST3_OUT);
  test_cmac(&expkey,CMAC_TEST4_IN,64,CMAC_TEST4_OUT);
  printf("\n");

  for(i=0;i<131072;++i)
    cfbin[i] = (unsigned char)(i & 0xff);
  for(i=0;i<16;++i)
    iv[i] = (unsigned char)(i & 0xff);
  for(i=12345;i<131072;i+=7777)
    test_cfb(&expkey,cfbin,i,iv,cfbin);

  printf("\nTesting AES-DIGEST...\n");
  printf("0 bytes: ");
  Anode_aes_digest(cfbin,0,iv);
  for(i=0;i<16;++i) printf("%.2x",(unsigned int)iv[i]);
  printf("\n");
  printf("%d bytes: ",(int)strlen(AES_DIGEST_TEST_1));
  Anode_aes_digest(AES_DIGEST_TEST_1,strlen(AES_DIGEST_TEST_1),iv);
  for(i=0;i<16;++i) printf("%.2x",(unsigned int)iv[i]);
  printf("\n");
  printf("%d bytes: ",(int)strlen(AES_DIGEST_TEST_2));
  Anode_aes_digest(AES_DIGEST_TEST_2,strlen(AES_DIGEST_TEST_2),iv);
  for(i=0;i<16;++i) printf("%.2x",(unsigned int)iv[i]);
  printf("\n");
  printf("%d bytes: ",(int)strlen(AES_DIGEST_TEST_3));
  Anode_aes_digest(AES_DIGEST_TEST_3,strlen(AES_DIGEST_TEST_3),iv);
  for(i=0;i<16;++i) printf("%.2x",(unsigned int)iv[i]);
  printf("\n");
  printf("%d bytes: ",(int)strlen(AES_DIGEST_TEST_4));
  Anode_aes_digest(AES_DIGEST_TEST_4,strlen(AES_DIGEST_TEST_4),iv);
  for(i=0;i<16;++i) printf("%.2x",(unsigned int)iv[i]);
  printf("\n");

  return 0;
}

