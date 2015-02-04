/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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

#include "Packet.hpp"

namespace ZeroTier {

const unsigned char Packet::ZERO_KEY[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

const char *Packet::verbString(Verb v)
	throw()
{
	switch(v) {
		case VERB_NOP: return "NOP";
		case VERB_HELLO: return "HELLO";
		case VERB_ERROR: return "ERROR";
		case VERB_OK: return "OK";
		case VERB_WHOIS: return "WHOIS";
		case VERB_RENDEZVOUS: return "RENDEZVOUS";
		case VERB_FRAME: return "FRAME";
		case VERB_EXT_FRAME: return "EXT_FRAME";
		case VERB_P5_MULTICAST_FRAME: return "P5_MULTICAST_FRAME";
		case VERB_MULTICAST_LIKE: return "MULTICAST_LIKE";
		case VERB_NETWORK_MEMBERSHIP_CERTIFICATE: return "NETWORK_MEMBERSHIP_CERTIFICATE";
		case VERB_NETWORK_CONFIG_REQUEST: return "NETWORK_CONFIG_REQUEST";
		case VERB_NETWORK_CONFIG_REFRESH: return "NETWORK_CONFIG_REFRESH";
		case VERB_MULTICAST_GATHER: return "MULTICAST_GATHER";
		case VERB_MULTICAST_FRAME: return "MULTICAST_FRAME";
		case VERB_PHYSICAL_ADDRESS_CHANGED: return "PHYSICAL_ADDRESS_CHANGED";
	}
	return "(unknown)";
}

const char *Packet::errorString(ErrorCode e)
	throw()
{
	switch(e) {
		case ERROR_NONE: return "NONE";
		case ERROR_INVALID_REQUEST: return "INVALID_REQUEST";
		case ERROR_BAD_PROTOCOL_VERSION: return "BAD_PROTOCOL_VERSION";
		case ERROR_OBJ_NOT_FOUND: return "OBJECT_NOT_FOUND";
		case ERROR_IDENTITY_COLLISION: return "IDENTITY_COLLISION";
		case ERROR_UNSUPPORTED_OPERATION: return "UNSUPPORTED_OPERATION";
		case ERROR_NEED_MEMBERSHIP_CERTIFICATE: return "NEED_MEMBERSHIP_CERTIFICATE";
		case ERROR_NETWORK_ACCESS_DENIED_: return "NETWORK_ACCESS_DENIED";
		case ERROR_UNWANTED_MULTICAST: return "UNWANTED_MULTICAST";
	}
	return "(unknown)";
}

} // namespace ZeroTier
