#define ENABLE_STRNATPMPERR
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 600

#ifdef __APPLE__
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#endif

#include "../ext/libnatpmp/getgateway.c"
#include "../ext/libnatpmp/wingettimeofday.c"
#include "../ext/libnatpmp/natpmp.c"
