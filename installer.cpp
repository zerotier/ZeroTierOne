/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "node/Constants.hpp"

#include "version.h"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <tchar.h>
#include <wchar.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#endif

#include "ext/lz4/lz4.h"
#include "ext/lz4/lz4hc.h"

// Include generated binaries -------------------------------------------------

// zerotier-one binary (or zerotier-one.exe for Windows)
#include "installer-build/zerotier_one.c"

// Unix uninstall script, installed in home for user to remove
#ifdef __UNIX_LIKE__
#include "installer-build/uninstall_sh.c"
#endif

// Linux init.d script
#ifdef __LINUX__
#include "installer-build/redhat__init_d__zerotier_one.c"
#include "installer-build/debian__init_d__zerotier_one.c"
#endif

// Apple Tap device driver
#ifdef __APPLE__
#include "installer-build/tap_mac__Info_plist.c"
#include "installer-build/tap_mac__tap.c"
#endif

// Windows Tap device drivers
#ifdef __WINDOWS__
#include "installer-build/tap_windows__x64__ztTap100_sys.c"
#include "installer-build/tap_windows__x64__ztTap100_inf.c"
#include "installer-build/tap_windows__x86__ztTap100_sys.c"
#include "installer-build/tap_windows__x86__ztTap100_inf.c"
#include "installer-build/tap_windows__devcon32_exe.c"
#include "installer-build/tap_windows__devcon64_exe.c"
#endif

// ----------------------------------------------------------------------------

static unsigned char *unlz4(const void *lz4,int decompressedLen)
{
	unsigned char *buf = new unsigned char[decompressedLen];
	if (LZ4_decompress_fast((const char *)lz4,(char *)buf,decompressedLen) != decompressedLen) {
		delete [] buf;
		return (unsigned char *)0;
	}
	return buf;
}

static bool _instFile(const void *lz4,int decompressedLen,const char *path)
{
	unsigned char *data = unlzr(lz4,decompressedLen);
	if (!data)
		return false;
	FILE *f = fopen(path,"w");
	if (!f) {
		delete [] data;
		return false;
	}
	if (fwrite(data,decompressedLen,1,f) != 1) {
		fclose(f);
		delete [] data;
		Utils::rm(path);
		return false;
	}
	fclose(f);
	delete [] data;
	return true;
}
#define instFile(name,path) _instFile(name,name##_UNCOMPRESSED_LEN,path)

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
	char buf[4096];

#ifdef __UNIX_LIKE__

	if (getuid() != 0) {
		fprintf(stderr,"ZeroTier One installer must be run as root.\n");
		return 2;
	}

	const char *zthome;
#ifdef __APPLE__
	mkdir("/Library/Application Support/ZeroTier",0755);
	mkdir(zthome = "/Library/Application Support/ZeroTier/One",0755);
#else
	mkdir("/var/lib",0755);
	mkdir(zthome = "/var/lib/zerotier-one",0755);
#endif
	chown(zthome,0,0);

	sprintf(buf,"%s/zerotier-one",zthome);
	if (!instFile(zerotier_one,buf)) {
		fprintf(stderr,"Unable to write %s\n",buf);
		return 1;
	}
	chmod(buf,0700);
	chown(buf,0,0);
	fprintf(stdout,"%s\n",buf);

	sprintf(buf,"%s/uninstall.sh",zthome);
	if (!instFile(uninstall_sh,buf)) {
		fprintf(stderr,"Unable to write %s\n",buf);
		return 1;
	}
	chmod(buf,0755);
	chown(buf,0,0);
	fprintf(stdout,"%s\n",buf);

#ifdef __APPLE__
	sprintf(buf,"%s/tap.kext");
	mkdir(buf,0755);
	chmod(buf,0755);
	chown(buf,0,0);
	sprintf(buf,"%s/tap.kext/Contents");
	mkdir(buf,0755);
	chmod(buf,0755);
	chown(buf,0,0);
	sprintf(buf,"%s/tap.kext/Contents/MacOS");
	mkdir(buf,0755);
	chmod(buf,0755);
	chown(buf,0,0);
	sprintf(buf,"%s/tap.kext/Contents/Info.plist",zthome);
	if (!instFile(tap_mac__Info_plist,buf)) {
		fprintf(stderr,"Unable to write %s\n",buf);
		return 1;
	}
	chmod(buf,0644);
	chown(buf,0,0);
	fprintf(stdout,"%s\n",buf);
	sprintf(buf,"%s/tap.kext/Contents/MacOS/tap",zthome);
	if (!instFile(tap_mac__tap,buf)) {
		fprintf(stderr,"Unable to write %s\n",buf);
		return 1;
	}
	chmod(buf,0755);
	chown(buf,0,0);
	fprintf(stdout,"%s\n",buf);
#endif

#ifdef __LINUX__
	struct stat st;
	if (stat("/etc/redhat-release",&st) == 0) {
		// Redhat-derived distribution
		sprintf(buf,"/etc/init.d/zerotier-one");
		if (!instFile(redhat__init_d__zerotier_one,buf)) {
			fprintf(stderr,"Unable to write %s\n",buf);
			return 1;
		}
		chmod(buf,0755);
		fprintf(stdout,"%s (version for RedHat-derived distros)\n",buf);
	}
	if (stat("/etc/debian_version",&st) == 0) {
		// Debian-derived distribution
		sprintf(buf,"/etc/init.d/zerotier-one");
		if (!instFile(debian__init_d__zerotier_one,buf)) {
			fprintf(stderr,"Unable to write %s\n",buf);
			return 1;
		}
		chmod(buf,0755);
		fprintf(stdout,"%s (version for Debian-derived distros)\n",buf);
	}
#endif

#endif // __UNIX_LIKE__

#ifdef __WINDOWS__

#endif // __WINDOWS__

	return 0;
}
