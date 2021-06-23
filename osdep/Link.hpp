/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_LINK_HPP
#define ZT_LINK_HPP

#include "../node/AtomicCounter.hpp"
#include "../node/SharedPtr.hpp"

#include <string>

namespace ZeroTier {

class Link {
	friend class SharedPtr<Link>;

  public:
	/**
	 *
	 * @param ifnameStr
	 * @param ipvPref
	 * @param speed
	 * @param enabled
	 * @param mode
	 * @param failoverToLinkStr
	 * @param userSpecifiedAlloc
	 */
	Link(std::string& ifnameStr, uint8_t ipvPref, uint32_t speed, uint32_t linkMonitorInterval, uint32_t upDelay, uint32_t downDelay, bool enabled, uint8_t mode, std::string failoverToLinkStr, float userSpecifiedAlloc)
		: _ifnameStr(ifnameStr)
		, _ipvPref(ipvPref)
		, _speed(speed)
		, _relativeSpeed(0)
		, _linkMonitorInterval(linkMonitorInterval)
		, _upDelay(upDelay)
		, _downDelay(downDelay)
		, _enabled(enabled)
		, _mode(mode)
		, _failoverToLinkStr(failoverToLinkStr)
		, _userSpecifiedAlloc(userSpecifiedAlloc)
		, _isUserSpecified(false)
	{
	}

	/**
	 * @return The string representation of this link's underlying interface's system name.
	 */
	inline std::string ifname()
	{
		return _ifnameStr;
	}

	/**
	 * @return Whether this link is designated as a primary.
	 */
	inline bool primary()
	{
		return _mode == ZT_MULTIPATH_SLAVE_MODE_PRIMARY;
	}

	/**
	 * @return Whether this link is designated as a spare.
	 */
	inline bool spare()
	{
		return _mode == ZT_MULTIPATH_SLAVE_MODE_SPARE;
	}

	/**
	 * @return The name of the link interface that should be used in the event of a failure.
	 */
	inline std::string failoverToLink()
	{
		return _failoverToLinkStr;
	}

	/**
	 * @return Whether this link interface was specified by the user or auto-detected.
	 */
	inline bool isUserSpecified()
	{
		return _isUserSpecified;
	}

	/**
	 * Signify that this link was specified by the user and not the result of auto-detection.
	 *
	 * @param isUserSpecified
	 */
	inline void setAsUserSpecified(bool isUserSpecified)
	{
		_isUserSpecified = isUserSpecified;
	}

	/**
	 * @return Whether or not the user has specified failover instructions.
	 */
	inline bool userHasSpecifiedFailoverInstructions()
	{
		return _failoverToLinkStr.length();
	}

	/**
	 * @return The speed of the link relative to others in the bond.
	 */
	inline uint8_t relativeSpeed()
	{
		return _relativeSpeed;
	}

	/**
	 * Sets the speed of the link relative to others in the bond.
	 *
	 * @param relativeSpeed The speed relative to the rest of the link.
	 */
	inline void setRelativeSpeed(uint8_t relativeSpeed)
	{
		_relativeSpeed = relativeSpeed;
	}

	/**
	 * Sets the speed of the link relative to others in the bond.
	 *
	 * @param relativeSpeed
	 */
	inline void setMonitorInterval(uint32_t interval)
	{
		_linkMonitorInterval = interval;
	}

	/**
	 * @return The absolute speed of the link (as specified by the user.)
	 */
	inline uint32_t monitorInterval()
	{
		return _linkMonitorInterval;
	}

	/**
	 * @return The absolute speed of the link (as specified by the user.)
	 */
	inline uint32_t speed()
	{
		return _speed;
	}

	/**
	 * @return The address preference for this link (as specified by the user.)
	 */
	inline uint8_t ipvPref()
	{
		return _ipvPref;
	}

	/**
	 * @return The mode (e.g. primary/spare) for this link (as specified by the user.)
	 */
	inline uint8_t mode()
	{
		return _mode;
	}

	/**
	 * @return The upDelay parameter for all paths on this link.
	 */
	inline uint32_t upDelay()
	{
		return _upDelay;
	}

	/**
	 * @return The downDelay parameter for all paths on this link.
	 */
	inline uint32_t downDelay()
	{
		return _downDelay;
	}

	/**
	 * @return Whether this link is enabled or disabled
	 */
	inline uint8_t enabled()
	{
		return _enabled;
	}

  private:
	/**
	 * String representation of underlying interface's system name
	 */
	std::string _ifnameStr;

	/**
	 * What preference (if any) a user has for IP protocol version used in
	 * path aggregations. Preference is expressed in the order of the digits:
	 *
	 *  0: no preference
	 *  4: IPv4 only
	 *  6: IPv6 only
	 * 46: IPv4 over IPv6
	 * 64: IPv6 over IPv4
	 */
	uint8_t _ipvPref;

	/**
	 * User-specified speed of this link
	 */
	uint32_t _speed;

	/**
	 * Speed relative to other specified links (computed by Bond)
	 */
	uint8_t _relativeSpeed;

	/**
	 * User-specified interval for monitoring paths on this specific link
	 * instead of using the more generic interval specified for the entire
	 * bond.
	 */
	uint32_t _linkMonitorInterval;

	/**
	 * How long before a path is considered to be usable after coming online. (when using policies that
	 * support fail-over events).
	 */
	uint32_t _upDelay;

	/**
	 * How long before a path is considered to be dead (when using policies that
	 * support fail-over events).
	 */
	uint32_t _downDelay;

	/**
	 * Whether this link is enabled, or (disabled (possibly bad config))
	 */
	uint8_t _enabled;

	/**
	 * Whether this link is designated as a primary, a spare, or no preference.
	 */
	uint8_t _mode;

	/**
	 * The specific name of the link to be used in the event that this
	 * link fails.
	 */
	std::string _failoverToLinkStr;

	/**
	 * User-specified allocation
	 */
	float _userSpecifiedAlloc;

	/**
	 * Whether or not this link was created as a result of manual user specification. This is
	 * important to know because certain policy decisions are dependent on whether the user
	 * intents to use a specific set of interfaces.
	 */
	bool _isUserSpecified;

	AtomicCounter __refCount;
};

}	// namespace ZeroTier

#endif
