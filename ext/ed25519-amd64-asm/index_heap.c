#include "sc25519.h"
#include "index_heap.h"

/* caller's responsibility to ensure hlen>=3 */
void heap_init(unsigned long long *h, unsigned long long hlen, sc25519 *scalars)
{
  h[0] = 0;
  unsigned long long i=1;
  while(i<hlen)
    heap_push(h, &i, i, scalars);
}

void heap_extend(unsigned long long *h, unsigned long long oldlen, unsigned long long newlen, sc25519 *scalars)
{
  unsigned long long i=oldlen;
  while(i<newlen)
    heap_push(h, &i, i, scalars);
}


void heap_push(unsigned long long *h, unsigned long long *hlen, unsigned long long elem, sc25519 *scalars)
{
  /* Move up towards the root */
  /* XXX: Check size of hlen, whether cast to signed value is ok */
  signed long long pos = *hlen;
  signed long long ppos = (pos-1)/2;
  unsigned long long t;
  h[*hlen] = elem;
  while(pos > 0)
  {
    /* if(sc25519_lt_vartime(&scalars[h[ppos]], &scalars[h[pos]])) */
    if(sc25519_lt(&scalars[h[ppos]], &scalars[h[pos]]))
    {
      t = h[ppos];
      h[ppos] = h[pos];
      h[pos] = t;
      pos = ppos;
      ppos = (pos-1)/2;
    }
    else break;
  } 
  (*hlen)++;
}

/* Put the largest value in the heap in max1, the second largest in max2 */
void heap_get2max(unsigned long long *h, unsigned long long *max1, unsigned long long *max2, sc25519 *scalars)
{
  *max1 = h[0];
  *max2 = h[1];
  if(sc25519_lt(&scalars[h[1]],&scalars[h[2]]))
    *max2 = h[2];
}

/* After the root has been replaced, restore heap property */
/* extern void heap_rootreplaced(unsigned long long *h, unsigned long long hlen, sc25519 *scalars);
*/
/* extern void heap_rootreplaced_shortscalars(unsigned long long *h, unsigned long long hlen, sc25519 *scalars);
*/
