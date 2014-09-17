/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

// Uncomment on Windows to assume -C and run in console instead of service
// Useful for Visual Studio debugging (launch VS as Administrator to run)
//#define ZT_WIN_RUN_IN_CONSOLE

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
#include <lmcons.h>
#include <newdev.h>
#include <atlbase.h>
#include "windows/ZeroTierOne/ServiceInstaller.h"
#include "windows/ZeroTierOne/ServiceBase.h"
#include "windows/ZeroTierOne/ZeroTierOneService.h"
#else
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#endif

#include "node/Constants.hpp"

#include "node/Defaults.hpp"
#include "node/Utils.hpp"
#include "node/Node.hpp"
#include "node/C25519.hpp"
#include "node/Identity.hpp"
#include "node/Thread.hpp"
#include "node/CertificateOfMembership.hpp"
#include "node/EthernetTapFactory.hpp"
#include "node/RoutingTable.hpp"

#include "control/NodeControlClient.hpp"
#include "control/NodeControlService.hpp"

#ifdef __WINDOWS__
#include "osnet/WindowsEthernetTapFactory.hpp"
#include "osnet/WindowsRoutingTable.hpp"
#define ZTCreatePlatformEthernetTapFactory (new WindowsEthernetTapFactory(homeDir))
#define ZTCreatePlatformRoutingTable (new WindowsRoutingTable())
#endif

#ifdef __LINUX__
#include "osnet/LinuxEthernetTapFactory.hpp"
#include "osnet/LinuxRoutingTable.hpp"
#define ZTCreatePlatformEthernetTapFactory (new LinuxEthernetTapFactory())
#define ZTCreatePlatformRoutingTable (new LinuxRoutingTable())
#endif

#ifdef __APPLE__
#include "osnet/OSXEthernetTapFactory.hpp"
#include "osnet/BSDRoutingTable.hpp"
#define ZTCreatePlatformEthernetTapFactory (new OSXEthernetTapFactory(homeDir,"tap.kext"))
#define ZTCreatePlatformRoutingTable (new BSDRoutingTable())
#endif

#ifndef ZTCreatePlatformEthernetTapFactory
#error Sorry, this platform has no osnet/ implementation yet. Fork me on GitHub and add one?
#endif

using namespace ZeroTier;

static Node *node = (Node *)0;

namespace ZeroTierCLI { // ---------------------------------------------------

static void printHelp(FILE *out,const char *cn)
{
	fprintf(out,"Usage: %s <command>   (use 'help' for help)"ZT_EOL_S,cn);
}

static void _CBresultHandler(void *arg,const char *line)
{
	if (line) {
		if ((line[0] == '.')&&(line[1] == (char)0)) {
			fflush(stdout);
			*((bool *)arg) = true;
		} else fprintf(stdout,"%s"ZT_EOL_S,line);
	}
}

#ifdef __WINDOWS__
static int main(const char *homeDir,int argc,_TCHAR* argv[])
#else
static int main(const char *homeDir,int argc,char **argv)
#endif
{
	if (argc < 2) {
		printHelp(stdout,argv[0]);
		return 1;
	}

	std::string query;
	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'q': // ignore -q since it's used to invoke this
					break;
				case 'h':
				default:
					printHelp(stdout,argv[0]);
					return 1;
			}
		} else {
			if (query.length())
				query.push_back(' ');
			query.append(argv[i]);
		}
	}
	if (!query.length()) {
		printHelp(stdout,argv[0]);
		return 1;
	}

	if (!homeDir)
		homeDir = ZT_DEFAULTS.defaultHomePath.c_str();

	try {
		std::string buf;
		if (!Utils::readFile((std::string(homeDir) + ZT_PATH_SEPARATOR_S + "identity.public").c_str(),buf)) {
			fprintf(stderr,"%s: fatal error: unable to read node address from identity.public in home path"ZT_EOL_S,argv[0]);
			return 1;
		}
		Identity id;
		if (!id.fromString(buf)) {
			fprintf(stderr,"%s: fatal error: unable to read node address from identity.public in home path"ZT_EOL_S,argv[0]);
			return 1;
		}

		std::string authToken(NodeControlClient::getAuthToken((std::string(homeDir) + ZT_PATH_SEPARATOR_S + "authtoken.secret").c_str(),false));
		if (!authToken.length())
			authToken = NodeControlClient::getAuthToken(NodeControlClient::authTokenDefaultUserPath(),false);
		if (!authToken.length()) {
			fprintf(stderr,"%s: fatal error: unable to read authentication token from home path or user home"ZT_EOL_S,argv[0]);
			return 1;
		}

		volatile bool done = false;
		NodeControlClient client((std::string(ZT_IPC_ENDPOINT_BASE) + id.address().toString()).c_str(),authToken.c_str(),&_CBresultHandler,(void *)&done);
		const char *err = client.error();
		if (err) {
			fprintf(stderr,"%s: fatal error: unable to connect (is ZeroTier One running?) (%s)"ZT_EOL_S,argv[0],err);
			return 1;
		}
		client.send(query.c_str());
		while (!done) Thread::sleep(100); // dis be ghetto
	} catch (std::exception &exc) {
		fprintf(stderr,"%s: fatal error: unable to connect (is ZeroTier One running?) (%s)"ZT_EOL_S,argv[0],exc.what());
		return 1;
	} catch ( ... ) {
		fprintf(stderr,"%s: fatal error: unable to connect (is ZeroTier One running?) (unknown exception)"ZT_EOL_S,argv[0]);
		return 1;
	}

	return 0;
}

} // namespace ZeroTierCLI ---------------------------------------------------

