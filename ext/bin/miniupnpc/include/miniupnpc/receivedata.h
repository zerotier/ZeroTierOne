/* $Id: receivedata.h,v 1.4 2012/09/27 15:42:10 nanard Exp $ */
/* Project: miniupnp
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * Author: Thomas Bernard
 * Copyright (c) 2011-2012 Thomas Bernard
 * This software is subjects to the conditions detailed
 * in the LICENCE file provided within this distribution */
#ifndef RECEIVEDATA_H_INCLUDED
#define RECEIVEDATA_H_INCLUDED

/* Reads data from the specified socket.
 * Returns the number of bytes read if successful, zero if no bytes were
 * read or if we timed out. Returns negative if there was an error. */
int receivedata(int socket,
                char * data, int length,
                int timeout, unsigned int * scope_id);

#endif

