/* $Id: miniupnpc.h,v 1.42 2015/07/21 13:16:55 nanard Exp $ */
/* Project: miniupnp
 * http://miniupnp.free.fr/
 * Author: Thomas Bernard
 * Copyright (c) 2005-2015 Thomas Bernard
 * This software is subjects to the conditions detailed
 * in the LICENCE file provided within this distribution */
#ifndef MINIUPNPC_H_INCLUDED
#define MINIUPNPC_H_INCLUDED

#include "miniupnpc_declspec.h"
#include "igd_desc_parse.h"

/* error codes : */
#define UPNPDISCOVER_SUCCESS (0)
#define UPNPDISCOVER_UNKNOWN_ERROR (-1)
#define UPNPDISCOVER_SOCKET_ERROR (-101)
#define UPNPDISCOVER_MEMORY_ERROR (-102)

/* versions : */
#define MINIUPNPC_VERSION	"1.9.20150721"
#define MINIUPNPC_API_VERSION	13

#ifdef __cplusplus
extern "C" {
#endif

/* Structures definitions : */
struct UPNParg { const char * elt; const char * val; };

char *
simpleUPnPcommand(int, const char *, const char *,
                  const char *, struct UPNParg *,
                  int *);

struct UPNPDev {
	struct UPNPDev * pNext;
	char * descURL;
	char * st;
	unsigned int scope_id;
	char buffer[2];
};

/* upnpDiscover()
 * discover UPnP devices on the network.
 * The discovered devices are returned as a chained list.
 * It is up to the caller to free the list with freeUPNPDevlist().
 * delay (in millisecond) is the maximum time for waiting any device
 * response.
 * If available, device list will be obtained from MiniSSDPd.
 * Default path for minissdpd socket will be used if minissdpdsock argument
 * is NULL.
 * If multicastif is not NULL, it will be used instead of the default
 * multicast interface for sending SSDP discover packets.
 * If sameport is not null, SSDP packets will be sent from the source port
 * 1900 (same as destination port) otherwise system assign a source port.
 * "searchalltypes" parameter is useful when searching several types,
 * if 0, the discovery will stop with the first type returning results. */
MINIUPNP_LIBSPEC struct UPNPDev *
upnpDiscover(int delay, const char * multicastif,
             const char * minissdpdsock, int sameport,
             int ipv6,
             int * error);

MINIUPNP_LIBSPEC struct UPNPDev *
upnpDiscoverAll(int delay, const char * multicastif,
                const char * minissdpdsock, int sameport,
                int ipv6,
                int * error);

MINIUPNP_LIBSPEC struct UPNPDev *
upnpDiscoverDevice(const char * device, int delay, const char * multicastif,
                const char * minissdpdsock, int sameport,
                int ipv6,
                int * error);

MINIUPNP_LIBSPEC struct UPNPDev *
upnpDiscoverDevices(const char * const deviceTypes[],
                    int delay, const char * multicastif,
                    const char * minissdpdsock, int sameport,
                    int ipv6,
                    int * error,
                    int searchalltypes);

/* freeUPNPDevlist()
 * free list returned by upnpDiscover() */
MINIUPNP_LIBSPEC void freeUPNPDevlist(struct UPNPDev * devlist);

/* parserootdesc() :
 * parse root XML description of a UPnP device and fill the IGDdatas
 * structure. */
MINIUPNP_LIBSPEC void parserootdesc(const char *, int, struct IGDdatas *);

/* structure used to get fast access to urls
 * controlURL: controlURL of the WANIPConnection
 * ipcondescURL: url of the description of the WANIPConnection
 * controlURL_CIF: controlURL of the WANCommonInterfaceConfig
 * controlURL_6FC: controlURL of the WANIPv6FirewallControl
 */
struct UPNPUrls {
	char * controlURL;
	char * ipcondescURL;
	char * controlURL_CIF;
	char * controlURL_6FC;
	char * rootdescURL;
};

/* UPNP_GetValidIGD() :
 * return values :
 *     0 = NO IGD found
 *     1 = A valid connected IGD has been found
 *     2 = A valid IGD has been found but it reported as
 *         not connected
 *     3 = an UPnP device has been found but was not recognized as an IGD
 *
 * In any non zero return case, the urls and data structures
 * passed as parameters are set. Donc forget to call FreeUPNPUrls(urls) to
 * free allocated memory.
 */
MINIUPNP_LIBSPEC int
UPNP_GetValidIGD(struct UPNPDev * devlist,
                 struct UPNPUrls * urls,
				 struct IGDdatas * data,
				 char * lanaddr, int lanaddrlen);

/* UPNP_GetIGDFromUrl()
 * Used when skipping the discovery process.
 * When succeding, urls, data, and lanaddr arguments are set.
 * return value :
 *   0 - Not ok
 *   1 - OK */
MINIUPNP_LIBSPEC int
UPNP_GetIGDFromUrl(const char * rootdescurl,
                   struct UPNPUrls * urls,
                   struct IGDdatas * data,
                   char * lanaddr, int lanaddrlen);

MINIUPNP_LIBSPEC void
GetUPNPUrls(struct UPNPUrls *, struct IGDdatas *,
            const char *, unsigned int);

MINIUPNP_LIBSPEC void
FreeUPNPUrls(struct UPNPUrls *);

/* return 0 or 1 */
MINIUPNP_LIBSPEC int UPNPIGD_IsConnected(struct UPNPUrls *, struct IGDdatas *);


#ifdef __cplusplus
}
#endif

#endif

