#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#define VSDM_DEBUG 1

#include "vsdm.hpp"

int main(int argc,char **argv)
{
	if (argc < 4) {
		printf("Usage: vsdm-test <id> <node-id> <port> [<remote-node-id>/<remote-address>/<remote-port>]\n");
		return 0;
	}

	uint64_t id = (uint64_t)strtoull(argv[1],(char **)0,10);
	uint64_t node = (uint64_t)strtoull(argv[2],(char **)0,10);
	int port = (int)strtol(argv[3],(char **)0,10);

	struct sockaddr_in sa;
	memset(&sa,0,sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons((uint16_t)port);

	vsdm<std::string,std::string> m(id,node,false);
	m.listen((const struct sockaddr *)&sa);

	for(int i=4;i<argc;++i) {
		char tmp[1024];
		strncpy(tmp,argv[i],sizeof(tmp));
		int k = 0;
		char *rnode = (char *)0;
		char *raddr = (char *)0;
		char *rport = (char *)0;
		for (char *f=strtok(tmp,"/");f;f=strtok((char *)0,"/")) {
			switch(k++) {
				case 0:
					rnode = f;
					break;
				case 1:
					raddr = f;
					break;
				case 2:
					rport = f;
					break;
			}
		}

		if ((rnode)&&(raddr)&&(rport)) {
			sa.sin_family = AF_INET;
			sa.sin_port = htons((uint16_t)strtol(rport,(char **)0,10));
			if (!inet_aton(raddr,&(sa.sin_addr))) {
				printf("Error: %s is not a valid IPv4 address.\n",raddr);
				return 1;
			}
			m.link((uint64_t)strtoull(rnode,(char **)0,10),&sa,sizeof(sa));
		}
	}

	for(;;) {
		char k[1024];
		char v[1024];
		snprintf(k,sizeof(k),"%lu",(unsigned long)node);
		snprintf(v,sizeof(v),"%lu",(unsigned long)time(0));
		m.set(k,v);
		usleep(1000);
	}

	return 0;
}
