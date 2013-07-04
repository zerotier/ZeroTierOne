/* Fake zerotier-one binary to test launcher upgrade procedure */

#include <stdio.h>
#include <unistd.h>
#include "launcher.h"

const unsigned char EMBEDDED_VERSION_STAMP[20] = {
	0x6d,0xfe,0xff,0x01,0x90,0xfa,0x89,0x57,0x88,0xa1,0xaa,0xdc,0xdd,0xde,0xb0,0x33,
	ZEROTIER_FAKE_VERSION_MAJOR,
	ZEROTIER_FAKE_VERSION_MINOR,
	(unsigned char)(((unsigned int)ZEROTIER_FAKE_VERSION_REVISION) & 0xff), /* little-endian */
	(unsigned char)((((unsigned int)ZEROTIER_FAKE_VERSION_REVISION) >> 8) & 0xff) 
};

int main(int argc,char **argv)
{
	fprintf(stderr,"Fake ZeroTier binary version %d.%d.%d\n",ZEROTIER_FAKE_VERSION_MAJOR,ZEROTIER_FAKE_VERSION_MINOR,ZEROTIER_FAKE_VERSION_REVISION);
	sleep(5);
	fprintf(stderr,"  (exiting)\n");
	return ZEROTIER_EXEC_RETURN_VALUE_TERMINATED_FOR_UPGRADE;
}
