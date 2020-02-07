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

#include <string.h>

#include "Constants.hpp"
#include "Utils.hpp"
#include "Identity.hpp"

namespace ZeroTier {

class VL1;
class VL2;
class Topology;
class Node;
class NetworkController;
class SelfAwareness;
class Trace;

/**
 * Holds global state for an instance of ZeroTier::Node
 */
class RuntimeEnvironment
{
public:
	ZT_ALWAYS_INLINE RuntimeEnvironment(Node *n) :
		node(n),
		localNetworkController(nullptr),
		rtmem(nullptr),
		t(nullptr),
		vl2(nullptr),
		vl1(nullptr),
		topology(nullptr),
		sa(nullptr)
	{
		publicIdentityStr[0] = (char)0;
		secretIdentityStr[0] = (char)0;
	}

	ZT_ALWAYS_INLINE ~RuntimeEnvironment()
	{
		Utils::burn(secretIdentityStr,sizeof(secretIdentityStr));
	}

	// Node instance that owns this RuntimeEnvironment
	Node *const node;

	// This is set externally to an instance of this base class
	NetworkController *localNetworkController;

	// Memory actually occupied by Trace, Switch, etc.
	void *rtmem;

	/* Order matters a bit here. These are constructed in this order
	 * and then deleted in the opposite order on Node exit. The order ensures
	 * that things that are needed are there before they're needed.
	 *
	 * These are constant and never null after startup unless indicated. */

	Trace *t;
	VL2 *vl2;
	VL1 *vl1;
	Topology *topology;
	SelfAwareness *sa;

	// This node's identity and string representations thereof
	Identity identity;
	char publicIdentityStr[ZT_IDENTITY_STRING_BUFFER_LENGTH];
	char secretIdentityStr[ZT_IDENTITY_STRING_BUFFER_LENGTH];
};

} // namespace ZeroTier

#endif
