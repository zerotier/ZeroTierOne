
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rng.h"

#define SUCCESS 0
#define FILE_OPEN_ERROR -1
#define KYBER_SYMBYTES 32

void fprintBstr(FILE *fp, unsigned char *A, unsigned long L)
{
	unsigned long  i;

	for ( i=0; i<L; i++ )
		fprintf(fp, "%02X", A[i]);

	fprintf(fp, "\n");
}

int main()
{
  unsigned char       seed[48];
  unsigned char       seeds[100][48];
  unsigned char       entropy_input[48];
  unsigned char       ckk[32], indcpa[32], enc[32];
  FILE                *fp_ckk, *fp_indcpa, *fp_enc;
  char                fn_ckk[32], fn_indcpa[32], fn_enc[32];

  // Create the crypto_kem_keypair file
  sprintf(fn_ckk, "crypto_kem_keypair");
  if ( (fp_ckk = fopen(fn_ckk, "w")) == NULL ) {
    printf("Couldn't open <%s> for write\n", fn_ckk);
    return FILE_OPEN_ERROR;
  }

    // Create the indcpa_keypair file
  sprintf(fn_indcpa, "indcpa_keypair");
  if ( (fp_indcpa = fopen(fn_indcpa, "w")) == NULL ) {
    printf("Couldn't open <%s> for write\n", fn_indcpa);
    return FILE_OPEN_ERROR;
  }

  // Create the encode file
  sprintf(fn_enc, "encode");
  if ( (fp_enc = fopen(fn_enc, "w")) == NULL ) {
    printf("Couldn't open <%s> for write\n", fn_enc);
    return FILE_OPEN_ERROR;
  }

  // 1 .. 48 initial seed
  for (int i=0; i<48; i++)
      entropy_input[i] = i;

 
  // initialise
  randombytes_init(entropy_input, NULL, 256);
  for (int i=0; i<100; i++) {
      randombytes(seed, 48);
      for (int j=0;j<48;j++)
        seeds[i][j] = seed[j];
  }

  for (int i=0; i<100; i++) {
    randombytes_init(seeds[i], NULL, 256);
    randombytes(indcpa, KYBER_SYMBYTES);
    randombytes(ckk, KYBER_SYMBYTES);
    randombytes(enc, KYBER_SYMBYTES);
    fprintBstr(fp_indcpa, indcpa, KYBER_SYMBYTES);
    fprintBstr(fp_ckk, ckk, KYBER_SYMBYTES);
    fprintBstr(fp_enc, enc, KYBER_SYMBYTES);
  }

  fclose(fp_indcpa);
  fclose(fp_ckk);
  fclose(fp_enc);

  return SUCCESS;
}
