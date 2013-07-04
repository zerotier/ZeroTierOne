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

/* Launcher for Linux/Unix/Mac */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "launcher.h"

/* Must match first 16 bytes of EMBEDDED_VERSION_STAMP in Node.cpp */
static const unsigned char EMBEDDED_VERSION_STAMP_KEY[16] = { 0x6d,0xfe,0xff,0x01,0x90,0xfa,0x89,0x57,0x88,0xa1,0xaa,0xdc,0xdd,0xde,0xb0,0x33 };

const unsigned char EMBEDDED_LAUNCHER_VERSION_STAMP[20] = {
	0x96,0xf0,0x00,0x08,0x18,0xff,0xc9,0xde,0xad,0xf0,0x0f,0xbe,0xef,0x30,0xce,0xa1, /* key */
	ZT_LAUNCHER_VERSION_MAJOR,
	ZT_LAUNCHER_VERSION_MINOR,
	(unsigned char)(((unsigned int)ZT_LAUNCHER_VERSION_REVISION) & 0xff), /* little-endian */
	(unsigned char)((((unsigned int)ZT_LAUNCHER_VERSION_REVISION) >> 8) & 0xff)
};

#define ZT_BINARY_NAME "zerotier-one"
#define ZT_BINARY_UPDATE_PREFIX "zerotier-one_update."

#define ZT_LAUNCHER_PIDFILE "zerotier-launcher.pid"
#define ZT_ONE_PIDFILE "zerotier-one.pid"

/* Load a file into newly malloc()'ed memory, len set to size */
static unsigned char *loadFile(const char *path,unsigned long *len)
{
	unsigned char *fbuf = (unsigned char *)0;
	FILE *f = fopen(path,"rb");
	if (f) {
		if (!fseek(f,0,SEEK_END)) {
			long l = ftell(f);
			if (l > 0) {
				fseek(f,0,SEEK_SET);
				fbuf = malloc(l);
				if (fbuf) {
					if (fread(fbuf,l,1,f) != 1) {
						free(fbuf);
						fbuf = (unsigned char *)0;
					} else *len = (unsigned long)l;
				}
			}
		}
		fclose(f);
	}
	return fbuf;
}

/* Scans a ZeroTier binary and determines its version from its embedded version code */
static int findVersion(const unsigned char *bin,unsigned long len,unsigned int *major,unsigned int *minor,unsigned int *revision)
{
	unsigned long i;

	if (len > 20) {
		for(i=0;i<(len - 20);++i) {
			if ((bin[i] == EMBEDDED_VERSION_STAMP_KEY[0])&&(!memcmp(bin + i,EMBEDDED_VERSION_STAMP_KEY,16))) {
				*major = bin[i + 16];
				*minor = bin[i + 17];
				*revision = ((unsigned int)bin[i + 18] & 0xff) | (((unsigned int)bin[i + 19] << 8) & 0xff00);
				return 1;
			}
		}
	}

	return 0;
}

/* Scan for updates and, if found, replace the main binary if possible */
static int doUpdateBinaryIfNewer()
{
	long pfxLen = strlen(ZT_BINARY_UPDATE_PREFIX);
	struct dirent dbuf,*d;
	int needUpdate;
	unsigned int major = 0,minor = 0,revision = 0;
	unsigned int existingMajor = 0,existingMinor = 0,existingRevision = 0;
	unsigned long binLen;
	unsigned char *bin;
	char oldname[1024];
	DIR *dir;

	binLen = 0;
	bin = loadFile(ZT_BINARY_NAME,&binLen);
	if (!((bin)&&(binLen)&&(findVersion(bin,binLen,&existingMajor,&existingMinor,&existingRevision)))) {
		if (bin)
			free(bin);
		return 0;
	}
	free(bin);

	dir = opendir(".");
	if (!dir)
		return 0;
	while (!readdir_r(dir,&dbuf,&d)) {
		if (!d) break;
		if (!strncasecmp(d->d_name,ZT_BINARY_UPDATE_PREFIX,pfxLen)) {
			binLen = 0;
			unsigned char *bin = loadFile(d->d_name,&binLen);
			if ((bin)&&(binLen)&&(findVersion(bin,binLen,&major,&minor,&revision))) {
				needUpdate = 0;
				if (major > existingMajor)
					needUpdate = 1;
				else if (major == existingMajor) {
					if (minor > existingMinor)
						needUpdate = 1;
					else if (minor == existingMinor) {
						if (revision > existingRevision)
							needUpdate = 1;
					}
				}
				free(bin);
				if (needUpdate) {
					/* fprintf(stderr,"zerotier-launcher: replacing %s with %s\n",ZT_BINARY_NAME,d->d_name); */
					sprintf(oldname,"%s.OLD",ZT_BINARY_NAME);
					if (!rename(ZT_BINARY_NAME,oldname)) {
						/* fprintf(stderr,"zerotier-launcher: %s -> %s\n",ZT_BINARY_NAME,oldname); */
						if (!rename(d->d_name,ZT_BINARY_NAME)) {
							/* fprintf(stderr,"zerotier-launcher: %s -> %s\nzerotier-launcher: delete %s\n",d->d_name,ZT_BINARY_NAME,oldname); */
							chmod(ZT_BINARY_NAME,0755);
							unlink(oldname);
							return 1;
						}
					}
					break;
				}
			}
			if (bin)
				free(bin);
		}
	}
	closedir(dir);

	return 0;
}

