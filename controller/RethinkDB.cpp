#include "RethinkDB.hpp"

#include <chrono>
#include <algorithm>
#include <stdexcept>

#include "../ext/librethinkdbxx/build/include/rethinkdb.h"

namespace R = RethinkDB;
using nlohmann::json;

namespace ZeroTier {

RethinkDB::RethinkDB(const Address &myAddress,const char *host,const int port,const char *db,const char *auth) :
	_myAddress(myAddress),
	_host(host ? host : "127.0.0.1"),
	_db(db),
	_auth(auth ? auth : ""),
	_port((port > 0) ? port : 28015),
	_ready(2), // two tables need to be synchronized before we're ready
	_run(1)
{
	_readyLock.lock();

	{
		char tmp[32];
		_myAddress.toString(tmp);
		_myAddressStr = tmp;
	}

	_membersDbWatcher = std::thread([this]() {
		while (_run == 1) {
			try {
				auto rdb = R::connect(this->_host,this->_port,this->_auth);
				if (rdb) {
					_membersDbWatcherConnection = (void *)rdb.get();
					auto cur = R::db(this->_db).table("Member").get_all(this->_myAddressStr,R::optargs("index","controllerId")).changes(R::optargs("squash",0.1,"include_initial",true,"include_types",true,"include_states",true)).run(*rdb);
					while (cur.has_next()) {
						if (_run != 1) break;
						json tmp(json::parse(cur.next().as_json()));
						if ((tmp["type"] == "state")&&(tmp["state"] == "ready")) {
							if (--this->_ready == 0)
								this->_readyLock.unlock();
						} else {
							try {
								this->_memberChanged(tmp["old_val"],tmp["new_val"]);
							} catch ( ... ) {} // ignore bad records
						}
					}
				}
			} catch (std::exception &e) {
				fprintf(stderr,"ERROR: controller RethinkDB: %s" ZT_EOL_S,e.what());
			} catch (R::Error &e) {
				fprintf(stderr,"ERROR: controller RethinkDB: %s" ZT_EOL_S,e.message.c_str());
			} catch ( ... ) {
				fprintf(stderr,"ERROR: controller RethinkDB: unknown exception" ZT_EOL_S);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
	});

	_networksDbWatcher = std::thread([this]() {
		while (_run == 1) {
			try {
				auto rdb = R::connect(this->_host,this->_port,this->_auth);
				if (rdb) {
					_membersDbWatcherConnection = (void *)rdb.get();
					auto cur = R::db(this->_db).table("Network").get_all(this->_myAddressStr,R::optargs("index","controllerId")).changes(R::optargs("squash",0.1,"include_initial",true,"include_types",true,"include_states",true)).run(*rdb);
					while (cur.has_next()) {
						if (_run != 1) break;
						json tmp(json::parse(cur.next().as_json()));
						if ((tmp["type"] == "state")&&(tmp["state"] == "ready")) {
							if (--this->_ready == 0)
								this->_readyLock.unlock();
						} else {
							try {
								this->_networkChanged(tmp["old_val"],tmp["new_val"]);
							} catch ( ... ) {} // ignore bad records
						}
					}
				}
			} catch (std::exception &e) {
				fprintf(stderr,"ERROR: controller RethinkDB: %s" ZT_EOL_S,e.what());
			} catch (R::Error &e) {
				fprintf(stderr,"ERROR: controller RethinkDB: %s" ZT_EOL_S,e.message.c_str());
			} catch ( ... ) {
				fprintf(stderr,"ERROR: controller RethinkDB: unknown exception" ZT_EOL_S);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
	});
}

RethinkDB::~RethinkDB()
{
	// FIXME: not totally safe but will generally work, and only happens on shutdown anyway
	_run = 0;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	if (_membersDbWatcherConnection)
		((R::Connection *)_membersDbWatcherConnection)->close();
	if (_networksDbWatcherConnection)
		((R::Connection *)_networksDbWatcherConnection)->close();
	_membersDbWatcher.join();
	_networksDbWatcher.join();
}

inline bool RethinkDB::get(const uint64_t networkId,nlohmann::json &network)
{
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}

	std::lock_guard<std::mutex> l2(nw->lock);
	network = nw->config;

	return true;
}

inline bool RethinkDB::get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,NetworkSummaryInfo &info)
{
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}

	std::lock_guard<std::mutex> l2(nw->lock);
		auto m = nw->members.find(memberId);
	if (m == nw->members.end())
		return false;
	network = nw->config;
	member = m->second;
	_fillSummaryInfo(nw,info);

	return true;
}

inline bool RethinkDB::get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members)
{
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}

