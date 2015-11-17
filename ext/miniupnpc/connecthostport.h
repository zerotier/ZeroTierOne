/* $Id: connecthostport.h,v 1.3 2012/09/27 15:42:10 nanard Exp $ */
/* Project: miniupnp
 * http://miniupnp.free.fr/
 * Author: Thomas Bernard
 * Copyright (c) 2010-2012 Thomas Bernard
 * This software is subjects to the conditions detailed
 * in the LICENCE file provided within this distribution */
#ifndef CONNECTHOSTPORT_H_INCLUDED
#define CONNECTHOSTPORT_H_INCLUDED

/* connecthostport()
 * return a socket connected (TCP) to the host and port
 * or -1 in case of error */
int connecthostport(const char * host, unsigned short port,
                    unsigned int scope_id);

#endif

