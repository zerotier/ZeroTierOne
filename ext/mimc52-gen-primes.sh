#!/bin/bash

# This generates the largest 512 primes less than 2^52 that are of the form (6k-5) for use with the MIMC52 VDF.
# It requires primesieve ("brew install primeseive" or "apt-get install primesieve"), sed, and bc.

# No need to ever really run this again. It's just included for documentation purposes.

for p in `primesieve 4503599627300000 4503599627370495 -p`; do
  echo $p FC_"`echo "(($p - 5) % 6)" | bc`" | grep -F FC_0 | cut -d ' ' -f 1
done | tail -n 512 | sed 's/$/ULL,/'
