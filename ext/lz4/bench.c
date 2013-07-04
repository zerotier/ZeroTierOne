/*
    bench.c - Demo program to benchmark open-source compression algorithm
    Copyright (C) Yann Collet 2012
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
// Compiler Options
//**************************************
// Disable some Visual warning messages
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE     // VS2005

// Unix Large Files support (>4GB)
#if (defined(__sun__) && (!defined(__LP64__)))   // Sun Solaris 32-bits requires specific definitions
#  define _LARGEFILE_SOURCE 
#  define FILE_OFFSET_BITS=64
#elif ! defined(__LP64__)                        // No point defining Large file for 64 bit
#  define _LARGEFILE64_SOURCE
#endif

// S_ISREG & gettimeofday() are not supported by MSVC
#if defined(_MSC_VER)
#  define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#  define BMK_LEGACY_TIMER 1
#endif

// GCC does not support _rotl outside of Windows
#if !defined(_WIN32)
#  define _rotl(x,r) ((x << r) | (x >> (32 - r)))
#endif


//**************************************
// Includes
//**************************************
#include <stdlib.h>      // malloc
#include <stdio.h>       // fprintf, fopen, ftello64
#include <sys/types.h>   // stat64
#include <sys/stat.h>    // stat64

// Use ftime() if gettimeofday() is not available on your target
#if defined(BMK_LEGACY_TIMER)   
#  include <sys/timeb.h>   // timeb, ftime
#else
#  include <sys/time.h>    // gettimeofday
#endif

#include "lz4.h"
#define COMPRESSOR0 LZ4_compress
#include "lz4hc.h"
#define COMPRESSOR1 LZ4_compressHC
#define DEFAULTCOMPRESSOR LZ4_compress



//**************************************
// Basic Types
//**************************************
#if defined(_MSC_VER)    // Visual Studio does not support 'stdint' natively
#define BYTE	unsigned __int8
#define U16		unsigned __int16
#define U32		unsigned __int32
#define S32		__int32
#define U64		unsigned __int64
#else
#include <stdint.h>
#define BYTE	uint8_t
#define U16		uint16_t
#define U32		uint32_t
#define S32		int32_t
#define U64		uint64_t
#endif


//**************************************
// Constants
//**************************************
#define NBLOOPS		3
#define TIMELOOP	2000

#define KNUTH		2654435761U
#define MAX_MEM		(1984<<20)
#define DEFAULT_CHUNKSIZE   (8<<20)


//**************************************
// Local structures
//**************************************
struct chunkParameters
{
	U32   id;
	char* inputBuffer;
	char* outputBuffer;
	int   inputSize;
	int   outputSize;
};

struct compressionParameters
{
	int (*compressionFunction)(const char*, char*, int);
	int (*decompressionFunction)(const char*, char*, int);
};


//**************************************
// MACRO
//**************************************
#define DISPLAY(...) fprintf(stderr, __VA_ARGS__)



//**************************************
// Benchmark Parameters
//**************************************
static int chunkSize = DEFAULT_CHUNKSIZE;
static int nbIterations = NBLOOPS;
static int BMK_pause = 0;

void BMK_SetBlocksize(int bsize)
{
	chunkSize = bsize;
	DISPLAY("-Using Block Size of %i KB-", chunkSize>>10);
}

void BMK_SetNbIterations(int nbLoops)
{
	nbIterations = nbLoops;
	DISPLAY("- %i iterations-", nbIterations);
}

void BMK_SetPause()
{
    BMK_pause = 1;
}

//*********************************************************
//  Private functions
//*********************************************************

#if defined(BMK_LEGACY_TIMER)

static int BMK_GetMilliStart()
{
  // Based on Legacy ftime()
  // Rolls over every ~ 12.1 days (0x100000/24/60/60)
  // Use GetMilliSpan to correct for rollover
  struct timeb tb;
  int nCount;
  ftime( &tb );
  nCount = (int) (tb.millitm + (tb.time & 0xfffff) * 1000);
  return nCount;
}

#else

static int BMK_GetMilliStart()
{
  // Based on newer gettimeofday()
  // Use GetMilliSpan to correct for rollover
  struct timeval tv;
  int nCount;
  gettimeofday(&tv, NULL);
  nCount = (int) (tv.tv_usec/1000 + (tv.tv_sec & 0xfffff) * 1000);
  return nCount;
}

#endif


static int BMK_GetMilliSpan( int nTimeStart )
{
  int nSpan = BMK_GetMilliStart() - nTimeStart;
  if ( nSpan < 0 )
    nSpan += 0x100000 * 1000;
  return nSpan;
}


static U32 BMK_checksum_MMH3A (char* buff, U32 length)
{
  const BYTE* data = (const BYTE*)buff;
  const int nblocks = length >> 2;

  U32 h1 = KNUTH;
  U32 c1 = 0xcc9e2d51;
  U32 c2 = 0x1b873593;

  const U32* blocks = (const U32*)(data + nblocks*4);
  int i;

  for(i = -nblocks; i; i++)
  {
    U32 k1 = blocks[i];

    k1 *= c1;
    k1 = _rotl(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = _rotl(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  {
	  const BYTE* tail = (const BYTE*)(data + nblocks*4);
	  U32 k1 = 0;

	  switch(length & 3)
	  {
	  case 3: k1 ^= tail[2] << 16;
	  case 2: k1 ^= tail[1] << 8;
	  case 1: k1 ^= tail[0];
			  k1 *= c1; k1 = _rotl(k1,15); k1 *= c2; h1 ^= k1;
	  };
  }

  h1 ^= length;
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  return h1;
}


static size_t BMK_findMaxMem(U64 requiredMem)
{
	size_t step = (64U<<20);   // 64 MB
	BYTE* testmem=NULL;

	requiredMem = (((requiredMem >> 25) + 1) << 26);
	if (requiredMem > MAX_MEM) requiredMem = MAX_MEM;

	requiredMem += 2*step;
	while (!testmem)
	{
		requiredMem -= step;
		testmem = malloc ((size_t)requiredMem);
	}

	free (testmem);
	return (size_t) (requiredMem - step);
}


static U64 BMK_GetFileSize(char* infilename)
{
	int r;
#if defined(_MSC_VER)
	struct _stat64 statbuf;
	r = _stat64(infilename, &statbuf);
#else
	struct stat statbuf;
	r = stat(infilename, &statbuf);
#endif
	if (r || !S_ISREG(statbuf.st_mode)) return 0;   // No good...
	return (U64)statbuf.st_size;
}


//*********************************************************
//  Public function
//*********************************************************

int BMK_benchFile(char** fileNamesTable, int nbFiles, int cLevel)
{
  int fileIdx=0;
  FILE* fileIn;
  char* infilename;
  U64 largefilesize;
  size_t benchedsize;
  int nbChunks;
  int maxCChunkSize;
  size_t readSize;
  char* in_buff;
  char* out_buff; int out_buff_size;
  struct chunkParameters* chunkP;
  U32 crcc, crcd=0;
  struct compressionParameters compP;

  U64 totals = 0;
  U64 totalz = 0;
  double totalc = 0.;
  double totald = 0.;


  // Init
  switch (cLevel)
  {
#ifdef COMPRESSOR0
  case 0 : compP.compressionFunction = COMPRESSOR0; break;
#endif
#ifdef COMPRESSOR1
  case 1 : compP.compressionFunction = COMPRESSOR1; break;
#endif
  default : compP.compressionFunction = DEFAULTCOMPRESSOR;
  }
  compP.decompressionFunction = LZ4_uncompress;

  // Loop for each file
  while (fileIdx<nbFiles)
  {
	  // Check file existence
	  infilename = fileNamesTable[fileIdx++];
	  fileIn = fopen( infilename, "rb" );
	  if (fileIn==NULL)
	  {
		DISPLAY( "Pb opening %s\n", infilename);
		return 11;
	  }

	  // Memory allocation & restrictions
	  largefilesize = BMK_GetFileSize(infilename);
	  benchedsize = (size_t) BMK_findMaxMem(largefilesize) / 2;
	  if ((U64)benchedsize > largefilesize) benchedsize = (size_t)largefilesize;
	  if (benchedsize < largefilesize)
	  {
		  DISPLAY("Not enough memory for '%s' full size; testing %i MB only...\n", infilename, (int)(benchedsize>>20));
	  }

	  // Alloc
	  chunkP = (struct chunkParameters*) malloc(((benchedsize / chunkSize)+1) * sizeof(struct chunkParameters));
	  in_buff = malloc((size_t )benchedsize);
	  nbChunks = (int) (benchedsize / chunkSize) + 1;
	  maxCChunkSize = LZ4_compressBound(chunkSize);
	  out_buff_size = nbChunks * maxCChunkSize;
	  out_buff = malloc((size_t )out_buff_size);


	  if(!in_buff || !out_buff)
	  {
		DISPLAY("\nError: not enough memory!\n");
		free(in_buff);
		free(out_buff);
		fclose(fileIn);
		return 12;
	  }

	  // Init chunks data
	  {
		  int i;
		  size_t remaining = benchedsize;
		  char* in = in_buff;
		  char* out = out_buff;
		  for (i=0; i<nbChunks; i++)
		  {
			  chunkP[i].id = i;
			  chunkP[i].inputBuffer = in; in += chunkSize;
			  if ((int)remaining > chunkSize) { chunkP[i].inputSize = chunkSize; remaining -= chunkSize; } else { chunkP[i].inputSize = (int)remaining; remaining = 0; }
			  chunkP[i].outputBuffer = out; out += maxCChunkSize;
			  chunkP[i].outputSize = 0;
		  }
	  }

	  // Fill input buffer
	  DISPLAY("Loading %s...       \r", infilename);
	  readSize = fread(in_buff, 1, benchedsize, fileIn);
	  fclose(fileIn);

	  if(readSize != benchedsize)
	  {
		DISPLAY("\nError: problem reading file '%s' !!    \n", infilename);
		free(in_buff);
		free(out_buff);
		return 13;
	  }

	  // Calculating input Checksum
	  crcc = BMK_checksum_MMH3A(in_buff, (unsigned int)benchedsize);


	  // Bench
	  {
		int loopNb, nb_loops, chunkNb;
		size_t cSize=0;
	    int milliTime;
		double fastestC = 100000000., fastestD = 100000000.;
		double ratio=0.;

		DISPLAY("\r%79s\r", "");
		for (loopNb = 1; loopNb <= nbIterations; loopNb++)
		{
		  // Compression
		  DISPLAY("%1i-%-14.14s : %9i ->\r", loopNb, infilename, (int)benchedsize);
		  { size_t i; for (i=0; i<benchedsize; i++) out_buff[i]=(char)i; }     // warmimg up memory

		  nb_loops = 0;
		  milliTime = BMK_GetMilliStart();
		  while(BMK_GetMilliStart() == milliTime);
		  milliTime = BMK_GetMilliStart();
		  while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
		  {
			for (chunkNb=0; chunkNb<nbChunks; chunkNb++)
				chunkP[chunkNb].outputSize = compP.compressionFunction(chunkP[chunkNb].inputBuffer, chunkP[chunkNb].outputBuffer, chunkP[chunkNb].inputSize);
			nb_loops++;
		  }
		  milliTime = BMK_GetMilliSpan(milliTime);

		  if ((double)milliTime < fastestC*nb_loops) fastestC = (double)milliTime/nb_loops;
		  cSize=0; for (chunkNb=0; chunkNb<nbChunks; chunkNb++) cSize += chunkP[chunkNb].outputSize;
		  ratio = (double)cSize/(double)benchedsize*100.;

		  DISPLAY("%1i-%-14.14s : %9i -> %9i (%5.2f%%),%7.1f MB/s\r", loopNb, infilename, (int)benchedsize, (int)cSize, ratio, (double)benchedsize / fastestC / 1000.);

		  // Decompression
		  { size_t i; for (i=0; i<benchedsize; i++) in_buff[i]=0; }     // zeroing area, for CRC checking

		  nb_loops = 0;
		  milliTime = BMK_GetMilliStart();
		  while(BMK_GetMilliStart() == milliTime);
		  milliTime = BMK_GetMilliStart();
		  while(BMK_GetMilliSpan(milliTime) < TIMELOOP)
		  {
			for (chunkNb=0; chunkNb<nbChunks; chunkNb++)
				chunkP[chunkNb].outputSize = LZ4_uncompress(chunkP[chunkNb].outputBuffer, chunkP[chunkNb].inputBuffer, chunkP[chunkNb].inputSize);
				//chunkP[chunkNb].inputSize = LZ4_uncompress_unknownOutputSize(chunkP[chunkNb].outputBuffer, chunkP[chunkNb].inputBuffer, chunkP[chunkNb].outputSize, chunkSize);
			nb_loops++;
		  }
		  milliTime = BMK_GetMilliSpan(milliTime);

		  if ((double)milliTime < fastestD*nb_loops) fastestD = (double)milliTime/nb_loops;
		  DISPLAY("%1i-%-14.14s : %9i -> %9i (%5.2f%%),%7.1f MB/s ,%7.1f MB/s\r", loopNb, infilename, (int)benchedsize, (int)cSize, ratio, (double)benchedsize / fastestC / 1000., (double)benchedsize / fastestD / 1000.);

		  // CRC Checking
		  crcd = BMK_checksum_MMH3A(in_buff, (unsigned int)benchedsize);
		  if (crcc!=crcd) { DISPLAY("\n!!! WARNING !!! %14s : Invalid Checksum : %x != %x\n", infilename, (unsigned)crcc, (unsigned)crcd); break; }
		}

	    if (crcc==crcd)
		{
			if (ratio<100.)
				DISPLAY("%-16.16s : %9i -> %9i (%5.2f%%),%7.1f MB/s ,%7.1f MB/s\n", infilename, (int)benchedsize, (int)cSize, ratio, (double)benchedsize / fastestC / 1000., (double)benchedsize / fastestD / 1000.);
			else
				DISPLAY("%-16.16s : %9i -> %9i (%5.1f%%),%7.1f MB/s ,%7.1f MB/s \n", infilename, (int)benchedsize, (int)cSize, ratio, (double)benchedsize / fastestC / 1000., (double)benchedsize / fastestD / 1000.);
		}
		totals += benchedsize;
		totalz += cSize;
		totalc += fastestC;
		totald += fastestD;
	  }

	  free(in_buff);
	  free(out_buff);
	  free(chunkP);
  }

  if (nbFiles > 1)
		printf("%-16.16s :%10llu ->%10llu (%5.2f%%), %6.1f MB/s , %6.1f MB/s\n", "  TOTAL", (long long unsigned int)totals, (long long unsigned int)totalz, (double)totalz/(double)totals*100., (double)totals/totalc/1000., (double)totals/totald/1000.);

  if (BMK_pause) { printf("press enter...\n"); getchar(); }

  return 0;
}