static volatile long childPid = 0;

static void sigRepeater(int sig)
{
	if (childPid > 0)
		kill(childPid,sig);
}

int main(int argc,char **argv)
{
	const char *zerotierHome = ZT_DEFAULT_HOME;
	FILE *pidf;
	int status,exitCode;
	unsigned long timeStart;
	unsigned int numSubTwoSecondRuns;

	/* Pass on certain signals transparently to the subprogram to do with as it will */
	signal(SIGHUP,&sigRepeater);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,&sigRepeater);
	signal(SIGUSR2,&sigRepeater);
	signal(SIGALRM,SIG_IGN);
	signal(SIGURG,SIG_IGN);
	signal(SIGTERM,&sigRepeater);
	signal(SIGQUIT,&sigRepeater);

	if (argc == 2)
		zerotierHome = argv[1];

	if (chdir(zerotierHome)) {
		fprintf(stderr,"%s: fatal error: could not chdir to %s\n",argv[0],zerotierHome);
		return ZT_EXEC_RETURN_VALUE_UNRECOVERABLE_ERROR;
	}

	pidf = fopen(ZT_LAUNCHER_PIDFILE,"w");
	if (pidf) {
		fprintf(pidf,"%d",(int)getpid());
		fclose(pidf);
	}

	numSubTwoSecondRuns = 0;
	exitCode = ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION;

restart_subprogram:
	/* We actually do this on every loop, which is fine. It picks up any
	 * newer versions that are waiting and swaps them out for the current
	 * running binary. */
	doUpdateBinaryIfNewer();

	timeStart = time(0);
	childPid = fork();
	if (childPid < 0) {
		fprintf(stderr,"%s: fatal error: could not fork(): %s\n",argv[0],strerror(errno));
		return ZT_EXEC_RETURN_VALUE_UNRECOVERABLE_ERROR;
	} else if (childPid) {
		pidf = fopen(ZT_ONE_PIDFILE,"w");
		if (pidf) {
			fprintf(pidf,"%d",(int)childPid);
			fclose(pidf);
		}

		status = ZT_EXEC_RETURN_VALUE_NO_BINARY;
wait_for_subprogram_exit:
		if ((long)waitpid(childPid,&status,0) >= 0) {
			if (WIFEXITED(status)) {
				unlink(ZT_ONE_PIDFILE);

				if ((time(0) - timeStart) < 2) {
					/* Terminate abnormally if we appear to be looping in a tight loop
					 * to avoid fork bombing if one exits abnormally without an abnormal
					 * exit code. */
					if (++numSubTwoSecondRuns >= 16) {
						fprintf(stderr,"%s: fatal error: program exiting immediately in infinite loop\n",argv[0]);
						return ZT_EXEC_RETURN_VALUE_UNRECOVERABLE_ERROR;
					}
				}

				switch(WEXITSTATUS(status)) {
					case ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION:
						exitCode = ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION;
						goto exit_launcher;
					case ZT_EXEC_RETURN_VALUE_NO_BINARY:
						fprintf(stderr,"%s: fatal error: binary zerotier-one not found at %s\n",argv[0],zerotierHome);
						exitCode = ZT_EXEC_RETURN_VALUE_UNRECOVERABLE_ERROR;
						goto exit_launcher;
					case ZT_EXEC_RETURN_VALUE_TERMINATED_FOR_UPGRADE:
					case ZT_EXEC_RETURN_VALUE_PLEASE_RESTART:
						goto restart_subprogram;
					default:
						exitCode = status;
						goto exit_launcher;
				}
			}
		} else if (errno != EINTR) {
			fprintf(stderr,"%s: fatal error: waitpid() failed: %s\n",argv[0],strerror(errno));
			exitCode = ZT_EXEC_RETURN_VALUE_UNRECOVERABLE_ERROR;
			goto exit_launcher;
		} else {
			goto wait_for_subprogram_exit;
		}
	} else {
		execl(ZT_BINARY_NAME,ZT_BINARY_NAME,zerotierHome,(char *)0);
		exit(ZT_EXEC_RETURN_VALUE_NO_BINARY); /* only reached if execl succeeds */
	}

exit_launcher:
	unlink(ZT_LAUNCHER_PIDFILE);
	return exitCode;
}
