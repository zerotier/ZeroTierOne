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
	ManagedRoute() :
		target(),
		via(),
		applied(false)
	{
		device[0] = (char)0;
	}

	~ManagedRoute()
	{
		this->remove();
	}

	/**
	 * @param target Route target (e.g. 0.0.0.0/0 for default)
	 * @param via Route next L3 hop or NULL InetAddress if local
	 * @param device Device name/ID if 'via' is null and route is local, otherwise ignored
	 * @return True if route was successfully set
	 */
	inline bool set(const InetAddress &target,const InetAddress &via,const char *device)
	{
		if ((!via)&&((!device)||(!device[0])))
			return false;
		this->remove();
		this->target = target;
		this->via = via;
		this->applied = true;
		Utils::scopy(this->device,sizeof(this->device),device);
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
	 * Remove and clear this ManagedRoute (also done automatically on destruct)
	 *
	 * This does nothing if this ManagedRoute is not set or has already been removed.
	 */
	void remove();

private:
	/*
	static inline bool _viaCompare(const InetAddress &v1,const InetAddress &v2)
	{
		if (v1) {
			if (v2)
				return v1.ipsEqual(v2);
			else return false;
		} else if (v2)
			return false;
		else return true;
	}
	*/

	// non-copyable
	ManagedRoute(const ManagedRoute &mr) {}
	inline ManagedRoute &operator=(const ManagedRoute &mr) { return *this; }

	InetAddress target;
	InetAddress via;
	bool applied;
	char device[128];
};

} // namespace ZeroTier

#endif
