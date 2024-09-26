/*
 * Copyright (c)2019 ZeroTier, Inc.
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
#include "Identity.hpp"
#include "Utils.hpp"

#include <string.h>

namespace ZeroTier {

class NodeConfig;
class Switch;
class Topology;
class Node;
class Multicaster;
class NetworkController;
class SelfAwareness;
class Trace;
class Bond;
class PacketMultiplexer;

/**
 * Holds global state for an instance of ZeroTier::Node
 */
class RuntimeEnvironment {
  public:
    RuntimeEnvironment(Node* n) : node(n), localNetworkController((NetworkController*)0), rtmem((void*)0), sw((Switch*)0), mc((Multicaster*)0), topology((Topology*)0), sa((SelfAwareness*)0)
    {
        publicIdentityStr[0] = (char)0;
        secretIdentityStr[0] = (char)0;
    }

    ~RuntimeEnvironment()
    {
        Utils::burn(secretIdentityStr, sizeof(secretIdentityStr));
    }

    // Node instance that owns this RuntimeEnvironment
    Node* const node;

    // This is set externally to an instance of this base class
    NetworkController* localNetworkController;

    // Memory actually occupied by Trace, Switch, etc.
    void* rtmem;

    /* Order matters a bit here. These are constructed in this order
     * and then deleted in the opposite order on Node exit. The order ensures
     * that things that are needed are there before they're needed.
     *
     * These are constant and never null after startup unless indicated. */

    Trace* t;
    Switch* sw;
    Multicaster* mc;
    Topology* topology;
    SelfAwareness* sa;
    Bond* bc;
    PacketMultiplexer* pm;

    // This node's identity and string representations thereof
    Identity identity;
    char publicIdentityStr[ZT_IDENTITY_STRING_BUFFER_LENGTH];
    char secretIdentityStr[ZT_IDENTITY_STRING_BUFFER_LENGTH];
};

}   // namespace ZeroTier

#endif
