/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_RUNTIMEENVIRONMENT_HPP
#define ZT_RUNTIMEENVIRONMENT_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "Identity.hpp"
#include "AES.hpp"

namespace ZeroTier {

class VL1;
class VL2;
class Topology;
class Node;
class NetworkController;
class SelfAwareness;
class Trace;
class Expect;

/**
 * ZeroTier::Node execution context
 *
 * This just holds pointers and various other information used by all the
 * various moving parts of a node. It's stored or passed as 'RR' to give it
 * a common name througout the code.
 */
class RuntimeEnvironment
{
public:
	ZT_INLINE RuntimeEnvironment(Node *const n) noexcept :
		instanceId(Utils::getSecureRandomU64()),
		node(n),
		localNetworkController(nullptr),
		rtmem(nullptr),
		t(nullptr),
		expect(nullptr),
		vl2(nullptr),
		vl1(nullptr),
		topology(nullptr),
		sa(nullptr)
	{
		publicIdentityStr[0] = 0;
		secretIdentityStr[0] = 0;
	}

	ZT_INLINE ~RuntimeEnvironment() noexcept
	{
		Utils::burn(secretIdentityStr,sizeof(secretIdentityStr));
	}

	// Unique ID generated on startup
	const uint64_t instanceId;

	// Node instance that owns this RuntimeEnvironment
	Node *const node;

	// This is set externally to an instance of this base class
	NetworkController *localNetworkController;

	// Memory actually occupied by Trace, Switch, etc.
	void *rtmem;

	Trace *t;
	Expect *expect;
	VL2 *vl2;
	VL1 *vl1;
	Topology *topology;
	SelfAwareness *sa;

	// This node's identity and string representations thereof
	Identity identity;
	char publicIdentityStr[ZT_IDENTITY_STRING_BUFFER_LENGTH];
	char secretIdentityStr[ZT_IDENTITY_STRING_BUFFER_LENGTH];

	// AES keyed with a hash of this node's identity secret keys for local cache encryption at rest (where needed).
	AES localCacheSymmetric;

	// Privileged ports from 1 to 1023 in a random order (for IPv4 NAT traversal)
	uint16_t randomPrivilegedPortOrder[1023];
};

} // namespace ZeroTier

#endif