namespace ZeroTierIdTool { // ------------------------------------------------

static void printHelp(FILE *out,const char *pn)
{
	fprintf(out,"Usage: %s <command> [<args>]"ZT_EOL_S""ZT_EOL_S"Commands:"ZT_EOL_S,pn);
	fprintf(out,"  generate [<identity.secret>] [<identity.public>]"ZT_EOL_S);
	fprintf(out,"  validate <identity.secret/public>"ZT_EOL_S);
	fprintf(out,"  getpublic <identity.secret>"ZT_EOL_S);
	fprintf(out,"  sign <identity.secret> <file>"ZT_EOL_S);
	fprintf(out,"  verify <identity.secret/public> <file> <signature>"ZT_EOL_S);
	fprintf(out,"  mkcom <identity.secret> [<id,value,maxDelta> ...] (hexadecimal integers)"ZT_EOL_S);
}

static Identity getIdFromArg(char *arg)
{
	Identity id;
	if ((strlen(arg) > 32)&&(arg[10] == ':')) { // identity is a literal on the command line
		if (id.fromString(arg))
			return id;
	} else { // identity is to be read from a file
		std::string idser;
		if (Utils::readFile(arg,idser)) {
			if (id.fromString(idser))
				return id;
		}
	}
	return Identity();
}

#ifdef __WINDOWS__
static int main(int argc,_TCHAR* argv[])
#else
static int main(int argc,char **argv)
#endif
{
	if (argc < 2) {
		printHelp(stdout,argv[0]);
		return 1;
	}

	if (!strcmp(argv[1],"generate")) {
		Identity id;
		id.generate();
		std::string idser = id.toString(true);
		if (argc >= 3) {
			if (!Utils::writeFile(argv[2],idser)) {
				fprintf(stderr,"Error writing to %s"ZT_EOL_S,argv[2]);
				return 1;
			} else printf("%s written"ZT_EOL_S,argv[2]);
			if (argc >= 4) {
				idser = id.toString(false);
				if (!Utils::writeFile(argv[3],idser)) {
					fprintf(stderr,"Error writing to %s"ZT_EOL_S,argv[3]);
					return 1;
				} else printf("%s written"ZT_EOL_S,argv[3]);
			}
		} else printf("%s",idser.c_str());
	} else if (!strcmp(argv[1],"validate")) {
		if (argc < 3) {
			printHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s"ZT_EOL_S,argv[2]);
			return 1;
		}

		if (!id.locallyValidate()) {
			fprintf(stderr,"%s FAILED validation."ZT_EOL_S,argv[2]);
			return 1;
		} else printf("%s is a valid identity"ZT_EOL_S,argv[2]);
	} else if (!strcmp(argv[1],"getpublic")) {
		if (argc < 3) {
			printHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s"ZT_EOL_S,argv[2]);
			return 1;
		}

		printf("%s",id.toString(false).c_str());
	} else if (!strcmp(argv[1],"sign")) {
		if (argc < 4) {
			printHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s"ZT_EOL_S,argv[2]);
			return 1;
		}

		if (!id.hasPrivate()) {
			fprintf(stderr,"%s does not contain a private key (must use private to sign)"ZT_EOL_S,argv[2]);
			return 1;
		}

		std::string inf;
		if (!Utils::readFile(argv[3],inf)) {
			fprintf(stderr,"%s is not readable"ZT_EOL_S,argv[3]);
			return 1;
		}
		C25519::Signature signature = id.sign(inf.data(),(unsigned int)inf.length());
		printf("%s",Utils::hex(signature.data,(unsigned int)signature.size()).c_str());
	} else if (!strcmp(argv[1],"verify")) {
		if (argc < 4) {
			printHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s"ZT_EOL_S,argv[2]);
			return 1;
		}

		std::string inf;
		if (!Utils::readFile(argv[3],inf)) {
			fprintf(stderr,"%s is not readable"ZT_EOL_S,argv[3]);
			return 1;
		}

		std::string signature(Utils::unhex(argv[4]));
		if ((signature.length() > ZT_ADDRESS_LENGTH)&&(id.verify(inf.data(),(unsigned int)inf.length(),signature.data(),(unsigned int)signature.length()))) {
			printf("%s signature valid"ZT_EOL_S,argv[3]);
		} else {
			fprintf(stderr,"%s signature check FAILED"ZT_EOL_S,argv[3]);
			return 1;
		}
	} else if (!strcmp(argv[1],"mkcom")) {
		if (argc < 3) {
			printHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if ((!id)||(!id.hasPrivate())) {
			fprintf(stderr,"Identity argument invalid, does not include private key, or file unreadable: %s"ZT_EOL_S,argv[2]);
			return 1;
		}

		CertificateOfMembership com;
		for(int a=3;a<argc;++a) {
			std::vector<std::string> params(Utils::split(argv[a],",","",""));
			if (params.size() == 3) {
				uint64_t qId = Utils::hexStrToU64(params[0].c_str());
				uint64_t qValue = Utils::hexStrToU64(params[1].c_str());
				uint64_t qMaxDelta = Utils::hexStrToU64(params[2].c_str());
				com.setQualifier(qId,qValue,qMaxDelta);
			}
		}
		if (!com.sign(id)) {
			fprintf(stderr,"Signature of certificate of membership failed."ZT_EOL_S);
			return 1;
		}

		printf("%s",com.toString().c_str());
	} else {
		printHelp(stdout,argv[0]);
		return 1;
	}

	return 0;
}

} // namespace ZeroTierIdTool ------------------------------------------------

#ifdef __UNIX_LIKE__
static void sighandlerHup(int sig)
{
	Node *n = node;
	if (n)
		n->resync();
}
static void sighandlerQuit(int sig)
{
	Node *n = node;
	if (n)
		n->terminate(Node::NODE_NORMAL_TERMINATION,"terminated by signal");
	else exit(0);
}
#endif

#ifdef __WINDOWS__
// Console signal handler routine to allow CTRL+C to work, mostly for testing
static BOOL WINAPI _winConsoleCtrlHandler(DWORD dwCtrlType)
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

// Pokes a hole in the Windows firewall (advfirewall) for the running program
static void _winPokeAHole()
{
	char myPath[MAX_PATH];
	DWORD ps = GetModuleFileNameA(NULL,myPath,sizeof(myPath));
	if ((ps > 0)&&(ps < (DWORD)sizeof(myPath))) {
		STARTUPINFOA startupInfo;
		PROCESS_INFORMATION processInfo;

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\netsh.exe advfirewall firewall delete rule name=\"ZeroTier One\" program=\"") + myPath + "\"").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\netsh.exe advfirewall firewall add rule name=\"ZeroTier One\" dir=in action=allow program=\"") + myPath + "\" enable=yes").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\netsh.exe advfirewall firewall add rule name=\"ZeroTier One\" dir=out action=allow program=\"") + myPath + "\" enable=yes").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
	}
}

