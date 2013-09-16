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

#include "node/Constants.hpp"

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

#include "node/Constants.hpp"
#include "node/Defaults.hpp"
#include "node/Utils.hpp"
#include "node/Node.hpp"

using namespace ZeroTier;

static Node *node = (Node *)0;

static void printHelp(const char *cn,FILE *out)
{
	fprintf(out,"ZeroTier One version %d.%d.%d"ZT_EOL_S"(c)2012-2013 ZeroTier Networks LLC"ZT_EOL_S"Licensed under the GNU General Public License v3"ZT_EOL_S""ZT_EOL_S"Usage: %s [home directory]"ZT_EOL_S,Node::versionMajor(),Node::versionMinor(),Node::versionRevision(),cn);
}

#ifdef __UNIX_LIKE__
static void sighandlerQuit(int sig)
{
	Node *n = node;
	if (n)
		n->terminate(Node::NODE_NORMAL_TERMINATION,"terminated by signal");
	else exit(0);
}
#endif

#ifdef __WINDOWS__
static BOOL WINAPI _handlerRoutine(DWORD dwCtrlType)
{
	switch(dwCtrlType) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			Node *n = node;
			if (n)
				n->terminate(Node::NODE_NORMAL_TERMINATION,"terminated by signal");
			return TRUE;
	}
	return FALSE;
}
#endif

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
#ifdef __UNIX_LIKE__
	signal(SIGHUP,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGALRM,SIG_IGN);
	signal(SIGINT,&sighandlerQuit);
	signal(SIGTERM,&sighandlerQuit);
	signal(SIGQUIT,&sighandlerQuit);
#endif

#ifdef __WINDOWS__
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2),&wsaData);
	SetConsoleCtrlHandler(&_handlerRoutine,TRUE);
#endif

	const char *homeDir = (const char *)0;
	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'h':
				case '?':
				default:
					printHelp(argv[0],stderr);
					return 0;
			}
		} else {
			if (homeDir) {
				printHelp(argv[0],stderr);
				return 0;
			}
			homeDir = argv[i];
			break;
		}
	}

	if ((!homeDir)||(strlen(homeDir) == 0))
		homeDir = ZT_DEFAULTS.defaultHomePath.c_str();

#ifdef __UNIX_LIKE__
	mkdir(homeDir,0755); // will fail if it already exists
#endif

	int exitCode = 0;

	node = new Node(homeDir);
	const char *termReason = (char *)0;
	switch(node->run()) {
		case Node::NODE_UNRECOVERABLE_ERROR:
			exitCode = -1;
			termReason = node->reasonForTermination();
			fprintf(stderr,"%s: abnormal termination: %s\n",argv[0],(termReason) ? termReason : "(unknown reason)");
			break;
		default:
			break;
	}
	delete node;
	node = (Node *)0;

	return exitCode;
}
