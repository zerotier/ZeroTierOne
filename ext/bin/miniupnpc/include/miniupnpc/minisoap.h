/* $Id: minisoap.h,v 1.5 2012/09/27 15:42:10 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2005 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution. */
#ifndef MINISOAP_H_INCLUDED
#define MINISOAP_H_INCLUDED

/*int httpWrite(int, const char *, int, const char *);*/
int soapPostSubmit(int, const char *, const char *, unsigned short,
		   const char *, const char *, const char *);

#endif