// Returns true if this is running as the local administrator
static BOOL IsCurrentUserLocalAdministrator(void)
{
	BOOL   fReturn         = FALSE;
	DWORD  dwStatus;
	DWORD  dwAccessMask;
	DWORD  dwAccessDesired;
	DWORD  dwACLSize;
	DWORD  dwStructureSize = sizeof(PRIVILEGE_SET);
	PACL   pACL            = NULL;
	PSID   psidAdmin       = NULL;

	HANDLE hToken              = NULL;
	HANDLE hImpersonationToken = NULL;

	PRIVILEGE_SET   ps;
	GENERIC_MAPPING GenericMapping;

	PSECURITY_DESCRIPTOR     psdAdmin           = NULL;
	SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;

	const DWORD ACCESS_READ  = 1;
	const DWORD ACCESS_WRITE = 2;

	__try
	{
		if (!OpenThreadToken(GetCurrentThread(), TOKEN_DUPLICATE|TOKEN_QUERY,TRUE,&hToken))
		{
			if (GetLastError() != ERROR_NO_TOKEN)
				__leave;
			if (!OpenProcessToken(GetCurrentProcess(),TOKEN_DUPLICATE|TOKEN_QUERY, &hToken))
				__leave;
		}
		if (!DuplicateToken (hToken, SecurityImpersonation,&hImpersonationToken))
			__leave;
		if (!AllocateAndInitializeSid(&SystemSidAuthority, 2,
			SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS,
			0, 0, 0, 0, 0, 0, &psidAdmin))
			__leave;
		psdAdmin = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
		if (psdAdmin == NULL)
			__leave;
		if (!InitializeSecurityDescriptor(psdAdmin,SECURITY_DESCRIPTOR_REVISION))
			__leave;
		dwACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psidAdmin) - sizeof(DWORD);
		pACL = (PACL)LocalAlloc(LPTR, dwACLSize);
		if (pACL == NULL)
			__leave;
		if (!InitializeAcl(pACL, dwACLSize, ACL_REVISION2))
			__leave;
		dwAccessMask= ACCESS_READ | ACCESS_WRITE;
		if (!AddAccessAllowedAce(pACL, ACL_REVISION2, dwAccessMask, psidAdmin))
			__leave;
		if (!SetSecurityDescriptorDacl(psdAdmin, TRUE, pACL, FALSE))
			__leave;

		SetSecurityDescriptorGroup(psdAdmin, psidAdmin, FALSE);
		SetSecurityDescriptorOwner(psdAdmin, psidAdmin, FALSE);

		if (!IsValidSecurityDescriptor(psdAdmin))
			__leave;
		dwAccessDesired = ACCESS_READ;

		GenericMapping.GenericRead    = ACCESS_READ;
		GenericMapping.GenericWrite   = ACCESS_WRITE;
		GenericMapping.GenericExecute = 0;
		GenericMapping.GenericAll     = ACCESS_READ | ACCESS_WRITE;

		if (!AccessCheck(psdAdmin, hImpersonationToken, dwAccessDesired,
			&GenericMapping, &ps, &dwStructureSize, &dwStatus,
			&fReturn))
		{
			fReturn = FALSE;
			__leave;
		}
	}
	__finally
	{
		// Clean up.
		if (pACL) LocalFree(pACL);
		if (psdAdmin) LocalFree(psdAdmin);
		if (psidAdmin) FreeSid(psidAdmin);
		if (hImpersonationToken) CloseHandle (hImpersonationToken);
		if (hToken) CloseHandle (hToken);
	}

	return fReturn;
}
#endif // __WINDOWS__

