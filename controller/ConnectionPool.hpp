/*
 * Copyright (c)2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_CONNECTION_POOL_H_
#define ZT_CONNECTION_POOL_H_


#ifndef _DEBUG
	#define _DEBUG(x)
#endif

#include "../node/Metrics.hpp"

#include <deque>
#include <set>
#include <memory>
#include <mutex>
#include <exception>
#include <string>

namespace ZeroTier {

struct ConnectionUnavailable : std::exception { 
    char const* what() const throw() {
        return "Unable to allocate connection";
    }; 
};


class Connection {
public:
    virtual ~Connection() {};
};

class ConnectionFactory {
public:
    virtual ~ConnectionFactory() {};
    virtual std::shared_ptr<Connection> create()=0;
};

struct ConnectionPoolStats {
    size_t pool_size;
    size_t borrowed_size;
};

template<class T>
class ConnectionPool {
public:
    ConnectionPool(size_t max_pool_size, size_t min_pool_size, std::shared_ptr<ConnectionFactory> factory)
        : m_maxPoolSize(max_pool_size)
        , m_minPoolSize(min_pool_size)
        , m_factory(factory)
    {
        Metrics::max_pool_size += max_pool_size;
        Metrics::min_pool_size += min_pool_size;
        while(m_pool.size() < m_minPoolSize){
            m_pool.push_back(m_factory->create());
            Metrics::pool_avail++;
        }
    };

    ConnectionPoolStats get_stats() {
        std::unique_lock<std::mutex> lock(m_poolMutex);

        ConnectionPoolStats stats;
        stats.pool_size = m_pool.size();
        stats.borrowed_size = m_borrowed.size();			

        return stats;
    };

    ~ConnectionPool() {
    };

    /**
     * Borrow
     *
     * Borrow a connection for temporary use
     *
     * When done, either (a) call unborrow() to return it, or (b) (if it's bad) just let it go out of scope.  This will cause it to automatically be replaced.
     * @retval a shared_ptr to the connection object
     */
    std::shared_ptr<T> borrow() {
        std::unique_lock<std::mutex> l(m_poolMutex);
        
        while((m_pool.size() + m_borrowed.size()) < m_minPoolSize) {
            std::shared_ptr<Connection> conn = m_factory->create();
            m_pool.push_back(conn);
            Metrics::pool_avail++;
        }

        if(m_pool.size()==0){
            
            if ((m_pool.size() + m_borrowed.size()) < m_maxPoolSize) {
                try {
                    std::shared_ptr<Connection> conn = m_factory->create();
                    m_borrowed.insert(conn);
                    Metrics::pool_in_use++;
                    return std::static_pointer_cast<T>(conn);
                } catch (std::exception &e) {
                    Metrics::pool_errors++;
                    throw ConnectionUnavailable();
                }
            } else {
                for(auto it = m_borrowed.begin(); it != m_borrowed.end(); ++it){
                    if((*it).unique()) {
                        // This connection has been abandoned! Destroy it and create a new connection
                        try {
                            // If we are able to create a new connection, return it
                            _DEBUG("Creating new connection to replace discarded connection");
                            std::shared_ptr<Connection> conn = m_factory->create();
                            m_borrowed.erase(it);
                            m_borrowed.insert(conn);
                            return std::static_pointer_cast<T>(conn);
                        } catch(std::exception& e) {
                            // Error creating a replacement connection
                            Metrics::pool_errors++;
                            throw ConnectionUnavailable();
                        }
                    }
                }
                // Nothing available
                Metrics::pool_errors++;
                throw ConnectionUnavailable();
            }
        }

        // Take one off the front
        std::shared_ptr<Connection> conn = m_pool.front();
        m_pool.pop_front();
        Metrics::pool_avail--;
        // Add it to the borrowed list
        m_borrowed.insert(conn);
        Metrics::pool_in_use++;
        return std::static_pointer_cast<T>(conn);
    };

    /**
     * Unborrow a connection
     *
     * Only call this if you are returning a working connection.  If the connection was bad, just let it go out of scope (so the connection manager can replace it).
     * @param the connection
     */
    void unborrow(std::shared_ptr<T> conn) {
        // Lock
        std::unique_lock<std::mutex> lock(m_poolMutex);
        m_borrowed.erase(conn);
        Metrics::pool_in_use--;
        if ((m_pool.size() + m_borrowed.size()) < m_maxPoolSize) {
            Metrics::pool_avail++;
            m_pool.push_back(conn);
        }
    };
protected:
    size_t m_maxPoolSize;
    size_t m_minPoolSize;
    std::shared_ptr<ConnectionFactory> m_factory;
    std::deque<std::shared_ptr<Connection> > m_pool;
    std::set<std::shared_ptr<Connection> > m_borrowed;
    std::mutex m_poolMutex;
};

}

#endif
