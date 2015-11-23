/* $Id: upnpdev.h,v 1.1 2015/08/28 12:14:19 nanard Exp $ */
/* Project : miniupnp
 * Web : http://miniupnp.free.fr/
 * Author : Thomas BERNARD
 * copyright (c) 2005-2015 Thomas Bernard
 * This software is subjet to the conditions detailed in the
 * provided LICENSE file. */
#ifndef UPNPDEV_H_INCLUDED
#define UPNPDEV_H_INCLUDED

#include "miniupnpc_declspec.h"

#ifdef __cplusplus
extern "C" {
#endif

struct UPNPDev {
	struct UPNPDev * pNext;
	char * descURL;
	char * st;
	unsigned int scope_id;
	char * usn;
	char buffer[3];
};

/* freeUPNPDevlist()
 * free list returned by upnpDiscover() */
MINIUPNP_LIBSPEC void freeUPNPDevlist(struct UPNPDev * devlist);


#ifdef __cplusplus
}
#endif


#endif /* UPNPDEV_H_INCLUDED */