// ---------------------------------------------------------------------------

static void printHelp(const char *cn,FILE *out)
{
	fprintf(out,"ZeroTier One version %d.%d.%d"ZT_EOL_S"(c)2011-2014 ZeroTier Networks LLC"ZT_EOL_S,Node::versionMajor(),Node::versionMinor(),Node::versionRevision());
	fprintf(out,"Licensed under the GNU General Public License v3"ZT_EOL_S""ZT_EOL_S);
#ifdef ZT_AUTO_UPDATE
	fprintf(out,"Auto-update enabled build, will update from URL:"ZT_EOL_S);
	fprintf(out,"  %s"ZT_EOL_S,ZT_DEFAULTS.updateLatestNfoURL.c_str());
	fprintf(out,"Update authentication signing authorities: "ZT_EOL_S);
	int no = 0;
	for(std::map< Address,Identity >::const_iterator sa(ZT_DEFAULTS.updateAuthorities.begin());sa!=ZT_DEFAULTS.updateAuthorities.end();++sa) {
		if (no == 0)
			fprintf(out,"  %s",sa->first.toString().c_str());
		else fprintf(out,", %s",sa->first.toString().c_str());
		if (++no == 6) {
			fprintf(out,ZT_EOL_S);
			no = 0;
		}
	}
	fprintf(out,ZT_EOL_S""ZT_EOL_S);
#else
	fprintf(out,"Auto-updates not enabled on this build. You must update manually."ZT_EOL_S""ZT_EOL_S);
#endif
	fprintf(out,"Usage: %s [-switches] [home directory] [-q <query>]"ZT_EOL_S""ZT_EOL_S,cn);
	fprintf(out,"Available switches:"ZT_EOL_S);
	fprintf(out,"  -h                - Display this help"ZT_EOL_S);
	fprintf(out,"  -v                - Show version"ZT_EOL_S);
	fprintf(out,"  -p<port>          - Port for UDP (default: 9993)"ZT_EOL_S);
	fprintf(out,"  -t<port>          - Port for TCP (default: disabled)"ZT_EOL_S);
#ifdef __UNIX_LIKE__
	fprintf(out,"  -d                - Fork and run as daemon (Unix-ish OSes)"ZT_EOL_S);
#endif
	fprintf(out,"  -q                - Send a query to a running service (zerotier-cli)"ZT_EOL_S);
	fprintf(out,"  -i                - Generate and manage identities (zerotier-idtool)"ZT_EOL_S);
#ifdef __WINDOWS__
	fprintf(out,"  -C                - Run from command line instead of as service (Windows)"ZT_EOL_S);
	fprintf(out,"  -I                - Install Windows service (Windows)"ZT_EOL_S);
	fprintf(out,"  -R                - Uninstall Windows service (Windows)"ZT_EOL_S);
	fprintf(out,"  -D                - Load tap driver into system driver store (Windows)"ZT_EOL_S);
#endif
}

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
#ifdef __UNIX_LIKE__
	signal(SIGHUP,&sighandlerHup);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGALRM,SIG_IGN);
	signal(SIGINT,&sighandlerQuit);
	signal(SIGTERM,&sighandlerQuit);
	signal(SIGQUIT,&sighandlerQuit);

	/* Ensure that there are no inherited file descriptors open from a previous
	 * incarnation. This is a hack to ensure that GitHub issue #61 or variants
	 * of it do not return, and should not do anything otherwise bad. */
	{
		int mfd = STDIN_FILENO;
		if (STDOUT_FILENO > mfd) mfd = STDOUT_FILENO;
		if (STDERR_FILENO > mfd) mfd = STDERR_FILENO;
		for(int f=mfd+1;f<1024;++f)
			::close(f);
	}
