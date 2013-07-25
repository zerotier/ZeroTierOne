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

#ifndef _ZT_RPC_HPP
#define _ZT_RPC_HPP

#include <stdint.h>

#include <stdexcept>
#include <map>
#include <vector>
#include <utility>

#include "Constants.hpp"
#include "NonCopyable.hpp"
#include "Mutex.hpp"
#include "Address.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Peer or method not found
 */
#define ZT_RPC_ERROR_NOT_FOUND -1

/**
 * A runtime error occurred
 */
#define ZT_RPC_ERROR_RUNTIME -2

/**
 * Call was expired without response from target
 */
#define ZT_RPC_ERROR_EXPIRED_NO_RESPONSE -3

/**
 * Call was cancelled (or RPC is shutting down)
 */
#define ZT_RPC_ERROR_CANCELLED -4

/**
 * RPC request and result handler
 */
class RPC : NonCopyable
{
public:
#ifndef __WINDOWS__
	/**
	 * A local service accessible by RPC, non-Windows only for now
	 *
	 * Each service DLL must export these functions:
	 *
	 * int ZeroTierPluginInit();
	 * int ZeroTierPluginDo(unsigned int,const unsigned int *,const void **,const unsigned int **,const void ***);
	 * void ZeroTierPluginFree(int,const unsigned int *,const void **);
	 * void ZeroTierPluginDestroy();
	 *
	 * Init is called on library load, Destroy on unload. Do() may
	 * be called from multiple threads concurrently, so any locking
	 * is the responsibility of the library. These must have C
	 * function signatures (extern "C" in C++).
	 *
	 * Do's arguments are: the number of paramters, the size of each parameter in bytes,
	 * and each parameter's contents. The last two arguments are result parameters. The
	 * first result parameter must be set to an array of integers describing the size of
	 * each result. The second is set to an array of pointers to actual results. The number
	 * of results (size of both arrays) is returned. If Do() returns zero or negative,
	 * these result paremeters are not used by the caller and don't need to be set.
	 *
	 * After the caller is done with Do()'s results, it calls ZeroTierPluginFree() to
	 * free them. This may also be called concurrently. Free() takes the number of
	 * results, the array of result sizes, and the result array.
	 */
	class LocalService : NonCopyable
	{
	public:
		/**
		 * @param dllPath Path to DLL/shared object
		 * @throws std::invalid_argument Unable to properly load or resolve symbol(s) in DLL
		 */
		LocalService(const char *dllPath)
			throw(std::invalid_argument);

		~LocalService();

		/**
		 * Call the DLL, return result
		 *
		 * @param args Input arguments
		 * @return Results from DLL
		 * @throws std::runtime_error Error calling DLL
		 */
		std::pair< int,std::vector<std::string> > operator()(const std::vector<std::string> &args);

	private:
		void *_handle;
		void *_init;
		void *_do;
		void *_free;
		void *_destroy;
	};
#endif

	RPC(const RuntimeEnvironment *renv);
	~RPC();

	/**
	 * Used by PacketDecoder to call local RPC methods
	 *
	 * @param name Name of locally loaded method to call
	 * @param args Arguments to method
	 * @return Return value of method, and results (negative first item and empty vector means error)
	 */
	std::pair< int,std::vector<std::string> > callLocal(const std::string &name,const std::vector<std::string> &args);

	/**
	 * Load a plugin
	 *
	 * @param name Name of RPC function
	 * @param path Path to plugin DLL
	 * @throws std::invalid_argument Unable to properly load or resolve symbol(s) in DLL
	 */
	void loadLocal(const char *name,const char *path)
		throw(std::invalid_argument);

	/**
	 * Call a remote service
	 *
	 * @param peer Peer to call on
	 * @param name Name of remote function
	 * @param args Arguments to remote function
	 * @param handler Handler to call on result
	 * @param arg First argument to handler
	 * @return Call ID (packet ID of sent packet)
	 */
	uint64_t callRemote(
		const Address &peer,
		const std::string &name,
		const std::vector<std::string> &args,
		void (*handler)(void *,uint64_t,const Address &,int,const std::vector<std::string> &),
		void *arg)
		throw(std::invalid_argument,std::out_of_range);

	/**
	 * Periodically called to clean up, such as by expiring remote calls
	 */
	void clean();

private:
	const RuntimeEnvironment *_r;

#ifndef __WINDOWS__
	std::map<std::string,LocalService *> _rpcServices;
	Mutex _rpcServices_m;
#endif

	struct RemoteCallOutstanding
	{
		uint64_t callTime;
		Address peer;
		void (*handler)(void *,uint64_t,const Address &,int,const std::vector<std::string> &);
		void *arg;
	};
	std::map<uint64_t,RemoteCallOutstanding> _remoteCallsOutstanding;
	Mutex _remoteCallsOutstanding_m;
};

} // namespace ZeroTier

#endif
