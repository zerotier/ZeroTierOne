/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#include "Path.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
//#include "Topology.hpp"

namespace ZeroTier {

bool Path::send(const RuntimeEnvironment *RR,const void *data,unsigned int len,uint64_t now)
{
	/*
	if (len > 13) {
		Address zta(reinterpret_cast<const uint8_t *>(data)+8,5);
		if ((zta.toInt() == 0x9d219039f3ULL)||(zta.toInt() == 0x8841408a2eULL)) {
			printf(">> %s@%s %u ",zta.toString().c_str(),address().toString().c_str(),len);
			Packet pcopy(data,len);
			SharedPtr<Peer> rp(RR->topology->getPeer(zta));
			if (pcopy.dearmor(rp->key())) {
				printf("%s\n",Packet::verbString(pcopy.verb()));
			} else printf("!!!!\n");
		}
	}
	*/
	if (RR->node->putPacket(_localAddress,address(),data,len)) {
		sent(now);
		return true;
	}
	return false;
}

} // namespace ZeroTier
