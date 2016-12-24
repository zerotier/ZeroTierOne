#ifndef ZT_MANAGEDROUTE_HPP
#define ZT_MANAGEDROUTE_HPP

#include <stdlib.h>
#include <string.h>

#include "../node/InetAddress.hpp"
#include "../node/Utils.hpp"

#include <stdexcept>
#include <vector>

namespace ZeroTier {

/**
 * A ZT-managed route that used C++ RAII semantics to automatically clean itself up on deallocate
 */
class ManagedRoute
{
public:
	ManagedRoute()
	{
		_device[0] = (char)0;
		_systemDevice[0] = (char)0;
		_applied = false;
	}

	~ManagedRoute()
	{
		this->remove();
	}

	ManagedRoute(const ManagedRoute &r)
	{
		_applied = false;
		*this = r;
	}

	inline ManagedRoute &operator=(const ManagedRoute &r)
	{
		if ((!_applied)&&(!r._applied)) {
			memcpy(this,&r,sizeof(ManagedRoute)); // InetAddress is memcpy'able
		} else {
			fprintf(stderr,"Applied ManagedRoute isn't copyable!\n");
			abort();
		}
		return *this;
	}

	/**
	 * Initialize object and set route
	 *
	 * Note: on Windows, use the interface NET_LUID in hexadecimal as the
	 * "device name."
	 *
	 * @param target Route target (e.g. 0.0.0.0/0 for default)
	 * @param via Route next L3 hop or NULL InetAddress if local in which case it will be routed via device
	 * @param device Name or hex LUID of ZeroTier device (e.g. zt#)
	 * @return True if route was successfully set
	 */
	inline bool set(const InetAddress &target,const InetAddress &via,const char *device)
	{
		if ((!via)&&(!device[0]))
			return false;
		this->remove();
		_target = target;
		_via = via;
		Utils::scopy(_device,sizeof(_device),device);
		return this->sync();
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
	char _device[128];
	char _systemDevice[128]; // for route overrides
	bool _applied;
};

} // namespace ZeroTier

#endif
