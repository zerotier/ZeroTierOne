/* $Id: testportlistingparse.c,v 1.2 2014/11/01 10:37:32 nanard Exp $ */
/* Project : miniupnp
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * Author : Thomas Bernard
 * Copyright (c) 2014 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */

#include <string.h>
#include <stdio.h>
#include "portlistingparse.h"

struct port_mapping {
	unsigned int leasetime;
	unsigned short externalport;
	unsigned short internalport;
	const char * remotehost;
	const char * client;
	const char * proto;
	const char * desc;
	unsigned char enabled;
};

/* return the number of differences */
int test(const char * portListingXml, int portListingXmlLen,
         const struct port_mapping * ref, int count)
{
	int i;
	int r = 0;
	struct PortMappingParserData data;
	struct PortMapping * pm;

	memset(&data, 0, sizeof(data));
	ParsePortListing(portListingXml, portListingXmlLen, &data);
	for(i = 0, pm = data.l_head;
	    (pm != NULL) && (i < count);
	    i++, pm = pm->l_next) {
		printf("%2d %s %5hu->%s:%-5hu '%s' '%s' %u\n",
		       i, pm->protocol, pm->externalPort, pm->internalClient,
		       pm->internalPort,
		       pm->description, pm->remoteHost,
		       (unsigned)pm->leaseTime);
		if(0 != strcmp(pm->protocol, ref[i].proto)) {
			printf("protocol : '%s' != '%s'\n", pm->protocol, ref[i].proto);
			r++;
		}
		if(pm->externalPort != ref[i].externalport) {
			printf("externalPort : %hu != %hu\n",
			       pm->externalPort, ref[i].externalport);
			r++;
		}
		if(0 != strcmp(pm->internalClient, ref[i].client)) {
			printf("client : '%s' != '%s'\n",
			       pm->internalClient, ref[i].client);
			r++;
		}
		if(pm->internalPort != ref[i].internalport) {
			printf("internalPort : %hu != %hu\n",
			       pm->internalPort, ref[i].internalport);
			r++;
		}
		if(0 != strcmp(pm->description, ref[i].desc)) {
			printf("description : '%s' != '%s'\n",
			       pm->description, ref[i].desc);
			r++;
		}
		if(0 != strcmp(pm->remoteHost, ref[i].remotehost)) {
			printf("remoteHost : '%s' != '%s'\n",
			       pm->remoteHost, ref[i].remotehost);
			r++;
		}
		if((unsigned)pm->leaseTime != ref[i].leasetime) {
			printf("leaseTime : %u != %u\n",
			       (unsigned)pm->leaseTime, ref[i].leasetime);
			r++;
		}
		if(pm->enabled != ref[i].enabled) {
			printf("enabled : %d != %d\n",
			       (int)pm->enabled, (int)ref[i].enabled);
			r++;
		}
	}
	if((i != count) || (pm != NULL)) {
		printf("count mismatch : i=%d count=%d pm=%p\n", i, count, pm);
		r++;
	}
	FreePortListing(&data);
	return r;
}

const char test_document[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<p:PortMappingList xmlns:p=\"urn:schemas-upnp-org:gw:WANIPConnection\"\n"
"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" \n"
"xsi:schemaLocation=\"urn:schemas-upnp-org:gw:WANIPConnection "
"http://www.upnp.org/schemas/gw/WANIPConnection-v2.xsd\">\n"
" <p:PortMappingEntry>\n"
"  <p:NewRemoteHost></p:NewRemoteHost>\n"
"  <p:NewExternalPort>5002</p:NewExternalPort>\n"
"  <p:NewProtocol>UDP</p:NewProtocol>\n"
"  <p:NewInternalPort>4001</p:NewInternalPort>\n"
"  <p:NewInternalClient>192.168.1.123</p:NewInternalClient>\n"
"  <p:NewEnabled>1</p:NewEnabled>\n"
"  <p:NewDescription>xxx</p:NewDescription>\n"
"  <p:NewLeaseTime>0</p:NewLeaseTime>\n"
" </p:PortMappingEntry>\n"
" <p:PortMappingEntry>\n"
"  <p:NewRemoteHost>202.233.2.1</p:NewRemoteHost>\n"
"  <p:NewExternalPort>2345</p:NewExternalPort>\n"
"  <p:NewProtocol>TCP</p:NewProtocol>\n"
"  <p:NewInternalPort>2349</p:NewInternalPort>\n"
"  <p:NewInternalClient>192.168.1.137</p:NewInternalClient>\n"
"  <p:NewEnabled>1</p:NewEnabled>\n"
"  <p:NewDescription>dooom</p:NewDescription>\n"
"  <p:NewLeaseTime>346</p:NewLeaseTime>\n"
" </p:PortMappingEntry>\n"
" <p:PortMappingEntry>\n"
"  <p:NewRemoteHost>134.231.2.11</p:NewRemoteHost>\n"
"  <p:NewExternalPort>12345</p:NewExternalPort>\n"
"  <p:NewProtocol>TCP</p:NewProtocol>\n"
"  <p:NewInternalPort>12345</p:NewInternalPort>\n"
"  <p:NewInternalClient>192.168.1.137</p:NewInternalClient>\n"
"  <p:NewEnabled>1</p:NewEnabled>\n"
"  <p:NewDescription>dooom A</p:NewDescription>\n"
"  <p:NewLeaseTime>347</p:NewLeaseTime>\n"
" </p:PortMappingEntry>\n"
"</p:PortMappingList>";

#define PORT_MAPPINGS_COUNT 3
const struct port_mapping port_mappings[PORT_MAPPINGS_COUNT] = {
{347, 12345, 12345, "134.231.2.11", "192.168.1.137", "TCP", "dooom A", 1},
{346, 2345, 2349, "202.233.2.1", "192.168.1.137", "TCP", "dooom", 1},
{0, 5002, 4001, "", "192.168.1.123", "UDP", "xxx", 1}
};

/* --- main --- */
int main(void)
{
	int r;
	r = test(test_document, sizeof(test_document) - 1,
	         port_mappings, PORT_MAPPINGS_COUNT);
	if(r == 0) {
		printf("test of portlistingparse OK\n");
		return 0;
	} else {
		printf("test FAILED (%d differences counted)\n", r);
		return 1;
	}
}

