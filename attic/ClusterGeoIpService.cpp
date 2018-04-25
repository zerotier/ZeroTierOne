/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifdef ZT_ENABLE_CLUSTER

#include <math.h>

#include <cmath>

#include "ClusterGeoIpService.hpp"

#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"

#define ZT_CLUSTERGEOIPSERVICE_FILE_MODIFICATION_CHECK_EVERY 10000

namespace ZeroTier {

ClusterGeoIpService::ClusterGeoIpService() :
	_pathToCsv(),
	_ipStartColumn(-1),
	_ipEndColumn(-1),
	_latitudeColumn(-1),
	_longitudeColumn(-1),
	_lastFileCheckTime(0),
	_csvModificationTime(0),
	_csvFileSize(0)
{
}

ClusterGeoIpService::~ClusterGeoIpService()
{
}

bool ClusterGeoIpService::locate(const InetAddress &ip,int &x,int &y,int &z)
{
	Mutex::Lock _l(_lock);

	if ((_pathToCsv.length() > 0)&&((OSUtils::now() - _lastFileCheckTime) > ZT_CLUSTERGEOIPSERVICE_FILE_MODIFICATION_CHECK_EVERY)) {
		_lastFileCheckTime = OSUtils::now();
		if ((_csvFileSize != OSUtils::getFileSize(_pathToCsv.c_str()))||(_csvModificationTime != OSUtils::getLastModified(_pathToCsv.c_str())))
			_load(_pathToCsv.c_str(),_ipStartColumn,_ipEndColumn,_latitudeColumn,_longitudeColumn);
	}

	/* We search by looking up the upper bound of the sorted vXdb vectors
	 * and then iterating down for a matching IP range. We stop when we hit
	 * the beginning or an entry whose start and end are before the IP we
	 * are searching. */

	if ((ip.ss_family == AF_INET)&&(_v4db.size() > 0)) {
		_V4E key;
		key.start = Utils::ntoh((uint32_t)(reinterpret_cast<const struct sockaddr_in *>(&ip)->sin_addr.s_addr));
		std::vector<_V4E>::const_iterator i(std::upper_bound(_v4db.begin(),_v4db.end(),key));
		while (i != _v4db.begin()) {
			--i;
			if ((key.start >= i->start)&&(key.start <= i->end)) {
				x = i->x;
				y = i->y;
				z = i->z;
				//printf("%s : %f,%f %d,%d,%d\n",ip.toIpString().c_str(),i->lat,i->lon,x,y,z);
				return true;
			} else if ((key.start > i->start)&&(key.start > i->end))
				break;
		}
	} else if ((ip.ss_family == AF_INET6)&&(_v6db.size() > 0)) {
		_V6E key;
		memcpy(key.start,reinterpret_cast<const struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		std::vector<_V6E>::const_iterator i(std::upper_bound(_v6db.begin(),_v6db.end(),key));
		while (i != _v6db.begin()) {
			--i;
			const int s_vs_s = memcmp(key.start,i->start,16);
			const int s_vs_e = memcmp(key.start,i->end,16);
			if ((s_vs_s >= 0)&&(s_vs_e <= 0)) {
				x = i->x;
				y = i->y;
				z = i->z;
				//printf("%s : %f,%f %d,%d,%d\n",ip.toIpString().c_str(),i->lat,i->lon,x,y,z);
				return true;
			} else if ((s_vs_s > 0)&&(s_vs_e > 0))
				break;
		}
	}

	return false;
}

void ClusterGeoIpService::_parseLine(const char *line,std::vector<_V4E> &v4db,std::vector<_V6E> &v6db,int ipStartColumn,int ipEndColumn,int latitudeColumn,int longitudeColumn)
{
	std::vector<std::string> ls(OSUtils::split(line,",\t","\\","\"'"));
	if ( ((ipStartColumn >= 0)&&(ipStartColumn < (int)ls.size()))&&
	     ((ipEndColumn >= 0)&&(ipEndColumn < (int)ls.size()))&&
	     ((latitudeColumn >= 0)&&(latitudeColumn < (int)ls.size()))&&
	     ((longitudeColumn >= 0)&&(longitudeColumn < (int)ls.size())) ) {
		InetAddress ipStart(ls[ipStartColumn].c_str(),0);
		InetAddress ipEnd(ls[ipEndColumn].c_str(),0);
		const double lat = strtod(ls[latitudeColumn].c_str(),(char **)0);
		const double lon = strtod(ls[longitudeColumn].c_str(),(char **)0);

		if ((ipStart.ss_family == ipEnd.ss_family)&&(ipStart)&&(ipEnd)&&(std::isfinite(lat))&&(std::isfinite(lon))) {
			const double latRadians = lat * 0.01745329251994; // PI / 180
			const double lonRadians = lon * 0.01745329251994; // PI / 180
			const double cosLat = cos(latRadians);
			const int x = (int)round((-6371.0) * cosLat * cos(lonRadians)); // 6371 == Earth's approximate radius in kilometers
			const int y = (int)round(6371.0 * sin(latRadians));
			const int z = (int)round(6371.0 * cosLat * sin(lonRadians));

			if (ipStart.ss_family == AF_INET) {
				v4db.push_back(_V4E());
				v4db.back().start = Utils::ntoh((uint32_t)(reinterpret_cast<const struct sockaddr_in *>(&ipStart)->sin_addr.s_addr));
				v4db.back().end = Utils::ntoh((uint32_t)(reinterpret_cast<const struct sockaddr_in *>(&ipEnd)->sin_addr.s_addr));
				v4db.back().lat = (float)lat;
				v4db.back().lon = (float)lon;
				v4db.back().x = x;
				v4db.back().y = y;
				v4db.back().z = z;
				//printf("%s - %s : %d,%d,%d\n",ipStart.toIpString().c_str(),ipEnd.toIpString().c_str(),x,y,z);
			} else if (ipStart.ss_family == AF_INET6) {
				v6db.push_back(_V6E());
				memcpy(v6db.back().start,reinterpret_cast<const struct sockaddr_in6 *>(&ipStart)->sin6_addr.s6_addr,16);
				memcpy(v6db.back().end,reinterpret_cast<const struct sockaddr_in6 *>(&ipEnd)->sin6_addr.s6_addr,16);
				v6db.back().lat = (float)lat;
				v6db.back().lon = (float)lon;
				v6db.back().x = x;
				v6db.back().y = y;
				v6db.back().z = z;
				//printf("%s - %s : %d,%d,%d\n",ipStart.toIpString().c_str(),ipEnd.toIpString().c_str(),x,y,z);
			}
		}
	}
}

long ClusterGeoIpService::_load(const char *pathToCsv,int ipStartColumn,int ipEndColumn,int latitudeColumn,int longitudeColumn)
{
	// assumes _lock is locked

	FILE *f = fopen(pathToCsv,"rb");
	if (!f)
		return -1;

	std::vector<_V4E> v4db;
	std::vector<_V6E> v6db;
	v4db.reserve(16777216);
	v6db.reserve(16777216);

	char buf[4096];
	char linebuf[1024];
	unsigned int lineptr = 0;
	for(;;) {
		int n = (int)fread(buf,1,sizeof(buf),f);
		if (n <= 0)
			break;
		for(int i=0;i<n;++i) {
			if ((buf[i] == '\r')||(buf[i] == '\n')||(buf[i] == (char)0)) {
				if (lineptr) {
					linebuf[lineptr] = (char)0;
					_parseLine(linebuf,v4db,v6db,ipStartColumn,ipEndColumn,latitudeColumn,longitudeColumn);
				}
				lineptr = 0;
			} else if (lineptr < (unsigned int)sizeof(linebuf))
				linebuf[lineptr++] = buf[i];
		}
	}
	if (lineptr) {
		linebuf[lineptr] = (char)0;
		_parseLine(linebuf,v4db,v6db,ipStartColumn,ipEndColumn,latitudeColumn,longitudeColumn);
	}

	fclose(f);

	if ((v4db.size() > 0)||(v6db.size() > 0)) {
		std::sort(v4db.begin(),v4db.end());
		std::sort(v6db.begin(),v6db.end());

		_pathToCsv = pathToCsv;
		_ipStartColumn = ipStartColumn;
		_ipEndColumn = ipEndColumn;
		_latitudeColumn = latitudeColumn;
		_longitudeColumn = longitudeColumn;

		_lastFileCheckTime = OSUtils::now();
		_csvModificationTime = OSUtils::getLastModified(pathToCsv);
		_csvFileSize = OSUtils::getFileSize(pathToCsv);

		_v4db.swap(v4db);
		_v6db.swap(v6db);

		return (long)(_v4db.size() + _v6db.size());
	} else {
		return 0;
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER

/*
int main(int argc,char **argv)
{
	char buf[1024];

	ZeroTier::ClusterGeoIpService gip;
	printf("loading...\n");
	gip.load("/Users/api/Code/ZeroTier/Infrastructure/root-servers/zerotier-one/cluster-geoip.csv",0,1,5,6);
	printf("... done!\n"); fflush(stdout);

	while (gets(buf)) { // unsafe, testing only
		ZeroTier::InetAddress addr(buf,0);
		printf("looking up: %s\n",addr.toString().c_str()); fflush(stdout);
		int x = 0,y = 0,z = 0;
		if (gip.locate(addr,x,y,z)) {
			//printf("%s: %d,%d,%d\n",addr.toString().c_str(),x,y,z); fflush(stdout);
		} else {
			printf("%s: not found!\n",addr.toString().c_str()); fflush(stdout);
		}
	}

	return 0;
}
*/
