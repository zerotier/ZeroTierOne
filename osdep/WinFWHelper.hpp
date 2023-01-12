#ifndef WIN_FW_HELPER_H_
#define WIN_FW_HELPER_H_

#include "../node/InetAddress.hpp"

#include <cstdint>
#include <vector>

namespace ZeroTier {

class WinFWHelper {
  public:
	static void newICMPRule(const InetAddress& ip, uint64_t nwid);
	static void removeICMPRule(const InetAddress& ip, uint64_t nwid);
	static void removeICMPRules(uint64_t nwid);
	static void removeICMPRules();


  private:
	static void _run(std::string cmd);
	static void newICMPv4Rule(std::string address, uint64_t nwid);
	static void newICMPv6Rule(std::string address, uint64_t nwid);
	static void removeICMPv4Rule(std::string address, uint64_t nwid);
	static void removeICMPv6Rule(std::string address, uint64_t nwid);
	static void removeICMPv4Rules(uint64_t nwid);
	static void removeICMPv6Rules(uint64_t nwid);
};

}	// namespace ZeroTier

#endif