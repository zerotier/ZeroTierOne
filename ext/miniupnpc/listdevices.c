/* $Id: listdevices.c,v 1.7 2015/10/08 16:15:47 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2013-2015 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#endif /* _WIN32 */
#include "miniupnpc.h"

int main(int argc, char * * argv)
{
	const char * searched_device = NULL;
	const char * * searched_devices = NULL;
	const char * multicastif = 0;
	const char * minissdpdpath = 0;
	int ipv6 = 0;
	unsigned char ttl = 2;
	int error = 0;
	struct UPNPDev * devlist = 0;
	struct UPNPDev * dev;
	int i;

#ifdef _WIN32
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(nResult != NO_ERROR)
	{
		fprintf(stderr, "WSAStartup() failed.\n");
		return -1;
	}
#endif

	for(i = 1; i < argc; i++) {
		if(strcmp(argv[i], "-6") == 0)
			ipv6 = 1;
		else if(strcmp(argv[i], "-d") == 0) {
			if(++i >= argc) {
				fprintf(stderr, "%s option needs one argument\n", "-d");
				return 1;
			}
			searched_device = argv[i];
		} else if(strcmp(argv[i], "-t") == 0) {
			if(++i >= argc) {
				fprintf(stderr, "%s option needs one argument\n", "-t");
				return 1;
			}
			ttl = (unsigned char)atoi(argv[i]);
		} else if(strcmp(argv[i], "-l") == 0) {
			if(++i >= argc) {
				fprintf(stderr, "-l option needs at least one argument\n");
				return 1;
			}
			searched_devices = (const char * *)(argv + i);
			break;
		} else if(strcmp(argv[i], "-m") == 0) {
			if(++i >= argc) {
				fprintf(stderr, "-m option needs one argument\n");
				return 1;
			}
			multicastif = argv[i];
		} else {
			printf("usage : %s [options] [-l <device1> <device2> ...]\n", argv[0]);
			printf("options :\n");
			printf("   -6 : use IPv6\n");
			printf("   -m address/ifname : network interface to use for multicast\n");
			printf("   -d <device string> : search only for this type of device\n");
			printf("   -l <device1> <device2> ... : search only for theses types of device\n");
			printf("   -t ttl : set multicast TTL. Default value is 2.\n");
			printf("   -h : this help\n");
			return 1;
		}
	}

	if(searched_device) {
		printf("searching UPnP device type %s\n", searched_device);
		devlist = upnpDiscoverDevice(searched_device,
		                             2000, multicastif, minissdpdpath,
		                             0/*localport*/, ipv6, ttl, &error);
	} else if(searched_devices) {
		printf("searching UPnP device types :\n");
		for(i = 0; searched_devices[i]; i++)
			printf("\t%s\n", searched_devices[i]);
		devlist = upnpDiscoverDevices(searched_devices,
		                              2000, multicastif, minissdpdpath,
		                              0/*localport*/, ipv6, ttl, &error, 1);
	} else {
		printf("searching all UPnP devices\n");
		devlist = upnpDiscoverAll(2000, multicastif, minissdpdpath,
		                             0/*localport*/, ipv6, ttl, &error);
	}
	if(devlist) {
		for(dev = devlist, i = 1; dev != NULL; dev = dev->pNext, i++) {
			printf("%3d: %-48s\n", i, dev->st);
			printf("     %s\n", dev->descURL);
			printf("     %s\n", dev->usn);
		}
		freeUPNPDevlist(devlist);
	} else {
		printf("no device found.\n");
	}

	return 0;
}