#endif

#ifdef __WINDOWS__
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2),&wsaData);
#endif

	if ((strstr(argv[0],"zerotier-cli"))||(strstr(argv[0],"ZEROTIER-CLI")))
		return ZeroTierCLI::main((const char *)0,argc,argv);
	if ((strstr(argv[0],"zerotier-idtool"))||(strstr(argv[0],"ZEROTIER-IDTOOL")))
		return ZeroTierIdTool::main(argc,argv);

	const char *homeDir = (const char *)0;
	unsigned int udpPort = ZT_DEFAULT_UDP_PORT;
	unsigned int tcpPort = 0;

#ifdef __UNIX_LIKE__
	bool runAsDaemon = false;
#endif
#ifdef __WINDOWS__
#ifdef ZT_WIN_RUN_IN_CONSOLE
	bool winRunFromCommandLine = true;
#else
	bool winRunFromCommandLine = false;
#endif
#endif // __WINDOWS__

	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'p':
					udpPort = Utils::strToUInt(argv[i] + 2);
					if (udpPort > 65535) {
						printHelp(argv[0],stdout);
						return 1;
					}
					break;
				case 't':
					tcpPort = Utils::strToUInt(argv[i] + 2);
					if (tcpPort > 65535) {
						printHelp(argv[0],stdout);
						return 1;
					}
					break;
#ifdef __UNIX_LIKE__
				case 'd':
					runAsDaemon = true;
					break;
