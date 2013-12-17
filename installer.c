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

/*
 * This can be run to install ZT1 for the first time or to update it. It
 * carries all payloads internally as LZ4 compressed blobs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "node/Constants.hpp"
#include "version.h"

#ifdef __WINDOWS__
#include <Windows.h>
#include <tchar.h>
#include <wchar.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#include "ext/lz4/lz4.h"
#include "ext/lz4/lz4hc.h"

/* Include LZ4 comrpessed blobs ********************************************/

/* zerotier-one binary (or zerotier-one.exe for Windows) */
#include "installer-build/zerotier_one.h"

/* Unix uninstall script, installed in home for user to remove */
#ifdef __UNIX_LIKE__
#include "installer-build/uninstall_sh.h"
#endif

/* Linux init.d script */
#ifdef __LINUX__
#include "installer-build/linux__init_d__zerotier_one.h"
#endif

/* Apple Tap device driver, launchd plist, and /Applications app */
#ifdef __APPLE__
#include "installer-build/tap_mac__Info_plist.h"
#include "installer-build/tap_mac__tap.h"
#include "installer-build/mac__launch_sh.h"
#include "installer-build/mac__com_zerotier_one_plist.h"
#include "installer-build/mac_ui__contents_info_plist.h"
#include "installer-build/mac_ui__contents_macos_zerotier_one.h"
#include "installer-build/mac_ui__contents_pkginfo.h"
#include "installer-build/mac_ui__contents_resources_empty_lproj.h"
#include "installer-build/mac_ui__contents_resources_zt1icon_icns.h"
#endif

/* Windows Tap device drivers for x86 and x64 (installer will be x86) */
#ifdef __WINDOWS__
#include "installer-build/tap_windows__x64__ztTap100_sys.h"
#include "installer-build/tap_windows__x64__ztTap100_inf.h"
#include "installer-build/tap_windows__x86__ztTap100_sys.h"
#include "installer-build/tap_windows__x86__ztTap100_inf.h"
#include "installer-build/tap_windows__devcon32_exe.h"
#include "installer-build/tap_windows__devcon64_exe.h"
#endif

/***************************************************************************/

static unsigned char *_unlz4(const void *lz4,int decompressedLen)
{
	unsigned char *buf;

	if (!decompressedLen)
		return (unsigned char *)0;
	buf = (unsigned char *)malloc(decompressedLen);
	if (!buf)
		return (unsigned char *)0;
	if (LZ4_decompress_fast((const char *)lz4,(char *)buf,decompressedLen) <= 0) {
		free(buf);
		return (unsigned char *)0;
	}
	return buf;
}

static int _putBlob(const void *lz4,int decompressedLen,const char *path,int executable,int protect,int preserveOwnership)
{
	unsigned char *data = _unlz4(lz4,decompressedLen);
	if ((!data)&&(decompressedLen))
		return 0;

#ifdef __WINDOWS__
	DeleteFileA(path);
#else
	struct stat oldModes;
	int hasOldModes = ((stat(path,&oldModes) == 0) ? 1 : 0);
	unlink(path);
#endif

	FILE *f = fopen(path,"wb");
	if (!f) {
		free(data);
		return 0;
	}

	if ((decompressedLen)&&(fwrite(data,decompressedLen,1,f) != 1)) {
		fclose(f);
		free(data);
#ifdef __WINDOWS__
		DeleteFileA(path);
#else
		unlink(path);
#endif
		return 0;
	}

	fclose(f);

#ifdef __WINDOWS__
	/* TODO: Windows exec/prot/etc. */
#else
	if (executable) {
		if (protect)
			chmod(path,0700);
		else chmod(path,0755);
	} else {
		if (protect)
			chmod(path,0600);
		else chmod(path,0644);
	}
	if (preserveOwnership&&hasOldModes)
		chown(path,oldModes.st_uid,oldModes.st_gid);
	else chown(path,0,0);
#endif

	free(data);
	return 1;
}

#define putBlob(name,path,exec,prot,pres) _putBlob((name),(name##_UNCOMPRESSED_LEN),(path),(exec),(prot),(pres))

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
	/* Installer/updater for Unix-like systems (including Mac) */
