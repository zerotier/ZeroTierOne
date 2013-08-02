/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

/* Self-tester that makes both new and repeated requests to netconf */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <vector>
#include <string>
#include <iostream>

#include "../node/Dictionary.hpp"
#include "../node/Service.hpp"
#include "../node/Identity.hpp"
#include "../node/RuntimeEnvironment.hpp"
#include "../node/Logger.hpp"
#include "../node/Thread.hpp"

using namespace ZeroTier;

static void svcHandler(void *arg,Service &svc,const Dictionary &msg)
{
	std::cout << msg.toString();
}

int main(int argc,char **argv)
{
	RuntimeEnvironment renv;
	renv.log = new Logger((const char *)0,(const char *)0,0);
	Service svc(&renv,"netconf","./netconf.service",&svcHandler,(void *)0);

	srand(time(0));

	std::vector<Identity> population;
	for(;;) {
		Identity id;
		if ((population.empty())||(rand() < (RAND_MAX / 4))) {
			id.generate();
			population.push_back(id);
			std::cout << "Testing with new identity: " << id.address().toString() << std::endl;
		} else {
			id = population[rand() % population.size()];
			Thread::sleep(1000);
			std::cout << "Testing with existing identity: " << id.address().toString() << std::endl;
		}

		Dictionary request;
		request["type"] = "netconf-request";
		request["peerId"] = id.toString(false);
		request["nwid"] = "6c92786fee000001";
		request["requestId"] = "12345";

		svc.send(request);
	}
}
