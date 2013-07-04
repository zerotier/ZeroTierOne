/*
    fuzzer.c - Fuzzer test tool for LZ4
    Copyright (C) Andrew Mahone - Yann Collet 2012
	Original code by Andrew Mahone / Modified by Yann Collet
	GPL v2 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

	You can contact the author at :
	- LZ4 homepage : http://fastcompression.blogspot.com/p/lz4.html
	- LZ4 source repository : http://code.google.com/p/lz4/
*/

//**************************************
// Remove Visual warning messages
//**************************************
#define _CRT_SECURE_NO_WARNINGS  // fgets


//**************************************
// Includes
//**************************************
#include <stdlib.h>
#include <stdio.h>      // fgets, sscanf
#include <sys/timeb.h>  // timeb
#include "lz4.h"


//**************************************
// Constants
//**************************************
#define NB_ATTEMPTS (1<<18)
#define LEN ((1<<15))
#define SEQ_POW 2
#define NUM_SEQ (1 << SEQ_POW)
#define SEQ_MSK ((NUM_SEQ) - 1)
#define MOD_SEQ(x) ((((x) >> 8) & 255) == 0)
#define NEW_SEQ(x) ((((x) >> 10) %10) == 0)
#define PAGE_SIZE 4096
#define ROUND_PAGE(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PRIME1   2654435761U
#define PRIME2   2246822519U
#define PRIME3   3266489917U



//*********************************************************
//  Functions
//*********************************************************
static int FUZ_GetMilliStart()
{
  struct timeb tb;
  int nCount;
  ftime( &tb );
  nCount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
  return nCount;
}

static int FUZ_GetMilliSpan( int nTimeStart )
{
  int nSpan = FUZ_GetMilliStart() - nTimeStart;
  if ( nSpan < 0 )
    nSpan += 0x100000 * 1000;
  return nSpan;
}


unsigned int FUZ_rand(unsigned int* src)
{
	*src =  ((*src) * PRIME1) + PRIME2;
	return *src;
}


int test_canary(unsigned char *buf) {
        int i;
        for (i = 0; i < 2048; i++)
                if (buf[i] != buf[i + 2048])
                        return 0;
        return 1;
}

int FUZ_SecurityTest()
{
  char* output;
  char* input;
  int i, r;

  printf("Starting security tests...");
  input = (char*) malloc (20<<20);
  output = (char*) malloc (20<<20);
  input[0] = 0x0F;
  input[1] = 0x00;
  input[2] = 0x00;
  for(i = 3; i < 16840000; i++)
    input[i] = 0xff;
  r = LZ4_uncompress(input, output, 20<<20);

  free(input);
  free(output);
  printf(" Completed (r=%i)\n",r);
  return 0;
}