#ifdef __UNIX_LIKE__ /******************************************************/

	char buf[4096];

	if (getuid() != 0) {
		printf("! ZeroTier One installer must be run as root.\n");
		return 2;
	}

	printf("# ZeroTier One installer/updater starting...\n");

	/* Create home folder */
	const char *zthome;
#ifdef __APPLE__
	mkdir("/Library/Application Support/ZeroTier",0755);
	chmod("/Library/Application Support/ZeroTier",0755);
	chown("/Library/Application Support/ZeroTier",0,0);
	printf("mkdir /Library/Application Support/ZeroTier\n");
	mkdir(zthome = "/Library/Application Support/ZeroTier/One",0755);
#else /* not Apple */
	mkdir("/var/lib",0755);
	printf("mkdir /var/lib\n");
	mkdir(zthome = "/var/lib/zerotier-one",0755);
#endif
	chmod(zthome,0755);
	chown(zthome,0,0);
	printf("mkdir %s\n",zthome);

	/* Write main ZT1 binary */
	sprintf(buf,"%s/zerotier-one",zthome);
	if (!putBlob(zerotier_one,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);

	/* Create command line interface symlink */
	unlink("/usr/bin/zerotier-cli");
	symlink(buf,"/usr/bin/zerotier-cli");
	printf("link %s /usr/bin/zerotier-cli\n",buf);

	/* Write uninstall script into home folder */
	sprintf(buf,"%s/uninstall.sh",zthome);
	if (!putBlob(uninstall_sh,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);

#ifdef __APPLE__
	/* Write launcher script for Mac */
	sprintf(buf,"%s/launch.sh",zthome);
	if (!putBlob(mac__launch_sh,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);

	/* Add ZT1 task to Apple's launchd */
	sprintf(buf,"/Library/LaunchDaemons/com.zerotier.one.plist");
	if (!putBlob(mac__com_zerotier_one_plist,buf,0,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);

	/* Write tap.kext into home folder */
	sprintf(buf,"%s/tap.kext",zthome);
	mkdir(buf,0755); chmod(buf,0755); chown(buf,0,0); printf("mkdir %s\n",buf);
	sprintf(buf,"%s/tap.kext/Contents",zthome);
	mkdir(buf,0755); chmod(buf,0755); chown(buf,0,0); printf("mkdir %s\n",buf);
	sprintf(buf,"%s/tap.kext/Contents/MacOS",zthome);
	mkdir(buf,0755); chmod(buf,0755); chown(buf,0,0); printf("mkdir %s\n",buf);
	sprintf(buf,"%s/tap.kext/Contents/Info.plist",zthome);
	if (!putBlob(tap_mac__Info_plist,buf,0,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);
	sprintf(buf,"%s/tap.kext/Contents/MacOS/tap",zthome);
	if (!putBlob(tap_mac__tap,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);

	/* Write or update GUI application into /Applications */
	sprintf(buf,"/Applications/ZeroTier One.app");
	mkdir(buf,0755); chmod(buf,0755); chown(buf,0,0); printf("mkdir %s\n",buf);
	sprintf(buf,"/Applications/ZeroTier One.app/Contents");
	mkdir(buf,0755); chmod(buf,0755); chown(buf,0,0); printf("mkdir %s\n",buf);
	sprintf(buf,"/Applications/ZeroTier One.app/Contents/MacOS");
	mkdir(buf,0755); chmod(buf,0755); chown(buf,0,0); printf("mkdir %s\n",buf);
	sprintf(buf,"/Applications/ZeroTier One.app/Contents/Resources");
	mkdir(buf,0755); chmod(buf,0755); chown(buf,0,0); printf("mkdir %s\n",buf);
	sprintf(buf,"/Applications/ZeroTier One.app/Contents/Info.plist");
	if (!putBlob(mac_ui__contents_info_plist,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);
	sprintf(buf,"/Applications/ZeroTier One.app/Contents/MacOS/ZeroTier One");
	if (!putBlob(mac_ui__contents_macos_zerotier_one,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);
	sprintf(buf,"/Applications/ZeroTier One.app/Contents/PkgInfo");
	if (!putBlob(mac_ui__contents_pkginfo,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);
	sprintf(buf,"/Applications/ZeroTier One.app/Contents/Resources/empty.lproj");
	if (!putBlob(mac_ui__contents_resources_empty_lproj,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);
	sprintf(buf,"/Applications/ZeroTier One.app/Contents/Resources/zt1icon.icns");
	if (!putBlob(mac_ui__contents_resources_zt1icon_icns,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);

	/* Load script into running launchd, start ZeroTier One */
	printf("exec launchctl load /Library/LaunchDaemons/com.zerotier.one.plist: "); fflush(stdout);
	long launchctlpid = (long)vfork();
	if (launchctlpid == 0) {
		execl("/bin/launchctl","/bin/launchctl","load","/Library/LaunchDaemons/com.zerotier.one.plist",(char *)0);
		execlp("launchctl","launchctl","load","/Library/LaunchDaemons/com.zerotier.one.plist",(char *)0); /* reached if execl() with full path fails */
		exit(0); /* never reached on successful execl() or execlp() */
	}
	if (launchctlpid > 0) {
		int exitcode = 0;
		waitpid(launchctlpid,&exitcode,0);
	}
	printf("installed.\n");
#endif /* __APPLE__ */

#ifdef __LINUX__
	/* Write Linux init script */
	sprintf(buf,"/etc/init.d/zerotier-one");
	if (!putBlob(linux__init_d__zerotier_one,buf,1,0,0)) { printf("! unable to write %s\n",buf); return 1; } printf("write %s\n",buf);

	/* Erase any previous startup/shutdown links */
	for(int rl=0;rl<=6;++rl) {
		sprintf(buf,"/etc/rc%d.d",rl);
		DIR *rcd = opendir(buf);
		if (rcd) {
			struct dirent *d;
			while ((d = readdir(rcd))) {
				if (strstr(d->d_name,"zerotier-one")) {
					sprintf(buf,"/etc/rc%d.d/%s",rl,d->d_name);
					unlink(buf);
					printf("rm %s\n",buf);
				}
			}
			closedir(rcd);
		}
	}

	/* Link init script to all the proper places for startup/shutdown */
	symlink("/etc/init.d/zerotier-one","/etc/rc0.d/K89zerotier-one");
	printf("link /etc/init.d/zerotier-one /etc/rc0.d/K89zerotier-one\n");
	symlink("/etc/init.d/zerotier-one","/etc/rc1.d/K89zerotier-one");
	printf("link /etc/init.d/zerotier-one /etc/rc1.d/K89zerotier-one\n");
	symlink("/etc/init.d/zerotier-one","/etc/rc2.d/S11zerotier-one");
	printf("link /etc/init.d/zerotier-one /etc/rc2.d/S11zerotier-one\n");
	symlink("/etc/init.d/zerotier-one","/etc/rc3.d/S11zerotier-one");
	printf("link /etc/init.d/zerotier-one /etc/rc3.d/S11zerotier-one\n");
	symlink("/etc/init.d/zerotier-one","/etc/rc4.d/S11zerotier-one");
	printf("link /etc/init.d/zerotier-one /etc/rc4.d/S11zerotier-one\n");
	symlink("/etc/init.d/zerotier-one","/etc/rc5.d/S11zerotier-one");
	printf("link /etc/init.d/zerotier-one /etc/rc5.d/S11zerotier-one\n");
	symlink("/etc/init.d/zerotier-one","/etc/rc6.d/K89zerotier-one");
	printf("link /etc/init.d/zerotier-one /etc/rc6.d/K89zerotier-one\n");
#endif /* __LINUX__ */

	printf("# Done!\n");

	/* -s causes this to exec() itself to ZeroTier One after install/update */
	if ((argc > 1)&&(!strcmp(argv[1],"-s"))) {
		sprintf(buf,"%s/zerotier-one",zthome);
		printf("# -s specified, proceeding to exec(%s)\n",zthome);
		execl(buf,buf,(char *)0);
		return 3; /* never reached on successful execl() */
	}

#endif /* __UNIX_LIKE__ ****************************************************/

	/* Installer/updater for Windows systems */
#ifdef __WINDOWS__ /********************************************************/

#endif /* __WINDOWS__ ******************************************************/

	return 0;
}
