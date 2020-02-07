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

#ifndef ZT_VL2_HPP
#define ZT_VL2_HPP

#include "Constants.hpp"
#include "Buf.hpp"
#include "Address.hpp"
#include "Protocol.hpp"
#include "Hashtable.hpp"
#include "Mutex.hpp"
#include "FCV.hpp"

namespace ZeroTier {

class Path;
class Peer;
class RuntimeEnvironment;
class VL1;

class VL2
{
	friend class VL1;

public:
	VL2(const RuntimeEnvironment *renv);
	~VL2();

protected:
	void _FRAME(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _EXT_FRAME(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _MULTICAST_LIKE(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _NETWORK_CREDENTIALS(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _NETWORK_CONFIG_REQUEST(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _NETWORK_CONFIG(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _MULTICAST_GATHER(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _MULTICAST_FRAME_deprecated(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _MULTICAST(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);

private:
};

} // namespace ZeroTier

#endif
