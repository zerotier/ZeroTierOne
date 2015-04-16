/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
#include <time.h>
#include <errno.h>

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

#include <string>
#include <stdexcept>

#include "version.h"
#include "include/ZeroTierOne.h"
#include "node/Constants.hpp"
#include "node/Identity.hpp"
#include "node/CertificateOfMembership.hpp"
#include "node/Utils.hpp"
#include "node/NetworkController.hpp"
#include "osdep/OSUtils.hpp"
#include "osdep/Http.hpp"
#include "service/OneService.hpp"
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
#include "controller/SqliteNetworkController.hpp"
#endif

#define ZT1_PID_PATH "zerotier-one.pid"
#define ZT1_CONTROLLER_DB_PATH "controller.db"

using namespace ZeroTier;

static OneService *volatile zt1Service = (OneService *)0;

/****************************************************************************/
/* zerotier-cli personality                                                 */
/****************************************************************************/

static void cliPrintHelp(const char *pn,FILE *out)
{
	fprintf(out,"ZeroTier One version %d.%d.%d"ZT_EOL_S"(c)2011-2015 ZeroTier, Inc."ZT_EOL_S,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
	fprintf(out,"Licensed under the GNU General Public License v3"ZT_EOL_S""ZT_EOL_S);
	fprintf(out,"Usage: %s [-switches] <command/path> [<args>]"ZT_EOL_S""ZT_EOL_S,pn);
	fprintf(out,"Available switches:"ZT_EOL_S);
	fprintf(out,"  -h                 - Display this help"ZT_EOL_S);
	fprintf(out,"  -v                 - Show version"ZT_EOL_S);
	fprintf(out,"  -j                 - Display full raw JSON output"ZT_EOL_S);
	fprintf(out,"  -D<path>           - ZeroTier home path for parameter auto-detect"ZT_EOL_S);
	fprintf(out,"  -p<port>           - HTTP port (default: auto)"ZT_EOL_S);
	fprintf(out,"  -T<token>          - Authentication token (default: auto)"ZT_EOL_S);
	fprintf(out,"  -H<ip>             - HTTP IP address (default: 127.0.0.1)"ZT_EOL_S""ZT_EOL_S);
	fprintf(out,"Available commands:"ZT_EOL_S);
	fprintf(out,"  info               - Display status info"ZT_EOL_S);
	fprintf(out,"  listpeers          - List all peers"ZT_EOL_S);
	fprintf(out,"  listnetworks       - List all networks"ZT_EOL_S);
	fprintf(out,"  join <network>     - Join a network"ZT_EOL_S);
	fprintf(out,"  leave <network>    - Leave a network"ZT_EOL_S);
}

static std::string cliFixJsonCRs(const std::string &s)
{
	std::string r;
	for(std::string::const_iterator c(s.begin());c!=s.end();++c) {
		if (*c == '\n')
			r.append(ZT_EOL_S);
		else r.push_back(*c);
	}
	return r;
}

#ifdef __WINDOWS__
static int cli(int argc, _TCHAR* argv[])
#else
static int cli(int argc,char **argv)
#endif
{
	unsigned int port = 0;
	std::string homeDir;
	std::string command;
	std::string arg1;
	std::string authToken;
	std::string ip("127.0.0.1");
	bool json = false;
	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {

				case 'q': // ignore -q used to invoke this personality
					if (argv[i][2]) {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'j':
					if (argv[i][2]) {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					json = true;
					break;

				case 'p': // port for HTTP
					port = Utils::strToUInt(argv[i] + 2);
					if ((port > 0xffff)||(port == 0)) {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'D': // Home path
					if (argv[i][2]) {
						homeDir = argv[i] + 2;
					} else {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'H': // HTTP IP
					if (argv[i][2]) {
						ip = argv[i] + 2;
					} else {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'T': // Override root topology
					if (argv[i][2]) {
						authToken = argv[i] + 2;
					} else {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'v': // Display version
					if (argv[i][2]) {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					printf("%d.%d.%d"ZT_EOL_S,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
					return 0;

				case 'h':
				case '?':
				default:
					cliPrintHelp(argv[0],stdout);
					return 0;
			}
		} else {
			if (command.length())
				arg1 = argv[i];
			else command = argv[i];
		}
	}
	if (!homeDir.length())
		homeDir = OneService::platformDefaultHomePath();

	if ((!port)||(!authToken.length())) {
		if (!homeDir.length()) {
			fprintf(stderr,"%s: missing port or authentication token and no home directory specified to auto-detect"ZT_EOL_S,argv[0]);
			return 2;
		}

		if (!port) {
			std::string portStr;
			OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "zerotier-one.port").c_str(),portStr);
			port = Utils::strToUInt(portStr.c_str());
			if ((port == 0)||(port > 0xffff)) {
				fprintf(stderr,"%s: missing port and zerotier-one.port not found in %s"ZT_EOL_S,argv[0],homeDir.c_str());
				return 2;
			}
		}

		if (!authToken.length()) {
			OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "authtoken.secret").c_str(),authToken);
			if (!authToken.length()) {
				fprintf(stderr,"%s: missing authentication token and authtoken.secret not found (or readable) in %s"ZT_EOL_S,argv[0],homeDir.c_str());
				return 2;
			}
		}
	}

	InetAddress addr;
	{
		char addrtmp[256];
		Utils::snprintf(addrtmp,sizeof(addrtmp),"%s/%u",ip.c_str(),port);
		addr = InetAddress(addrtmp);
	}

	std::map<std::string,std::string> requestHeaders;
	std::map<std::string,std::string> responseHeaders;
	std::string responseBody;

	requestHeaders["X-ZT1-Auth"] = authToken;

	if ((command == "info")||(command == "status")) {
		unsigned int scode = Http::GET(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			"/status",
			requestHeaders,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			if (json) {
				printf("%s",cliFixJsonCRs(responseBody).c_str());
				return 0;
			} else {
			}
		} else {
			printf("%u %s %s"ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "listpeers") {
		unsigned int scode = Http::GET(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			"/peer",
			requestHeaders,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			if (json) {
				printf("%s",cliFixJsonCRs(responseBody).c_str());
				return 0;
			} else {
			}
		} else {
			printf("%u %s %s"ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "listnetworks") {
		unsigned int scode = Http::GET(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			"/network",
			requestHeaders,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			if (json) {
				printf("%s",cliFixJsonCRs(responseBody).c_str());
				return 0;
			} else {
			}
		} else {
			printf("%u %s %s"ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "join") {
		if (arg1.length() != 16) {
			cliPrintHelp(argv[0],stderr);
			return 2;
		}
		unsigned int scode = Http::POST(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			(std::string("/network/") + arg1).c_str(),
			requestHeaders,
			"",
			0,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			if (json) {
				printf("%s",cliFixJsonCRs(responseBody).c_str());
			} else {
			}
			return 0;
		} else {
			printf("%u %s %s"ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "leave") {
		if (arg1.length() != 16) {
			cliPrintHelp(argv[0],stderr);
			return 2;
		}
		unsigned int scode = Http::DELETE(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			(std::string("/network/") + arg1).c_str(),
			requestHeaders,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			if (json) {
				printf("%s",cliFixJsonCRs(responseBody).c_str());
			} else {
				printf("200 leave OK"ZT_EOL_S);
			}
			return 0;
		} else {
			printf("%u %s %s"ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else {
		cliPrintHelp(argv[0],stderr);
		return 0;
	}
}

/****************************************************************************/
/* zerotier-idtool personality                                              */
/****************************************************************************/

static void idtoolPrintHelp(FILE *out,const char *pn)
{
	fprintf(out,"ZeroTier One version %d.%d.%d"ZT_EOL_S"(c)2011-2015 ZeroTier, Inc."ZT_EOL_S,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
	fprintf(out,"Licensed under the GNU General Public License v3"ZT_EOL_S""ZT_EOL_S);
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
		if (OSUtils::readFile(arg,idser)) {
			if (id.fromString(idser))
				return id;
		}
	}
	return Identity();
}

#ifdef __WINDOWS__
static int idtool(int argc, _TCHAR* argv[])
#else
static int idtool(int argc,char **argv)
#endif
{
	if (argc < 2) {
		idtoolPrintHelp(stdout,argv[0]);
		return 1;
	}

	if (!strcmp(argv[1],"generate")) {
		Identity id;
		id.generate();
		std::string idser = id.toString(true);
		if (argc >= 3) {
			if (!OSUtils::writeFile(argv[2],idser)) {
				fprintf(stderr,"Error writing to %s"ZT_EOL_S,argv[2]);
				return 1;
			} else printf("%s written"ZT_EOL_S,argv[2]);
			if (argc >= 4) {
				idser = id.toString(false);
				if (!OSUtils::writeFile(argv[3],idser)) {
					fprintf(stderr,"Error writing to %s"ZT_EOL_S,argv[3]);
					return 1;
				} else printf("%s written"ZT_EOL_S,argv[3]);
			}
		} else printf("%s",idser.c_str());
	} else if (!strcmp(argv[1],"validate")) {
		if (argc < 3) {
			idtoolPrintHelp(stdout,argv[0]);
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
			idtoolPrintHelp(stdout,argv[0]);
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
			idtoolPrintHelp(stdout,argv[0]);
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
		if (!OSUtils::readFile(argv[3],inf)) {
			fprintf(stderr,"%s is not readable"ZT_EOL_S,argv[3]);
			return 1;
		}
		C25519::Signature signature = id.sign(inf.data(),(unsigned int)inf.length());
		printf("%s",Utils::hex(signature.data,(unsigned int)signature.size()).c_str());
	} else if (!strcmp(argv[1],"verify")) {
		if (argc < 4) {
			idtoolPrintHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s"ZT_EOL_S,argv[2]);
			return 1;
		}

		std::string inf;
		if (!OSUtils::readFile(argv[3],inf)) {
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
			idtoolPrintHelp(stdout,argv[0]);
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
		idtoolPrintHelp(stdout,argv[0]);
		return 1;
	}

	return 0;
}

/****************************************************************************/
/* Unix helper functions and signal handlers                                */
/****************************************************************************/

#ifdef __UNIX_LIKE__
static void _sighandlerHup(int sig)
{
}
static void _sighandlerQuit(int sig)
{
	OneService *s = zt1Service;
	if (s)
		s->terminate();
	else exit(0);
}
#endif

/****************************************************************************/
/* Windows helper functions and signal handlers                             */
/****************************************************************************/

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

/****************************************************************************/
/* main() and friends                                                       */
/****************************************************************************/

static void printHelp(const char *cn,FILE *out)
{
	fprintf(out,"ZeroTier One version %d.%d.%d"ZT_EOL_S"(c)2011-2015 ZeroTier, Inc."ZT_EOL_S,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
	fprintf(out,"Licensed under the GNU General Public License v3"ZT_EOL_S""ZT_EOL_S);
	fprintf(out,"Usage: %s [-switches] [home directory]"ZT_EOL_S""ZT_EOL_S,cn);
	fprintf(out,"Available switches:"ZT_EOL_S);
	fprintf(out,"  -h                - Display this help"ZT_EOL_S);
	fprintf(out,"  -v                - Show version"ZT_EOL_S);
	fprintf(out,"  -p<port>          - Port for UDP and TCP/HTTP (default: 9993)"ZT_EOL_S);
	//fprintf(out,"  -T<path>          - Override root topology, do not authenticate or update"ZT_EOL_S);
#ifdef __UNIX_LIKE__
	fprintf(out,"  -d                - Fork and run as daemon (Unix-ish OSes)"ZT_EOL_S);
#endif // __UNIX_LIKE__
	fprintf(out,"  -i                - Generate and manage identities (zerotier-idtool)"ZT_EOL_S);
	fprintf(out,"  -q                - Query API (zerotier-cli)"ZT_EOL_S);
#ifdef __WINDOWS__
	fprintf(out,"  -C                - Run from command line instead of as service (Windows)"ZT_EOL_S);
	fprintf(out,"  -I                - Install Windows service (Windows)"ZT_EOL_S);
	fprintf(out,"  -R                - Uninstall Windows service (Windows)"ZT_EOL_S);
	fprintf(out,"  -D                - Load tap driver into system driver store (Windows)"ZT_EOL_S);
#endif // __WINDOWS__
}

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
#ifdef __UNIX_LIKE__
	signal(SIGHUP,&_sighandlerHup);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGALRM,SIG_IGN);
	signal(SIGINT,&_sighandlerQuit);
	signal(SIGTERM,&_sighandlerQuit);
	signal(SIGQUIT,&_sighandlerQuit);

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

	bool runAsDaemon = false;
#endif // __UNIX_LIKE__

#ifdef __WINDOWS__
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2),&wsaData);

#ifdef ZT_WIN_RUN_IN_CONSOLE
	bool winRunFromCommandLine = true;
#else
	bool winRunFromCommandLine = false;
#endif
#endif // __WINDOWS__

	if ((strstr(argv[0],"zerotier-idtool"))||(strstr(argv[0],"ZEROTIER-IDTOOL")))
		return idtool(argc,argv);
	if ((strstr(argv[0],"zerotier-cli"))||(strstr(argv[0],"ZEROTIER-CLI")))
		return cli(argc,argv);

	std::string overrideRootTopology;
	std::string homeDir;
	unsigned int port = ZT1_DEFAULT_PORT;

	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {

				case 'p': // port -- for both UDP and TCP, packets and control plane
					port = Utils::strToUInt(argv[i] + 2);
					if ((port > 0xffff)||(port == 0)) {
						printHelp(argv[0],stdout);
						return 1;
					}
					break;

#ifdef __UNIX_LIKE__
				case 'd': // Run in background as daemon
					runAsDaemon = true;
					break;
#endif // __UNIX_LIKE__

				case 'T': // Override root topology
					if (argv[i][2]) {
						if (!OSUtils::readFile(argv[i] + 2,overrideRootTopology)) {
							fprintf(stderr,"%s: cannot read root topology from %s"ZT_EOL_S,argv[0],argv[i] + 2);
							return 1;
						}
					} else {
						printHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'v': // Display version
					printf("%d.%d.%d"ZT_EOL_S,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
					return 0;

				case 'i': // Invoke idtool personality
					if (argv[i][2]) {
						printHelp(argv[0],stdout);
						return 0;
					} else return idtool(argc,argv);

				case 'q': // Invoke cli personality
					if (argv[i][2]) {
						printHelp(argv[0],stdout);
						return 0;
					} else return cli(argc,argv);

#ifdef __WINDOWS__
				case 'C': // Run from command line instead of as Windows service
					winRunFromCommandLine = true;
					break;

				case 'I': { // Install this binary as a Windows service
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

				case 'R': { // Uninstall this binary as Windows service
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

#if 0
				case 'D': { // Install Windows driver (since PNPUTIL.EXE seems to be weirdly unreliable)
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
#endif

				case 'h':
				case '?':
				default:
					printHelp(argv[0],stdout);
					return 0;
			}
		} else {
			if (homeDir.length()) {
				printHelp(argv[0],stdout);
				return 0;
			} else {
				homeDir = argv[i];
			}
		}
	}

	if (!homeDir.length())
		homeDir = OneService::platformDefaultHomePath();
	if (!homeDir.length()) {
		fprintf(stderr,"%s: no home path specified and no platform default available"ZT_EOL_S,argv[0]);
		return 1;
	} else {
		std::vector<std::string> hpsp(Utils::split(homeDir.c_str(),ZT_PATH_SEPARATOR_S,"",""));
		std::string ptmp;
		if (homeDir[0] == ZT_PATH_SEPARATOR)
			ptmp.push_back(ZT_PATH_SEPARATOR);
		for(std::vector<std::string>::iterator pi(hpsp.begin());pi!=hpsp.end();++pi) {
			if (ptmp.length() > 0)
				ptmp.push_back(ZT_PATH_SEPARATOR);
			ptmp.append(*pi);
			if ((*pi != ".")&&(*pi != "..")) {
				if (!OSUtils::mkdir(ptmp))
					throw std::runtime_error("home path does not exist, and could not create");
			}
		}
	}

#ifdef __UNIX_LIKE__
	if (getuid() != 0) {
		fprintf(stderr,"%s: must be run as root (uid 0)"ZT_EOL_S,argv[0]);
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
#endif // __UNIX_LIKE__

#ifdef __WINDOWS__
	if (winRunFromCommandLine) {
		// Running in "interactive" mode (mostly for debugging)
		if (IsCurrentUserLocalAdministrator() != TRUE) {
			fprintf(stderr,"%s: must be run as a local administrator."ZT_EOL_S,argv[0]);
			return 1;
		}
		_winPokeAHole();
		SetConsoleCtrlHandler(&_winConsoleCtrlHandler,TRUE);
		// continues on to ordinary command line execution code below...
	} else {
		// Running from service manager
		_winPokeAHole();
		ZeroTierOneService zt1Service;
		if (CServiceBase::Run(zt1Service) == TRUE) {
			return 0;
		} else {
			fprintf(stderr,"%s: unable to start service (try -h for help)"ZT_EOL_S,argv[0]);
			return 1;
		}
	}
#endif // __WINDOWS__

	NetworkController *controller = (NetworkController *)0;
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
	try {
		controller = new SqliteNetworkController((homeDir + ZT_PATH_SEPARATOR_S + ZT1_CONTROLLER_DB_PATH).c_str());
	} catch (std::exception &exc) {
		fprintf(stderr,"%s: failure initializing SqliteNetworkController: %s"ZT_EOL_S,argv[0],exc.what());
		return 1;
	} catch ( ... ) {
		fprintf(stderr,"%s: failure initializing SqliteNetworkController: unknown exception"ZT_EOL_S,argv[0]);
		return 1;
	}
#endif // ZT_ENABLE_NETWORK_CONTROLLER

#ifdef __UNIX_LIKE__
	std::string pidPath(homeDir + ZT_PATH_SEPARATOR_S + ZT1_PID_PATH);
	{
		// Write .pid file to home folder
		FILE *pf = fopen(pidPath.c_str(),"w");
		if (pf) {
			fprintf(pf,"%ld",(long)getpid());
			fclose(pf);
		}
	}
#endif // __UNIX_LIKE__

	unsigned int returnValue = 0;

	try {
		for(;;) {
			zt1Service = OneService::newInstance(homeDir.c_str(),port,controller,(overrideRootTopology.length() > 0) ? overrideRootTopology.c_str() : (const char *)0);
			switch(zt1Service->run()) {
				case OneService::ONE_STILL_RUNNING: // shouldn't happen, run() won't return until done
				case OneService::ONE_NORMAL_TERMINATION:
					break;
				case OneService::ONE_UNRECOVERABLE_ERROR:
					fprintf(stderr,"%s: fatal error: %s"ZT_EOL_S,argv[0],zt1Service->fatalErrorMessage().c_str());
					returnValue = 1;
					break;
				case OneService::ONE_IDENTITY_COLLISION: {
					delete zt1Service;
					zt1Service = (OneService *)0;
					std::string oldid;
					OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret").c_str(),oldid);
					if (oldid.length()) {
						OSUtils::writeFile((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret.saved_after_collision").c_str(),oldid);
						OSUtils::rm((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret").c_str());
						OSUtils::rm((homeDir + ZT_PATH_SEPARATOR_S + "identity.public").c_str());
					}
				}	continue; // restart!
			}
			break; // terminate loop -- normally we don't keep restarting
		}
	} catch (std::exception &exc) {
		fprintf(stderr,"%s: fatal error: %s"ZT_EOL_S,argv[0],exc.what());
		returnValue = 1;
	} catch ( ... ) {
		fprintf(stderr,"%s: fatal error: unknown exception"ZT_EOL_S,argv[0]);
		returnValue = 1;
	}

	delete zt1Service;
	zt1Service = (OneService *)0;
	delete controller;

#ifdef __UNIX_LIKE__
	OSUtils::rm(pidPath.c_str());
#endif

	return returnValue;
}
