/* $Id: natpmpc.c,v 1.13 2012/08/21 17:23:38 nanard Exp $ */
/* libnatpmp
Copyright (c) 2007-2011, Thomas BERNARD
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * The name of the author may not be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#if defined(_MSC_VER)
#if _MSC_VER >= 1400
#define strcasecmp _stricmp
#else
#define strcasecmp stricmp
#endif
#else
#include <unistd.h>
#endif
#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "natpmp.h"

void usage(FILE * out, const char * argv0)
{
	fprintf(out, "Usage :\n");
	fprintf(out, "  %s [options]\n", argv0);
	fprintf(out, "\tdisplay the public IP address.\n");
	fprintf(out, "  %s -h\n", argv0);
	fprintf(out, "\tdisplay this help screen.\n");
	fprintf(out, "  %s [options] -a <public port> <private port> <protocol> [lifetime]\n", argv0);
	fprintf(out, "\tadd a port mapping.\n");
	fprintf(out, "\nOption available :\n");
	fprintf(out, "  -g ipv4address\n");
	fprintf(out, "\tforce the gateway to be used as destination for NAT-PMP commands.\n");
	fprintf(out, "\n  In order to remove a mapping, set it with a lifetime of 0 seconds.\n");
	fprintf(out, "  To remove all mappings for your machine, use 0 as private port and lifetime.\n");
}

/* sample code for using libnatpmp */
int main(int argc, char * * argv)
{
	natpmp_t natpmp;
	natpmpresp_t response;
	int r;
	int sav_errno;
	struct timeval timeout;
	fd_set fds;
	int i;
	int protocol = 0;
	uint16_t privateport = 0;
	uint16_t publicport = 0;
	uint32_t lifetime = 3600;
	int command = 0;
	int forcegw = 0;
	in_addr_t gateway = 0;
	struct in_addr gateway_in_use;

#ifdef WIN32
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(nResult != NO_ERROR)
	{
		fprintf(stderr, "WSAStartup() failed.\n");
		return -1;
	}
#endif

	/* argument parsing */
	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			switch(argv[i][1]) {
			case 'h':
				usage(stdout, argv[0]);
				return 0;
			case 'g':
				forcegw = 1;
				if(argc < i + 1) {
					fprintf(stderr, "Not enough arguments for option -%c\n", argv[i][1]);
					return 1;
				}
				gateway = inet_addr(argv[++i]);
				break;
			case 'a':
				command = 'a';
				if(argc < i + 4) {
					fprintf(stderr, "Not enough arguments for option -%c\n", argv[i][1]);
					return 1;
				}
				i++;
				if(1 != sscanf(argv[i], "%hu", &publicport)) {
					fprintf(stderr, "%s is not a correct 16bits unsigned integer\n", argv[i]);
					return 1;
				}
				i++;
				if(1 != sscanf(argv[i], "%hu", &privateport)) {
					fprintf(stderr, "%s is not a correct 16bits unsigned integer\n", argv[i]);
					return 1;
				}
				i++;
				if(0 == strcasecmp(argv[i], "tcp"))
					protocol = NATPMP_PROTOCOL_TCP;
				else if(0 == strcasecmp(argv[i], "udp"))
					protocol = NATPMP_PROTOCOL_UDP;
				else {
					fprintf(stderr, "%s is not a valid protocol\n", argv[i]);
					return 1;
				}
				if(argc > i + 1) {
					if(1 != sscanf(argv[i+1], "%u", &lifetime)) {
						fprintf(stderr, "%s is not a correct 32bits unsigned integer\n", argv[i]);
					} else {
						i++;
					}
				}
				break;
			default:
				fprintf(stderr, "Unknown option %s\n", argv[i]);
				usage(stderr, argv[0]);
				return 1;
			}
		} else {
			fprintf(stderr, "Unknown option %s\n", argv[i]);
			usage(stderr, argv[0]);
			return 1;
		}
	}

	/* initnatpmp() */
	r = initnatpmp(&natpmp, forcegw, gateway);
	printf("initnatpmp() returned %d (%s)\n", r, r?"FAILED":"SUCCESS");
	if(r<0)
		return 1;

	gateway_in_use.s_addr = natpmp.gateway;
	printf("using gateway : %s\n", inet_ntoa(gateway_in_use));

	/* sendpublicaddressrequest() */
	r = sendpublicaddressrequest(&natpmp);
	printf("sendpublicaddressrequest returned %d (%s)\n",
	       r, r==2?"SUCCESS":"FAILED");
	if(r<0)
		return 1;

	do {
		FD_ZERO(&fds);
		FD_SET(natpmp.s, &fds);
		getnatpmprequesttimeout(&natpmp, &timeout);
		r = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		if(r<0) {
			fprintf(stderr, "select()");
			return 1;
		}
		r = readnatpmpresponseorretry(&natpmp, &response);
		sav_errno = errno;
		printf("readnatpmpresponseorretry returned %d (%s)\n",
		       r, r==0?"OK":(r==NATPMP_TRYAGAIN?"TRY AGAIN":"FAILED"));
		if(r<0 && r!=NATPMP_TRYAGAIN) {
#ifdef ENABLE_STRNATPMPERR
			fprintf(stderr, "readnatpmpresponseorretry() failed : %s\n",
			        strnatpmperr(r));
#endif
			fprintf(stderr, "  errno=%d '%s'\n",
			        sav_errno, strerror(sav_errno));
		}
	} while(r==NATPMP_TRYAGAIN);
	if(r<0)
		return 1;

	/* TODO : check that response.type == 0 */
	printf("Public IP address : %s\n", inet_ntoa(response.pnu.publicaddress.addr));
	printf("epoch = %u\n", response.epoch);

	if(command == 'a') {
		/* sendnewportmappingrequest() */
		r = sendnewportmappingrequest(&natpmp, protocol,
        	                      privateport, publicport,
								  lifetime);
		printf("sendnewportmappingrequest returned %d (%s)\n",
		       r, r==12?"SUCCESS":"FAILED");
		if(r < 0)
			return 1;

		do {
			FD_ZERO(&fds);
			FD_SET(natpmp.s, &fds);
			getnatpmprequesttimeout(&natpmp, &timeout);
			select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
			r = readnatpmpresponseorretry(&natpmp, &response);
			printf("readnatpmpresponseorretry returned %d (%s)\n",
			       r, r==0?"OK":(r==NATPMP_TRYAGAIN?"TRY AGAIN":"FAILED"));
		} while(r==NATPMP_TRYAGAIN);
		if(r<0) {
#ifdef ENABLE_STRNATPMPERR
			fprintf(stderr, "readnatpmpresponseorretry() failed : %s\n",
			        strnatpmperr(r));
#endif
			return 1;
		}

		printf("Mapped public port %hu protocol %s to local port %hu "
		       "liftime %u\n",
	    	   response.pnu.newportmapping.mappedpublicport,
			   response.type == NATPMP_RESPTYPE_UDPPORTMAPPING ? "UDP" :
			    (response.type == NATPMP_RESPTYPE_TCPPORTMAPPING ? "TCP" :
			     "UNKNOWN"),
			   response.pnu.newportmapping.privateport,
			   response.pnu.newportmapping.lifetime);
		printf("epoch = %u\n", response.epoch);
	}

	r = closenatpmp(&natpmp);
	printf("closenatpmp() returned %d (%s)\n", r, r==0?"SUCCESS":"FAILED");
	if(r<0)
		return 1;

	return 0;
}