	std::lock_guard<std::mutex> l2(nw->lock);
	network = nw->config;
	for(auto m=nw->members.begin();m!=nw->members.end();++m)
		members.push_back(m->second);

	return true;
}

inline bool RethinkDB::summary(const uint64_t networkId,NetworkSummaryInfo &info)
{
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}

	std::lock_guard<std::mutex> l2(nw->lock);
	_fillSummaryInfo(nw,info);

	return true;
}

void RethinkDB::_memberChanged(nlohmann::json &old,nlohmann::json &member)
{
	uint64_t memberId = 0;
	uint64_t networkId = 0;
	std::shared_ptr<_Network> nw;

	if (old.is_object()) {
		json &config = old["config"];
		if (config.is_object()) {
			memberId = OSUtils::jsonIntHex(config["id"],0ULL);
			networkId = OSUtils::jsonIntHex(config["nwid"],0ULL);
			if ((memberId)&&(networkId)) {
				{
					std::lock_guard<std::mutex> l(_networks_l);
					auto nw2 = _networks.find(networkId);
					if (nw2 != _networks.end())
						nw = nw2->second;
				}
				if (nw) {
					std::lock_guard<std::mutex> l(nw->lock);
					if (OSUtils::jsonBool(config["activeBridge"],false))
						nw->activeBridgeMembers.erase(memberId);
					if (OSUtils::jsonBool(config["authorized"],false))
						nw->authorizedMembers.erase(memberId);
					json &ips = config["ipAssignments"];
					if (ips.is_array()) {
						for(unsigned long i=0;i<ips.size();++i) {
							json &ipj = ips[i];
							if (ipj.is_string()) {
								const std::string ips = ipj;
								InetAddress ipa(ips.c_str());
								ipa.setPort(0);
								nw->allocatedIps.erase(ipa);
							}
						}
					}
				}
			}
		}
	}

	if (member.is_object()) {
		json &config = member["config"];
		if (config.is_object()) {
			if (!nw) {
				memberId = OSUtils::jsonIntHex(config["id"],0ULL);
				networkId = OSUtils::jsonIntHex(config["nwid"],0ULL);
				if ((!memberId)||(!networkId))
					return;
				std::lock_guard<std::mutex> l(_networks_l);
				std::shared_ptr<_Network> &nw2 = _networks[networkId];
				if (!nw2)
					nw2.reset(new _Network);
				nw = nw2;
			}
			std::lock_guard<std::mutex> l(nw->lock);

			nw->members[memberId] = config;

			if (OSUtils::jsonBool(config["activeBridge"],false))
				nw->activeBridgeMembers.insert(memberId);
			const bool isAuth = OSUtils::jsonBool(config["authorized"],false);
			if (isAuth)
				nw->authorizedMembers.insert(memberId);
			json &ips = config["ipAssignments"];
			if (ips.is_array()) {
				for(unsigned long i=0;i<ips.size();++i) {
					json &ipj = ips[i];
					if (ipj.is_string()) {
						const std::string ips = ipj;
						InetAddress ipa(ips.c_str());
						ipa.setPort(0);
						nw->allocatedIps.insert(ipa);
					}
				}
			}

			if (!isAuth) {
				const int64_t ldt = (int64_t)OSUtils::jsonInt(config["lastDeauthorizedTime"],0ULL);
				if (ldt > nw->mostRecentDeauthTime)
					nw->mostRecentDeauthTime = ldt;
			}
		}
	}
}

void RethinkDB::_networkChanged(nlohmann::json &old,nlohmann::json &network)
{
	if (network.is_object()) {
		json &config = network["config"];
		if (config.is_object()) {
			const std::string ids = config["id"];
			const uint64_t id = Utils::hexStrToU64(ids.c_str());
			if (id) {
				std::shared_ptr<_Network> nw;
				{
					std::lock_guard<std::mutex> l(_networks_l);
					std::shared_ptr<_Network> &nw2 = _networks[id];
					if (!nw2)
						nw2.reset(new _Network);
					nw = nw2;
				}
				std::lock_guard<std::mutex> l2(nw->lock);
				nw->config = config;
			}
		}
	} else if (old.is_object()) {
		const std::string ids = old["id"];
		const uint64_t id = Utils::hexStrToU64(ids.c_str());
		if (id) {
			std::lock_guard<std::mutex> l(_networks_l);
			_networks.erase(id);
		}
	}
}

} // namespace ZeroTier

/*
int main(int argc,char **argv)
{
	ZeroTier::RethinkDB db(ZeroTier::Address(0x8056c2e21cULL),"10.6.6.188",28015,"ztc","");
	db.waitForReady();
	printf("ready.\n");
	pause();
}
*/
