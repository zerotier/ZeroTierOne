/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
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
#include "TinyMap.hpp"
#include "SharedPtr.hpp"

namespace ZeroTier {

class VL1;
class VL2;
class Topology;
class Node;
class NetworkController;
class SelfAwareness;
class Trace;
class Expect;
class TrustStore;
class Store;
class Network;

/**
 * Node instance context
 */
class Context
{
public:
	ZT_INLINE Context(Node *const n) noexcept:
		instanceId(Utils::getSecureRandomU64()),
		node(n),
		uPtr(nullptr),
		localNetworkController(nullptr),
		store(nullptr),
		networks(nullptr),
		t(nullptr),
		expect(nullptr),
		vl2(nullptr),
		vl1(nullptr),
		topology(nullptr),
		sa(nullptr),
		ts(nullptr)
	{
		publicIdentityStr[0] = 0;
		secretIdentityStr[0] = 0;
	}

	ZT_INLINE ~Context() noexcept
	{
		Utils::burn(secretIdentityStr, sizeof(secretIdentityStr));
	}

	// Unique ID generated on startup
	const uint64_t instanceId;

	// Node instance that owns this RuntimeEnvironment
	Node *const restrict node;

	// Callbacks specified by caller who created node
	ZT_Node_Callbacks cb;

	// User pointer specified by external code via API
	void *restrict uPtr;

	// This is set externally to an instance of this base class
	NetworkController *restrict localNetworkController;

	Store *restrict store;
	TinyMap< SharedPtr< Network > > *restrict networks;
	Trace *restrict t;
	Expect *restrict expect;
	VL2 *restrict vl2;
	VL1 *restrict vl1;
	Topology *restrict topology;
	SelfAwareness *restrict sa;
	TrustStore *restrict ts;

	// This node's identity and string representations thereof
	Identity identity;
	char publicIdentityStr[ZT_IDENTITY_STRING_BUFFER_LENGTH];
	char secretIdentityStr[ZT_IDENTITY_STRING_BUFFER_LENGTH];

	// Symmetric key for encrypting secrets at rest on this system.
	AES localSecretCipher;

	// Privileged ports from 1 to 1023 in a random order (for IPv4 NAT traversal)
	uint16_t randomPrivilegedPortOrder[1023];
};

} // namespace ZeroTier

#endif