//int main(int argc, char *argv[]) {
int main() {
        unsigned long long bytes = 0;
        unsigned long long cbytes = 0;
        unsigned char buf[LEN];
        unsigned char testOut[LEN+1];
#       define FUZ_max   LZ4_COMPRESSBOUND(LEN)
#       define FUZ_avail ROUND_PAGE(FUZ_max)
        const int off_full = FUZ_avail - FUZ_max;
        unsigned char cbuf[FUZ_avail + PAGE_SIZE];
		unsigned int seed, cur_seq=PRIME3, seeds[NUM_SEQ], timestamp=FUZ_GetMilliStart();
        int i, j, k, ret, len;
		char userInput[30] = {0};

		printf("starting LZ4 fuzzer\n");
		printf("Select an Initialisation number (default : random) : ");
		fflush(stdout);
		if ( fgets(userInput, sizeof userInput, stdin) )
		{
			if ( sscanf(userInput, "%d", &seed) == 1 ) {}
			else seed = FUZ_GetMilliSpan(timestamp);
		}
		printf("Seed = %u\n", seed);

		FUZ_SecurityTest();

		for (i = 0; i < 2048; i++)
                cbuf[FUZ_avail + i] = cbuf[FUZ_avail + 2048 + i] = FUZ_rand(&seed) >> 16;

        for (i = 0; i < NB_ATTEMPTS; i++) 
        {
			printf("\r%7i /%7i\r", i, NB_ATTEMPTS);
			
			FUZ_rand(&seed);
            for (j = 0; j < NUM_SEQ; j++) {
                    seeds[j] = FUZ_rand(&seed) << 8;
                    seeds[j] ^= (FUZ_rand(&seed) >> 8) & 65535;
            }
            for (j = 0; j < LEN; j++) {
                    k = FUZ_rand(&seed);
                    if (j == 0 || NEW_SEQ(k))
                            cur_seq = seeds[(FUZ_rand(&seed) >> 16) & SEQ_MSK];
                    if (MOD_SEQ(k)) {
                            k = (FUZ_rand(&seed) >> 16) & SEQ_MSK;
                            seeds[k] = FUZ_rand(&seed) << 8;
                            seeds[k] ^= (FUZ_rand(&seed) >> 8) & 65535;
                    }
                    buf[j] = FUZ_rand(&cur_seq) >> 16;
            }

			// Test compression
            ret = LZ4_compress_limitedOutput((const char*)buf, (char*)&cbuf[off_full], LEN, FUZ_max);
			if (ret == 0) { printf("compression failed despite sufficient space: seed %u, len %d\n", seed, LEN); goto _output_error; }
            len = ret;

			// Test decoding with output size being exactly what's necessary => must work
			ret = LZ4_uncompress((char*)&cbuf[off_full], (char*)testOut, LEN);
			if (ret<0) { printf("decompression failed despite correct space: seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test decoding with one byte missing => must fail
			ret = LZ4_uncompress((char*)&cbuf[off_full], (char*)testOut, LEN-1);
			if (ret>=0) { printf("decompression should have failed, due to Output Size being too small : seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test decoding with one byte too much => must fail
			ret = LZ4_uncompress((char*)&cbuf[off_full], (char*)testOut, LEN+1);
			if (ret>=0) { printf("decompression should have failed, due to Output Size being too large : seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test decoding with enough output size => must work
			ret = LZ4_uncompress_unknownOutputSize((char*)&cbuf[off_full], (char*)testOut, len, LEN+1);
			if (ret<0) { printf("decompression failed despite sufficient space: seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test decoding with output size being exactly what's necessary => must work
			ret = LZ4_uncompress_unknownOutputSize((char*)&cbuf[off_full], (char*)testOut, len, LEN);
			if (ret<0) { printf("decompression failed despite sufficient space: seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test decoding with output size being one byte too short => must fail
			ret = LZ4_uncompress_unknownOutputSize((char*)&cbuf[off_full], (char*)testOut, len, LEN-1);
			if (ret>=0) { printf("decompression should have failed, due to Output Size being too small : seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test decoding with input size being one byte too short => must fail
			ret = LZ4_uncompress_unknownOutputSize((char*)&cbuf[off_full], (char*)testOut, len-1, LEN);
			if (ret>=0) { printf("decompression should have failed, due to input size being too small : seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test decoding with input size being one byte too large => must fail
			ret = LZ4_uncompress_unknownOutputSize((char*)&cbuf[off_full], (char*)testOut, len+1, LEN);
			if (ret>=0) { printf("decompression should have failed, due to input size being too large : seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test compression with output size being exactly what's necessary (should work)
            ret = LZ4_compress_limitedOutput((const char*)buf, (char*)&cbuf[FUZ_avail-len], LEN, len);
            if (!test_canary(&cbuf[FUZ_avail])) { printf("compression overran output buffer: seed %u, len %d, olen %d\n", seed, LEN, len); goto _output_error; }
            if (ret == 0) { printf("compression failed despite sufficient space: seed %u, len %d\n", seed, LEN); goto _output_error; }

			// Test compression with just one missing byte into output buffer => must fail
            ret = LZ4_compress_limitedOutput((const char*)buf, (char*)&cbuf[FUZ_avail-(len-1)], LEN, len-1);
            if (ret) { printf("compression overran output buffer: seed %u, len %d, olen %d => ret %d", seed, LEN, len-1, ret); goto _output_error; }
            if (!test_canary(&cbuf[FUZ_avail])) { printf("compression overran output buffer: seed %u, len %d, olen %d", seed, LEN, len-1); goto _output_error; }

			bytes += LEN;
            cbytes += len;
        }

		printf("all tests completed successfully \n");
        printf("compression ratio: %0.3f%%\n", (double)cbytes/bytes*100);
		getchar();
        return 0;

_output_error:
		getchar();
		return 1;
}
