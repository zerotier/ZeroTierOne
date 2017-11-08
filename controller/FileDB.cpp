/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#include "FileDB.hpp"

namespace ZeroTier
{

FileDB::FileDB(EmbeddedNetworkController *const nc,const Address &myAddress,const char *path) :
	DB(nc,myAddress,path),
	_networksPath(_path + ZT_PATH_SEPARATOR_S + "network")
{
	OSUtils::mkdir(_path.c_str());
	OSUtils::lockDownFile(_path.c_str(),true);

	std::vector<std::string> networks(OSUtils::listDirectory(_networksPath.c_str(),false));
	std::string buf;
	for(auto n=networks.begin();n!=networks.end();++n) {
		buf.clear();
		if ((n->length() == 21)&&(OSUtils::readFile((_networksPath + ZT_PATH_SEPARATOR_S + *n).c_str(),buf))) {
			try {
				nlohmann::json network(OSUtils::jsonParse(buf));
				const std::string nwids = network["id"];
				if (nwids.length() == 16) {
					nlohmann::json nullJson;
					_networkChanged(nullJson,network,false);
					std::string membersPath(_networksPath + ZT_PATH_SEPARATOR_S + nwids + ZT_PATH_SEPARATOR_S "member");
					std::vector<std::string> members(OSUtils::listDirectory(membersPath.c_str(),false));
					for(auto m=members.begin();m!=members.end();++m) {
						buf.clear();
						if ((m->length() == 15)&&(OSUtils::readFile((membersPath + ZT_PATH_SEPARATOR_S + *m).c_str(),buf))) {
							try {
								nlohmann::json member(OSUtils::jsonParse(buf));
								const std::string addrs = member["id"];
								if (addrs.length() == 10) {
									nlohmann::json nullJson2;
									_memberChanged(nullJson2,member,false);
								}
							} catch ( ... ) {}
						}
					}
				}
			} catch ( ... ) {}
		}
	}
}

FileDB::~FileDB()
{
}

bool FileDB::waitForReady()
{
	return true;
}

void FileDB::save(nlohmann::json *orig,nlohmann::json &record)
{
	char p1[16384],p2[16384];
	try {
		nlohmann::json rec(record);
		const std::string objtype = rec["objtype"];
		if (objtype == "network") {
			const uint64_t nwid = OSUtils::jsonIntHex(rec["id"],0ULL);
			if (nwid) {
				nlohmann::json old;
				get(nwid,old);

				OSUtils::ztsnprintf(p1,sizeof(p1),"%s" ZT_PATH_SEPARATOR_S "%.16llx.json.new",_networksPath.c_str(),nwid);
				OSUtils::ztsnprintf(p2,sizeof(p2),"%s" ZT_PATH_SEPARATOR_S "%.16llx.json",_networksPath.c_str(),nwid);
				if (!OSUtils::writeFile(p1,OSUtils::jsonDump(rec,-1)))
					fprintf(stderr,"WARNING: controller unable to write to path: %s" ZT_EOL_S,p1);
				OSUtils::rename(p1,p2);

				_networkChanged(old,rec,true);
			}
		} else if (objtype == "member") {
			const uint64_t id = OSUtils::jsonIntHex(rec["id"],0ULL);
			const uint64_t nwid = OSUtils::jsonIntHex(rec["nwid"],0ULL);
			if ((id)&&(nwid)) {
				nlohmann::json network,old;
				get(nwid,network,id,old);

				OSUtils::ztsnprintf(p1,sizeof(p1),"%s" ZT_PATH_SEPARATOR_S "%.16llx" ZT_PATH_SEPARATOR_S "member" ZT_PATH_SEPARATOR_S "%.10llx.json.new",_networksPath.c_str(),nwid);
				OSUtils::ztsnprintf(p2,sizeof(p2),"%s" ZT_PATH_SEPARATOR_S "%.16llx" ZT_PATH_SEPARATOR_S "member" ZT_PATH_SEPARATOR_S "%.10llx.json",_networksPath.c_str(),nwid);
				if (!OSUtils::writeFile(p1,OSUtils::jsonDump(rec,-1)))
					fprintf(stderr,"WARNING: controller unable to write to path: %s" ZT_EOL_S,p1);
				OSUtils::rename(p1,p2);

				_memberChanged(old,rec,true);
			}
		} else if (objtype == "trace") {
			const std::string id = rec["id"];
			OSUtils::ztsnprintf(p1,sizeof(p1),"%s" ZT_PATH_SEPARATOR_S "trace" ZT_PATH_SEPARATOR_S "%s.json",_path.c_str(),id.c_str());
			OSUtils::writeFile(p1,OSUtils::jsonDump(rec,-1));
		}
	} catch ( ... ) {} // drop invalid records missing fields
}

void FileDB::eraseNetwork(const uint64_t networkId)
{
	nlohmann::json network,nullJson;
	get(networkId,network);
	char p[16384];
	OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.16llx.json",_networksPath.c_str(),networkId);
	OSUtils::rm(p);
	_networkChanged(network,nullJson,true);
}

void FileDB::eraseMember(const uint64_t networkId,const uint64_t memberId)
{
}

void FileDB::nodeIsOnline(const uint64_t networkId,const uint64_t memberId)
{
	// Nothing to do here right now in the filesystem store mode since we can just get this from the peer list
}

} // namespace ZeroTier
