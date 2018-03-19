#include "sc25519.h"

void sc25519_slide(signed char r[256], const sc25519 *s, int swindowsize)
{
  int i,j,k,b,m=(1<<(swindowsize-1))-1, soplen=256;
  unsigned long long sv0 = s->v[0];
  unsigned long long sv1 = s->v[1];
  unsigned long long sv2 = s->v[2];
  unsigned long long sv3 = s->v[3];

  /* first put the binary expansion into r  */
  for(i=0;i<64;i++) {
    r[i] = sv0 & 1;
    r[i+64] = sv1 & 1;
    r[i+128] = sv2 & 1;
    r[i+192] = sv3 & 1;
    sv0 >>= 1;
    sv1 >>= 1;
    sv2 >>= 1;
    sv3 >>= 1;
  }

  /* Making it sliding window */
  for (j = 0;j < soplen;++j) 
  {
    if (r[j]) {
      for (b = 1;b < soplen - j && b <= 6;++b) {
        if (r[j] + (r[j + b] << b) <= m) 
        {
          r[j] += r[j + b] << b; r[j + b] = 0;
        } 
        else if (r[j] - (r[j + b] << b) >= -m) 
        {
          r[j] -= r[j + b] << b;
          for (k = j + b;k < soplen;++k) 
          {
            if (!r[k]) {
              r[k] = 1;
              break;
            }
            r[k] = 0;
          }
        } 
        else if (r[j + b])
          break;
      }
    }
  }
}
