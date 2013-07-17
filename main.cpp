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
#include <time.h>
#include <errno.h>
#include <string>
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#endif

#include "node/Node.hpp"
#include "node/Utils.hpp"

#include "launcher.h"

using namespace ZeroTier;

static Node *node = (Node *)0;

static void printHelp(const char *cn,FILE *out)
{
	fprintf(out,"ZeroTier One version %d.%d.%d\n(c)2012-2013 ZeroTier Networks LLC\nLicensed under the GNU General Public License v3\n\nUsage: %s <home directory>\n",Node::versionMajor(),Node::versionMinor(),Node::versionRevision(),cn);
}

#ifndef _WIN32
static void sighandlerQuit(int sig)
{
	Node *n = node;
	if (n)
		n->terminate();
	else exit(0);
}
#endif

int main(int argc,char **argv)
{
#ifndef _WIN32
	signal(SIGHUP,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGALRM,SIG_IGN);
	signal(SIGINT,&sighandlerQuit);
	signal(SIGTERM,&sighandlerQuit);
	signal(SIGQUIT,&sighandlerQuit);
#endif

	if (argc < 2) {
		printHelp(argv[0],stderr);
		return ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION;
	}

	const char *homeDir = (const char *)0;
	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				default:
					printHelp(argv[0],stderr);
					return ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION;
			}
		} else {
			if (homeDir) {
				printHelp(argv[0],stderr);
				return ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION;
			}
			homeDir = argv[i];
			break;
		}
	}

	if ((!homeDir)||(strlen(homeDir) <= 0)) {
		printHelp(argv[0],stderr);
		return ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION;
	}
#ifndef _WIN32
	mkdir(homeDir,0755); // will fail if it already exists
#endif

	int exitCode = ZT_EXEC_RETURN_VALUE_NORMAL_TERMINATION;

	node = new Node(homeDir);
	const char *termReason = (char *)0;
	switch(node->run()) {
		case Node::NODE_RESTART_FOR_RECONFIGURATION:
			exitCode = ZT_EXEC_RETURN_VALUE_PLEASE_RESTART;
			break;
		case Node::NODE_UNRECOVERABLE_ERROR:
			exitCode = ZT_EXEC_RETURN_VALUE_UNRECOVERABLE_ERROR;
			termReason = node->reasonForTermination();
			fprintf(stderr,"%s: abnormal termination: %s\n",argv[0],(termReason) ? termReason : "(unknown reason)");
			break;
		case Node::NODE_NEW_VERSION_AVAILABLE:
			exitCode = ZT_EXEC_RETURN_VALUE_TERMINATED_FOR_UPGRADE;
			break;
		default:
			break;
	}
	delete node;
	node = (Node *)0;

	return exitCode;
}
