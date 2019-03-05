/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */
 
#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_LIBPQ_HPP
#define ZT_CONTROLLER_LIBPQ_HPP

#include "DB.hpp"

#define ZT_CENTRAL_CONTROLLER_COMMIT_THREADS 4

extern "C" {
    typedef struct pg_conn PGconn;
}

namespace ZeroTier
{

struct MQConfig;

/**
 * A controller database driver that talks to PostgreSQL
 *
 * This is for use with ZeroTier Central.  Others are free to build and use it
 * but be aware taht we might change it at any time.
 */
class PostgreSQL : public DB
{
public:
    PostgreSQL(EmbeddedNetworkController *const nc, const Identity &myId, const char *path, int listenPort, MQConfig *mqc = NULL);
    virtual ~PostgreSQL();

    virtual bool waitForReady();
    virtual bool isReady();
    virtual void save(nlohmann::json *orig, nlohmann::json &record);
    virtual void eraseNetwork(const uint64_t networkId);
    virtual void eraseMember(const uint64_t networkId, const uint64_t memberId);
    virtual void nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress &physicalAddress);

protected:
    struct _PairHasher
	{
		inline std::size_t operator()(const std::pair<uint64_t,uint64_t> &p) const { return (std::size_t)(p.first ^ p.second); }
	};

private:
    void initializeNetworks(PGconn *conn);
    void initializeMembers(PGconn *conn);
    void heartbeat();
    void membersDbWatcher();
    void _membersWatcher_Postgres(PGconn *conn);
    void _membersWatcher_RabbitMQ();
    void networksDbWatcher();
    void _networksWatcher_Postgres(PGconn *conn);
    void _networksWatcher_RabbitMQ();


    void commitThread();
    void onlineNotificationThread();

    enum OverrideMode {
        ALLOW_PGBOUNCER_OVERRIDE = 0,
        NO_OVERRIDE = 1
    };

    PGconn * getPgConn( OverrideMode m = ALLOW_PGBOUNCER_OVERRIDE );

    std::string _connString;

    BlockingQueue<nlohmann::json *> _commitQueue;


    std::thread _heartbeatThread;
    std::thread _membersDbWatcher;
    std::thread _networksDbWatcher;
    std::thread _commitThread[ZT_CENTRAL_CONTROLLER_COMMIT_THREADS];
    std::thread _onlineNotificationThread;

	std::unordered_map< std::pair<uint64_t,uint64_t>,std::pair<int64_t,InetAddress>,_PairHasher > _lastOnline;

    mutable std::mutex _lastOnline_l;
    mutable std::mutex _readyLock;
    std::atomic<int> _ready, _connected, _run;
    mutable volatile bool _waitNoticePrinted;

    int _listenPort;

    MQConfig *_mqc;
};

}

#endif // ZT_CONTROLLER_LIBPQ_HPP

#endif // ZT_CONTROLLER_USE_LIBPQ