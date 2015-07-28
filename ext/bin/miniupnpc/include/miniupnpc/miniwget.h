/* $Id: miniwget.h,v 1.10 2015/07/21 13:16:55 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2005-2015 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */
#ifndef MINIWGET_H_INCLUDED
#define MINIWGET_H_INCLUDED

#include "miniupnpc_declspec.h"

#ifdef __cplusplus
extern "C" {
#endif

MINIUPNP_LIBSPEC void * getHTTPResponse(int s, int * size);

MINIUPNP_LIBSPEC void * miniwget(const char *, int *, unsigned int);

MINIUPNP_LIBSPEC void * miniwget_getaddr(const char *, int *, char *, int, unsigned int);

int parseURL(const char *, char *, unsigned short *, char * *, unsigned int *);

#ifdef __cplusplus
}
#endif

#endif

