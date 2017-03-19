#ifndef ZT_MANAGEDROUTE_HPP
#define ZT_MANAGEDROUTE_HPP

#include <stdlib.h>
#include <string.h>

#include "../node/InetAddress.hpp"
#include "../node/Utils.hpp"
#include "../node/SharedPtr.hpp"
#include "../node/AtomicCounter.hpp"
#include "../node/NonCopyable.hpp"

#include <stdexcept>
#include <vector>
#include <map>

namespace ZeroTier {

/**
 * A ZT-managed route that used C++ RAII semantics to automatically clean itself up on deallocate
 */
class ManagedRoute : NonCopyable
{
	friend class SharedPtr<ManagedRoute>;

public:
	ManagedRoute(const InetAddress &target,const InetAddress &via,const char *device)
	{
		_target = target;
		_via = via;
		if (via.ss_family == AF_INET)
			_via.setPort(32);
		else if (via.ss_family == AF_INET6)
			_via.setPort(128);
		Utils::scopy(_device,sizeof(_device),device);
		_systemDevice[0] = (char)0;
	}

	~ManagedRoute()
	{
		this->remove();
	}

	/**
	 * Set or update currently set route
	 *
	 * This must be called periodically for routes that shadow others so that
	 * shadow routes can be updated. In some cases it has no effect
	 *
	 * @return True if route add/update was successful
	 */
	bool sync();

	/**
	 * Remove and clear this ManagedRoute
	 *
	 * This does nothing if this ManagedRoute is not set or has already been
	 * removed. If this is not explicitly called it is called automatically on
	 * destruct.
	 */
	void remove();

	inline const InetAddress &target() const { return _target; }
	inline const InetAddress &via() const { return _via; }
	inline const char *device() const { return _device; }

private:
	InetAddress _target;
	InetAddress _via;
	InetAddress _systemVia; // for route overrides
	std::map<InetAddress,bool> _applied; // routes currently applied
	char _device[128];
	char _systemDevice[128]; // for route overrides

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