#endif
				case 'v':
					printf("%s"ZT_EOL_S,Node::versionString());
					return 0;
				case 'q':
					if (argv[i][2]) {
						printHelp(argv[0],stdout);
						return 0;
					} else return ZeroTierCLI::main(homeDir,argc,argv);
				case 'i':
					if (argv[i][2]) {
						printHelp(argv[0],stdout);
						return 0;
					} else return ZeroTierIdTool::main(argc,argv);
#ifdef __WINDOWS__
				case 'C':
					winRunFromCommandLine = true;
					break;
				case 'I': { // install self as service
						if (IsCurrentUserLocalAdministrator() != TRUE) {
							fprintf(stderr,"%s: must be run as a local administrator."ZT_EOL_S,argv[0]);
							return 1;
						}
						std::string ret(InstallService(ZT_SERVICE_NAME,ZT_SERVICE_DISPLAY_NAME,ZT_SERVICE_START_TYPE,ZT_SERVICE_DEPENDENCIES,ZT_SERVICE_ACCOUNT,ZT_SERVICE_PASSWORD));
						if (ret.length()) {
							fprintf(stderr,"%s: unable to install service: %s"ZT_EOL_S,argv[0],ret.c_str());
							return 3;
						}
						return 0;
					} break;
				case 'R': { // uninstall self as service
						if (IsCurrentUserLocalAdministrator() != TRUE) {
							fprintf(stderr,"%s: must be run as a local administrator."ZT_EOL_S,argv[0]);
							return 1;
						}
						std::string ret(UninstallService(ZT_SERVICE_NAME));
						if (ret.length()) {
							fprintf(stderr,"%s: unable to uninstall service: %s"ZT_EOL_S,argv[0],ret.c_str());
							return 3;
						}
						return 0;
					} break;
				case 'D': { // install Windows driver (since PNPUTIL.EXE seems to be weirdly unreliable)
						std::string pathToInf;
#ifdef _WIN64
						pathToInf = ZT_DEFAULTS.defaultHomePath + "\\tap-windows\\x64\\zttap200.inf";
#else
						pathToInf = ZT_DEFAULTS.defaultHomePath + "\\tap-windows\\x86\\zttap200.inf";
#endif
						printf("Installing ZeroTier One virtual Ethernet port driver."ZT_EOL_S""ZT_EOL_S"NOTE: If you don't see a confirmation window to allow driver installation,"ZT_EOL_S"check to make sure it didn't appear under the installer."ZT_EOL_S);
						BOOL needReboot = FALSE;
						if (DiInstallDriverA(NULL,pathToInf.c_str(),DIIRFLAG_FORCE_INF,&needReboot)) {
							printf("%s: driver successfully installed from %s"ZT_EOL_S,argv[0],pathToInf.c_str());
							return 0;
						} else {
							printf("%s: failed installing %s: %d"ZT_EOL_S,argv[0],pathToInf.c_str(),(int)GetLastError());
							return 3;
						}
					} break;
#endif // __WINDOWS__
				case 'h':
				case '?':
				default:
					printHelp(argv[0],stdout);
					return 0;
			}
		} else {
			if (homeDir) {
				printHelp(argv[0],stdout);
				return 0;
			} else homeDir = argv[i];
		}
	}
	if ((!homeDir)||(strlen(homeDir) == 0))
		homeDir = ZT_DEFAULTS.defaultHomePath.c_str();

#ifdef __UNIX_LIKE__
	if (getuid() != 0) {
		fprintf(stderr,"%s: must be run as root (uid 0)\n",argv[0]);
		return 1;
	}

	if (runAsDaemon) {
		long p = (long)fork();
		if (p < 0) {
			fprintf(stderr,"%s: could not fork"ZT_EOL_S,argv[0]);
			return 1;
		} else if (p > 0)
			return 0; // forked
		// else p == 0, so we are daemonized
	}

	mkdir(homeDir,0755); // will fail if it already exists, but that's fine

	{
		// Write .pid file to home folder
		char pidpath[4096];
		Utils::snprintf(pidpath,sizeof(pidpath),"%s/zerotier-one.pid",homeDir);
		FILE *pf = fopen(pidpath,"w");
		if (pf) {
			fprintf(pf,"%ld",(long)getpid());
			fclose(pf);
		}
	}
#endif // __UNIX_LIKE__

