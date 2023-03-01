#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#ifndef NO_RETURN_ATTR
  #ifdef __GNUC__
    #define NO_RETURN_ATTR __attribute__ ((noreturn))
  #else // __GNUC__
    #define NO_RETURN_ATTR
  #endif // __GNUC__
#endif // NO_RETURN_ATTR


void loop_forever(void) NO_RETURN_ATTR;

uint8_t normal_return(void);
