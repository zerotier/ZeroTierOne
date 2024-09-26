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

#ifndef ZT_NETWORKCONFIGMASTER_HPP
#define ZT_NETWORKCONFIGMASTER_HPP

#include "Address.hpp"
#include "Constants.hpp"
#include "Dictionary.hpp"
#include "NetworkConfig.hpp"
#include "Revocation.hpp"

#include <stdint.h>

namespace ZeroTier {

class Identity;
struct InetAddress;

/**
 * Interface for network controller implementations
 */
class NetworkController {
  public:
    enum ErrorCode { NC_ERROR_NONE = 0, NC_ERROR_OBJECT_NOT_FOUND = 1, NC_ERROR_ACCESS_DENIED = 2, NC_ERROR_INTERNAL_SERVER_ERROR = 3, NC_ERROR_AUTHENTICATION_REQUIRED = 4 };

    /**
     * Interface for sender used to send pushes and replies
     */
    class Sender {
      public:
        /**
         * Send a configuration to a remote peer
         *
         * @param nwid Network ID
         * @param requestPacketId Request packet ID to send OK(NETWORK_CONFIG_REQUEST) or 0 to send NETWORK_CONFIG (push)
         * @param destination Destination peer Address
         * @param nc Network configuration to send
         * @param sendLegacyFormatConfig If true, send an old-format network config
         */
        virtual void ncSendConfig(uint64_t nwid, uint64_t requestPacketId, const Address& destination, const NetworkConfig& nc, bool sendLegacyFormatConfig) = 0;

        /**
         * Send revocation to a node
         *
         * @param destination Destination node address
         * @param rev Revocation to send
         */
        virtual void ncSendRevocation(const Address& destination, const Revocation& rev) = 0;

        /**
         * Send a network configuration request error
         *
         * If errorData/errorDataSize are provided they must point to a valid serialized
         * Dictionary containing error data. They can be null/zero if not specified.
         *
         * @param nwid Network ID
         * @param requestPacketId Request packet ID or 0 if none
         * @param destination Destination peer Address
         * @param errorCode Error code
         * @param errorData Data associated with error or NULL if none
         * @param errorDataSize Size of errorData in bytes
         */
        virtual void ncSendError(uint64_t nwid, uint64_t requestPacketId, const Address& destination, NetworkController::ErrorCode errorCode, const void* errorData, unsigned int errorDataSize) = 0;
    };

    NetworkController()
    {
    }
    virtual ~NetworkController()
    {
    }

    /**
     * Called when this is added to a Node to initialize and supply info
     *
     * @param signingId Identity for signing of network configurations, certs, etc.
     * @param sender Sender implementation for sending replies or config pushes
     */
    virtual void init(const Identity& signingId, Sender* sender) = 0;

    /**
     * Handle a network configuration request
     *
     * @param nwid 64-bit network ID
     * @param fromAddr Originating wire address or null address if packet is not direct (or from self)
     * @param requestPacketId Packet ID of request packet or 0 if not initiated by remote request
     * @param identity ZeroTier identity of originating peer
     * @param metaData Meta-data bundled with request (if any)
     * @return Returns NETCONF_QUERY_OK if result 'nc' is valid, or an error code on error
     */
    virtual void request(uint64_t nwid, const InetAddress& fromAddr, uint64_t requestPacketId, const Identity& identity, const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY>& metaData) = 0;
};

}   // namespace ZeroTier

#endif
