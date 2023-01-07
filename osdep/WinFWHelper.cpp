#include "WinFWHelper.hpp"


namespace ZeroTier {



void ZeroTier::WinFWHelper::newICMPRule(const InetAddress& ip, uint64_t nwid)
{
	char nwString[32] = { 0 };
	char ipbuf[64];

	sprintf(nwString, "%.16llx", nwid);
	std::string nwString2 = { nwString };
	
	ip.toString(ipbuf);

	if (ip.isV4()) {
		WinFWHelper::newICMPv4Rule(ipbuf, nwid);
	}
	else {
		WinFWHelper::newICMPv6Rule(ipbuf, nwid);
	}
}

void ZeroTier::WinFWHelper::removeICMPRule(const InetAddress& ip, uint64_t nwid)
{
	char nwString[32] = { 0 };
	char ipbuf[64];

	sprintf(nwString, "%.16llx", nwid);
	std::string nwString2 = { nwString };

	ip.toString(ipbuf);

	if (ip.isV4()) {
		WinFWHelper::removeICMPv4Rule(ipbuf, nwid);
	}
	else {
		WinFWHelper::removeICMPv6Rule(ipbuf, nwid);
	}
}


void WinFWHelper::newICMPv4Rule(std::string address, uint64_t nwid)
{
	// allows icmp, scoped to a specific ip address and interface name

	char nwString[32] = { 0 };
	sprintf(nwString, "%.16llx", nwid);
	std::string nwString2 = { nwString };

		std::string cmd = R"(C:\Windows\System32\WindowsPowershell\v1.0\powershell.exe "New-NetFirewallRule -DisplayName zerotier-icmpv4-)" + nwString2 + address +
			R"( -InterfaceAlias 'ZeroTier One `[)" + nwString2 + R"(`]')" + 
			" -Protocol ICMPv4 -Action Allow" + 
			" -LocalAddress " + address + "\"\r\n";
		
		_run(cmd);
}

void WinFWHelper::newICMPv6Rule(std::string address, uint64_t nwid)
{
		// allows icmp, scoped to a specific ip address and interface name

		char nwString[32] = { 0 };
		sprintf(nwString, "%.16llx", nwid);
		std::string nwString2 = { nwString };

		std::string cmd = R"(C:\Windows\System32\WindowsPowershell\v1.0\powershell.exe "New-NetFirewallRule -DisplayName zerotier-icmpv6-)" + nwString2 + address +
			R"( -InterfaceAlias 'ZeroTier One `[)" + nwString2 + R"(`]')" + 
			" -Protocol ICMPv6 -Action Allow" + 
			" -LocalAddress " + address + "\"\r\n";

		_run(cmd);
}

void WinFWHelper::removeICMPv4Rule(std::string addr, uint64_t nwid)
{
		// removes 1 icmp firewall rule

		char nwString[32] = { 0 };
		sprintf(nwString, "%.16llx", nwid);
		std::string nwString2 = { nwString };

		std::string cmd = R"(C:\Windows\System32\WindowsPowershell\v1.0\powershell.exe "Remove-NetFirewallRule -DisplayName zerotier-icmpv4-)" + nwString2 + addr +
		 "\"\r\n";

		_run(cmd);
}

void WinFWHelper::removeICMPv6Rule(std::string addr, uint64_t nwid)
{
		// removes 1 icmp firewall rule

		char nwString[32] = { 0 };
		sprintf(nwString, "%.16llx", nwid);
		std::string nwString2 = { nwString };

		std::string cmd = R"(C:\Windows\System32\WindowsPowershell\v1.0\powershell.exe "Remove-NetFirewallRule -DisplayName zerotier-icmpv6-)" + nwString2 + addr +
		 "\"\r\n";

		_run(cmd);
}

void WinFWHelper::removeICMPv4Rules(uint64_t nwid)
{
		// removes all icmp firewall rules for this network id

		char nwString[32] = { 0 };
		sprintf(nwString, "%.16llx", nwid);
		std::string nwString2 = { nwString };

		std::string cmd = R"(C:\Windows\System32\WindowsPowershell\v1.0\powershell.exe "Remove-NetFirewallRule -DisplayName zerotier-icmpv4-)" + nwString2 + "*\" \r\n";
		
		_run(cmd);
}

void WinFWHelper::removeICMPv6Rules(uint64_t nwid)
{
		// removes all icmp firewall rules for this network id

		char nwString[32] = { 0 };
		sprintf(nwString, "%.16llx", nwid);
		std::string nwString2 = { nwString };

		std::string cmd = R"(C:\Windows\System32\WindowsPowershell\v1.0\powershell.exe "Remove-NetFirewallRule -DisplayName zerotier-icmpv6-)" + nwString2 + "*\" \r\n";

		_run(cmd);
}

void WinFWHelper::removeICMPRules()
{
		// removes all icmp firewall rules for all networks

		std::string cmd = R"(C:\Windows\System32\WindowsPowershell\v1.0\powershell.exe "Remove-NetFirewallRule -DisplayName zerotier-icmp*)" + std::string("\r\n");

		_run(cmd);
}

void WinFWHelper::removeICMPRules(uint64_t nwid)
{
		// removes all icmp firewall rules for this network
		WinFWHelper::removeICMPv4Rules(nwid);
		WinFWHelper::removeICMPv6Rules(nwid);
}



void WinFWHelper::_run(std::string cmd)
{
		
		#ifdef ZT_DEBUG
				fprintf(stderr, cmd.c_str());
		#endif

		STARTUPINFOA startupInfo;
		PROCESS_INFORMATION processInfo;
		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo, 0, sizeof(STARTUPINFOA));
		memset(&processInfo, 0, sizeof(PROCESS_INFORMATION));

		if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startupInfo, &processInfo)) {
			WaitForSingleObject(processInfo.hProcess, INFINITE);

			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
}



}	// namespace ZeroTier