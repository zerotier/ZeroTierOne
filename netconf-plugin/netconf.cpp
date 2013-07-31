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

/*
 * This is the netconf service. It's currently used only by netconf nodes that
 * are run by ZeroTier itself. There is nothing to prevent you from running
 * your own if you wanted to create your own networks outside our system.
 *
 * That being said, we'd like to charge for private networks to support
 * ZeroTier One and future development efforts. So while this software is
 * open source and we're not going to stop you from sidestepping this, we
 * do ask -- honor system here -- that you pay for private networks if you
 * are going to use them for any commercial purpose such as a business VPN
 * alternative.
 *
 * This will at the moment only build on Linux and requires the mysql++
 * library, which is available here:
 *
 * http://tangentsoft.net/mysql++/
 *
 * (Packages are available for CentOS via EPEL and for any Debian distro.)
 *
 * This program must be built and installed in the services.d subfolder of
 * the ZeroTier One home folder of the node designated to act as a master
 * for networks. Doing so will enable the NETWORK_CONFIG_REQUEST protocol
 * verb.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <algorithm>

#include <mysql++.h>

#include "../node/Dictionary.hpp"

using namespace ZeroTier;
using namespace mysqlpp;

static Connection *dbCon = (Connection *)0;

static void connectOrReconnect()
{
	if (dbCon)
		delete dbCon;
	dbCon = new Connection(mysqlDatabase,mysqlHost,mysqlUser,mysqlPassword,(unsigned int)strtol(mysqlPort,(char **)0,10));
	if (dbCon->connected())
		break;
	else {
		fprintf(stderr,"Unable to connect to database server.\n");
		usleep(1000);
	}
}

int main(int argc,char **argv)
{
	char mysqlHost[64],mysqlPort[64],mysqlDatabase[64],mysqlUser[64],mysqlPassword[64];

	{
		char *ee = getenv("ZT_NETCONF_MYSQL_HOST");
		if (!ee) {
			fprintf(stderr,"Missing environment variable: ZT_NETCONF_MYSQL_HOST\n");
			return -1;
		}
		strcpy(mysqlHost,ee);
		ee = getenv("ZT_NETCONF_MYSQL_PORT");
		if (ee == null)
			strcpy(mysqlPort,"3306");
		else strcpy(mysqlPort,ee);
		ee = getenv("ZT_NETCONF_MYSQL_DATABASE");
		if (!ee) {
			fprintf(stderr,"Missing environment variable: ZT_NETCONF_MYSQL_DATABASE\n");
			return -1;
		}
		strcpy(mysqlDatabase,ee);
		ee = getenv("ZT_NETCONF_MYSQL_USER");
		if (!ee) {
			fprintf(stderr,"Missing environment variable: ZT_NETCONF_MYSQL_USER\n");
			return -1;
		}
		strcpy(mysqlUser,ee);
		ee = getenv("ZT_NETCONF_MYSQL_PASSWORD");
		if (!ee) {
			fprintf(stderr,"Missing environment variable: ZT_NETCONF_MYSQL_PASSWORD\n");
			return -1;
		}
		strcpy(mysqlPassword,ee);
	}

	connectOrReconnect();
	for(;;) {
		if (!dbCon->connected())
			connectOrReconnect();
	}
}
