/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#include "TestEthernetTap.hpp"
#include "TestEthernetTapFactory.hpp"
#include "../node/Utils.hpp"

namespace ZeroTier {

TestEthernetTap::TestEthernetTap(
	TestEthernetTapFactory *parent,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *desiredDevice,
	const char *friendlyName,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg) :
	EthernetTap("TestEthernetTap",mac,mtu,metric),
	_parent(parent),
	_handler(handler),
	_arg(arg),
	_enabled(true)
{
	char tmp[64];
	Utils::snprintf(tmp,sizeof(tmp),"%.16llx",(unsigned long long)nwid);
	_dev = tmp;
	_thread = Thread::start(this);
}

TestEthernetTap::~TestEthernetTap()
{
	{
		Mutex::Lock _l(_pq_m);
		_pq.push(TestFrame()); // 0-length frame = exit
	}
	_pq_c.signal();
	Thread::join(_thread);
}

void TestEthernetTap::setEnabled(bool en)
{
	_enabled = en;
}

bool TestEthernetTap::enabled() const
{
	return _enabled;
}

bool TestEthernetTap::addIP(const InetAddress &ip)
{
	return true;
}

bool TestEthernetTap::removeIP(const InetAddress &ip)
{
	return true;
}

std::set<InetAddress> TestEthernetTap::ips() const
{
	return std::set<InetAddress>();
}

void TestEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	static Mutex printLock;
	Mutex::Lock _l(printLock);
	fprintf(stderr,"%s << %s %.4x %s"ZT_EOL_S,to.toString().c_str(),from.toString().c_str(),etherType,std::string((const char *)data,len).c_str());
}

std::string TestEthernetTap::deviceName() const
{
	return _dev;
}

void TestEthernetTap::setFriendlyName(const char *friendlyName)
{
}

bool TestEthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	return false;
}

void TestEthernetTap::threadMain()
	throw()
{
	TestFrame tf;
	for(;;) {
		tf.len = 0;
		{
			Mutex::Lock _l(_pq_m);
			if (!_pq.empty()) {
				if (_pq.front().len == 0)
					break;
				memcpy(&tf,&(_pq.front()),sizeof(tf));
				_pq.pop();
			}
		}

		if ((tf.len > 0)&&(_enabled))
			_handler(_arg,tf.from,tf.to,ZT_TEST_ETHERNET_ETHERTYPE,Buffer<4096>(tf.data,tf.len));

		_pq_c.wait();
	}
}

} // namespace ZeroTier