#ifdef __WINDOWS__
	_winPokeAHole();
	if (winRunFromCommandLine) {
		// Running in "interactive" mode (mostly for debugging)
		if (IsCurrentUserLocalAdministrator() != TRUE) {
			fprintf(stderr,"%s: must be run as a local administrator."ZT_EOL_S,argv[0]);
			return 1;
		}
		SetConsoleCtrlHandler(&_winConsoleCtrlHandler,TRUE);
		// continues on to ordinary command line execution code below...
	} else {
		// Running from service manager
		ZeroTierOneService zt1Service;
		if (CServiceBase::Run(zt1Service) == TRUE) {
			return 0;
		} else {
			fprintf(stderr,"%s: unable to start service (try -h for help)"ZT_EOL_S,argv[0]);
			return 1;
		}
	}
#endif // __WINDOWS__

	int exitCode = 0;
	bool needsReset = false;
	EthernetTapFactory *tapFactory = (EthernetTapFactory *)0;
	RoutingTable *routingTable = (RoutingTable *)0;
	NodeControlService *controlService = (NodeControlService *)0;

	try {
		// Get or create authtoken.secret -- note that if this fails, authentication
		// will always fail since an empty auth token won't work. This should always
		// succeed unless something is wrong with the filesystem.
		std::string authToken(NodeControlClient::getAuthToken((std::string(homeDir) + ZT_PATH_SEPARATOR_S + "authtoken.secret").c_str(),true));

		tapFactory = ZTCreatePlatformEthernetTapFactory;
		routingTable = ZTCreatePlatformRoutingTable;

		node = new Node(homeDir,tapFactory,routingTable,udpPort,tcpPort,needsReset);
		controlService = new NodeControlService(node,authToken.c_str());

		switch(node->run()) {
#ifdef __WINDOWS__
			case Node::NODE_RESTART_FOR_UPGRADE: {
				const char *upgPath = node->reasonForTermination();
				if (upgPath) {
					if (!ZeroTierOneService::doStartUpgrade(std::string(upgPath))) {
						exitCode = 3;
						fprintf(stderr,"%s: abnormal termination: unable to execute update at %s (doStartUpgrade failed)\n",argv[0],(upgPath) ? upgPath : "(unknown path)");
					}
				} else {
					exitCode = 3;
					fprintf(stderr,"%s: abnormal termination: unable to execute update at %s (no upgrade path provided)\n",argv[0],(upgPath) ? upgPath : "(unknown path)");
				}
			}	break;
#else // __UNIX_LIKE__
			case Node::NODE_RESTART_FOR_UPGRADE: {
				const char *upgPath = node->terminationMessage();
				// On Unix-type OSes we exec() right into the upgrade. This in turn will
				// end with us being re-launched either via the upgrade itself or something
				// like OSX's launchd.
				if (upgPath) {
					Utils::rm((std::string(homeDir)+"/zerotier-one.pid").c_str());
					std::string updateLogPath(homeDir);
					updateLogPath.append("/autoupdate.log");
					Utils::rm(updateLogPath.c_str());
					Utils::redirectUnixOutputs(updateLogPath.c_str(),(const char *)0);
					::execl(upgPath,upgPath,(char *)0);
				}
				exitCode = 3;
				fprintf(stderr,"%s: abnormal termination: unable to execute update at %s\n",argv[0],(upgPath) ? upgPath : "(unknown path)");
			}	break;
#endif // __WINDOWS__ / __UNIX_LIKE__

			case Node::NODE_UNRECOVERABLE_ERROR: {
				exitCode = 3;
				const char *termReason = node->terminationMessage();
				fprintf(stderr,"%s: abnormal termination: %s\n",argv[0],(termReason) ? termReason : "(unknown reason)");
			}	break;

			default:
				break;
		}
	} catch ( std::exception &exc ) {
		fprintf(stderr,"%s: unexpected exception: %s"ZT_EOL_S,argv[0],exc.what());
		exitCode = 3;
	} catch ( ... ) {
		fprintf(stderr,"%s: unexpected exception: unknown exception"ZT_EOL_S,argv[0]);
		exitCode = 3;
	}

	delete controlService;
	delete node; node = (Node *)0;
	delete routingTable;
	delete tapFactory;

#ifdef __UNIX_LIKE__
	Utils::rm((std::string(homeDir)+"/zerotier-one.pid").c_str());
#endif

	return exitCode;
}
