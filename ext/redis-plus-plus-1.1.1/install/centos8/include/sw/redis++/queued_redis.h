/**************************************************************************
   Copyright (c) 2017 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#ifndef SEWENEW_REDISPLUSPLUS_QUEUED_REDIS_H
#define SEWENEW_REDISPLUSPLUS_QUEUED_REDIS_H

#include <cassert>
#include <chrono>
#include <initializer_list>
#include <vector>
#include "connection.h"
#include "utils.h"
#include "reply.h"
#include "command.h"
#include "redis.h"

namespace sw {

namespace redis {

class QueuedReplies;

// If any command throws, QueuedRedis resets the connection, and becomes invalid.
// In this case, the only thing we can do is to destory the QueuedRedis object.
template <typename Impl>
class QueuedRedis {
public:
    QueuedRedis(QueuedRedis &&) = default;
    QueuedRedis& operator=(QueuedRedis &&) = default;

    // When it destructs, the underlying *Connection* will be closed,
    // and any command that has NOT been executed will be ignored.
    ~QueuedRedis() = default;

    Redis redis();

    template <typename Cmd, typename ...Args>
    auto command(Cmd cmd, Args &&...args)
        -> typename std::enable_if<!std::is_convertible<Cmd, StringView>::value,
                                    QueuedRedis&>::type;

    template <typename ...Args>
    QueuedRedis& command(const StringView &cmd_name, Args &&...args);

    template <typename Input>
    auto command(Input first, Input last)
        -> typename std::enable_if<IsIter<Input>::value, QueuedRedis&>::type;

    QueuedReplies exec();

    void discard();

    // CONNECTION commands.

    QueuedRedis& auth(const StringView &password) {
        return command(cmd::auth, password);
    }

    QueuedRedis& echo(const StringView &msg) {
        return command(cmd::echo, msg);
    }

    QueuedRedis& ping() {
        return command<void (*)(Connection &)>(cmd::ping);
    }

    QueuedRedis& ping(const StringView &msg) {
        return command<void (*)(Connection &, const StringView &)>(cmd::ping, msg);
    }

    // We DO NOT support the QUIT command. See *Redis::quit* doc for details.
    //
    // QueuedRedis& quit();

    QueuedRedis& select(long long idx) {
        return command(cmd::select, idx);
    }

    QueuedRedis& swapdb(long long idx1, long long idx2) {
        return command(cmd::swapdb, idx1, idx2);
    }

    // SERVER commands.

    QueuedRedis& bgrewriteaof() {
        return command(cmd::bgrewriteaof);
    }

    QueuedRedis& bgsave() {
        return command(cmd::bgsave);
    }

    QueuedRedis& dbsize() {
        return command(cmd::dbsize);
    }

    QueuedRedis& flushall(bool async = false) {
        return command(cmd::flushall, async);
    }

    QueuedRedis& flushdb(bool async = false) {
        return command(cmd::flushdb, async);
    }

    QueuedRedis& info() {
        return command<void (*)(Connection &)>(cmd::info);
    }

    QueuedRedis& info(const StringView &section) {
        return command<void (*)(Connection &, const StringView &)>(cmd::info, section);
    }

    QueuedRedis& lastsave() {
        return command(cmd::lastsave);
    }

    QueuedRedis& save() {
        return command(cmd::save);
    }

    // KEY commands.

    QueuedRedis& del(const StringView &key) {
        return command(cmd::del, key);
    }

    template <typename Input>
    QueuedRedis& del(Input first, Input last) {
        return command(cmd::del_range<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& del(std::initializer_list<T> il) {
        return del(il.begin(), il.end());
    }

    QueuedRedis& dump(const StringView &key) {
        return command(cmd::dump, key);
    }

    QueuedRedis& exists(const StringView &key) {
        return command(cmd::exists, key);
    }

    template <typename Input>
    QueuedRedis& exists(Input first, Input last) {
        return command(cmd::exists_range<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& exists(std::initializer_list<T> il) {
        return exists(il.begin(), il.end());
    }

    QueuedRedis& expire(const StringView &key, long long timeout) {
        return command(cmd::expire, key, timeout);
    }

    QueuedRedis& expire(const StringView &key,
                        const std::chrono::seconds &timeout) {
        return expire(key, timeout.count());
    }

    QueuedRedis& expireat(const StringView &key, long long timestamp) {
        return command(cmd::expireat, key, timestamp);
    }

    QueuedRedis& expireat(const StringView &key,
                            const std::chrono::time_point<std::chrono::system_clock,
                                                            std::chrono::seconds> &tp) {
        return expireat(key, tp.time_since_epoch().count());
    }

    QueuedRedis& keys(const StringView &pattern) {
        return command(cmd::keys, pattern);
    }

    QueuedRedis& move(const StringView &key, long long db) {
        return command(cmd::move, key, db);
    }

    QueuedRedis& persist(const StringView &key) {
        return command(cmd::persist, key);
    }

    QueuedRedis& pexpire(const StringView &key, long long timeout) {
        return command(cmd::pexpire, key, timeout);
    }

    QueuedRedis& pexpire(const StringView &key,
                            const std::chrono::milliseconds &timeout) {
        return pexpire(key, timeout.count());
    }

    QueuedRedis& pexpireat(const StringView &key, long long timestamp) {
        return command(cmd::pexpireat, key, timestamp);
    }

    QueuedRedis& pexpireat(const StringView &key,
                            const std::chrono::time_point<std::chrono::system_clock,
                                                            std::chrono::milliseconds> &tp) {
        return pexpireat(key, tp.time_since_epoch().count());
    }

    QueuedRedis& pttl(const StringView &key) {
        return command(cmd::pttl, key);
    }

    QueuedRedis& randomkey() {
        return command(cmd::randomkey);
    }

    QueuedRedis& rename(const StringView &key, const StringView &newkey) {
        return command(cmd::rename, key, newkey);
    }

    QueuedRedis& renamenx(const StringView &key, const StringView &newkey) {
        return command(cmd::renamenx, key, newkey);
    }

    QueuedRedis& restore(const StringView &key,
                                const StringView &val,
                                long long ttl,
                                bool replace = false) {
        return command(cmd::restore, key, val, ttl, replace);
    }

    QueuedRedis& restore(const StringView &key,
                            const StringView &val,
                            const std::chrono::milliseconds &ttl = std::chrono::milliseconds{0},
                            bool replace = false) {
        return restore(key, val, ttl.count(), replace);
    }

    // TODO: sort

    QueuedRedis& scan(long long cursor,
                        const StringView &pattern,
                        long long count) {
        return command(cmd::scan, cursor, pattern, count);
    }

    QueuedRedis& scan(long long cursor) {
        return scan(cursor, "*", 10);
    }

    QueuedRedis& scan(long long cursor,
                        const StringView &pattern) {
        return scan(cursor, pattern, 10);
    }

    QueuedRedis& scan(long long cursor,
                        long long count) {
        return scan(cursor, "*", count);
    }

    QueuedRedis& touch(const StringView &key) {
        return command(cmd::touch, key);
    }

    template <typename Input>
    QueuedRedis& touch(Input first, Input last) {
        return command(cmd::touch_range<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& touch(std::initializer_list<T> il) {
        return touch(il.begin(), il.end());
    }

    QueuedRedis& ttl(const StringView &key) {
        return command(cmd::ttl, key);
    }

    QueuedRedis& type(const StringView &key) {
        return command(cmd::type, key);
    }

    QueuedRedis& unlink(const StringView &key) {
        return command(cmd::unlink, key);
    }

    template <typename Input>
    QueuedRedis& unlink(Input first, Input last) {
        return command(cmd::unlink_range<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& unlink(std::initializer_list<T> il) {
        return unlink(il.begin(), il.end());
    }

    QueuedRedis& wait(long long numslaves, long long timeout) {
        return command(cmd::wait, numslaves, timeout);
    }

    QueuedRedis& wait(long long numslaves, const std::chrono::milliseconds &timeout) {
        return wait(numslaves, timeout.count());
    }

    // STRING commands.

    QueuedRedis& append(const StringView &key, const StringView &str) {
        return command(cmd::append, key, str);
    }

    QueuedRedis& bitcount(const StringView &key,
                            long long start = 0,
                            long long end = -1) {
        return command(cmd::bitcount, key, start, end);
    }

    QueuedRedis& bitop(BitOp op,
                        const StringView &destination,
                        const StringView &key) {
        return command(cmd::bitop, op, destination, key);
    }

    template <typename Input>
    QueuedRedis& bitop(BitOp op,
                        const StringView &destination,
                        Input first,
                        Input last) {
        return command(cmd::bitop_range<Input>, op, destination, first, last);
    }

    template <typename T>
    QueuedRedis& bitop(BitOp op,
                        const StringView &destination,
                        std::initializer_list<T> il) {
        return bitop(op, destination, il.begin(), il.end());
    }

    QueuedRedis& bitpos(const StringView &key,
                        long long bit,
                        long long start = 0,
                        long long end = -1) {
        return command(cmd::bitpos, key, bit, start, end);
    }

    QueuedRedis& decr(const StringView &key) {
        return command(cmd::decr, key);
    }

    QueuedRedis& decrby(const StringView &key, long long decrement) {
        return command(cmd::decrby, key, decrement);
    }

    QueuedRedis& get(const StringView &key) {
        return command(cmd::get, key);
    }

    QueuedRedis& getbit(const StringView &key, long long offset) {
        return command(cmd::getbit, key, offset);
    }

    QueuedRedis& getrange(const StringView &key, long long start, long long end) {
        return command(cmd::getrange, key, start, end);
    }

    QueuedRedis& getset(const StringView &key, const StringView &val) {
        return command(cmd::getset, key, val);
    }

    QueuedRedis& incr(const StringView &key) {
        return command(cmd::incr, key);
    }

    QueuedRedis& incrby(const StringView &key, long long increment) {
        return command(cmd::incrby, key, increment);
    }

    QueuedRedis& incrbyfloat(const StringView &key, double increment) {
        return command(cmd::incrbyfloat, key, increment);
    }

    template <typename Input>
    QueuedRedis& mget(Input first, Input last) {
        return command(cmd::mget<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& mget(std::initializer_list<T> il) {
        return mget(il.begin(), il.end());
    }

    template <typename Input>
    QueuedRedis& mset(Input first, Input last) {
        return command(cmd::mset<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& mset(std::initializer_list<T> il) {
        return mset(il.begin(), il.end());
    }

    template <typename Input>
    QueuedRedis& msetnx(Input first, Input last) {
        return command(cmd::msetnx<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& msetnx(std::initializer_list<T> il) {
        return msetnx(il.begin(), il.end());
    }

    QueuedRedis& psetex(const StringView &key,
                        long long ttl,
                        const StringView &val) {
        return command(cmd::psetex, key, ttl, val);
    }

    QueuedRedis& psetex(const StringView &key,
                        const std::chrono::milliseconds &ttl,
                        const StringView &val) {
        return psetex(key, ttl.count(), val);
    }

    QueuedRedis& set(const StringView &key,
                        const StringView &val,
                        const std::chrono::milliseconds &ttl = std::chrono::milliseconds(0),
                        UpdateType type = UpdateType::ALWAYS) {
        _set_cmd_indexes.push_back(_cmd_num);

        return command(cmd::set, key, val, ttl.count(), type);
    }

    QueuedRedis& setex(const StringView &key,
                        long long ttl,
                        const StringView &val) {
        return command(cmd::setex, key, ttl, val);
    }

    QueuedRedis& setex(const StringView &key,
                        const std::chrono::seconds &ttl,
                        const StringView &val) {
        return setex(key, ttl.count(), val);
    }

    QueuedRedis& setnx(const StringView &key, const StringView &val) {
        return command(cmd::setnx, key, val);
    }

    QueuedRedis& setrange(const StringView &key,
                            long long offset,
                            const StringView &val) {
        return command(cmd::setrange, key, offset, val);
    }

    QueuedRedis& strlen(const StringView &key) {
        return command(cmd::strlen, key);
    }

    // LIST commands.

    QueuedRedis& blpop(const StringView &key, long long timeout) {
        return command(cmd::blpop, key, timeout);
    }

    QueuedRedis& blpop(const StringView &key,
                        const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return blpop(key, timeout.count());
    }

    template <typename Input>
    QueuedRedis& blpop(Input first, Input last, long long timeout) {
        return command(cmd::blpop_range<Input>, first, last, timeout);
    }

    template <typename T>
    QueuedRedis& blpop(std::initializer_list<T> il, long long timeout) {
        return blpop(il.begin(), il.end(), timeout);
    }

    template <typename Input>
    QueuedRedis& blpop(Input first,
                        Input last,
                        const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return blpop(first, last, timeout.count());
    }

    template <typename T>
    QueuedRedis& blpop(std::initializer_list<T> il,
                        const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return blpop(il.begin(), il.end(), timeout);
    }

    QueuedRedis& brpop(const StringView &key, long long timeout) {
        return command(cmd::brpop, key, timeout);
    }

    QueuedRedis& brpop(const StringView &key,
                        const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return brpop(key, timeout.count());
    }

    template <typename Input>
    QueuedRedis& brpop(Input first, Input last, long long timeout) {
        return command(cmd::brpop_range<Input>, first, last, timeout);
    }

    template <typename T>
    QueuedRedis& brpop(std::initializer_list<T> il, long long timeout) {
        return brpop(il.begin(), il.end(), timeout);
    }

    template <typename Input>
    QueuedRedis& brpop(Input first,
                        Input last,
                        const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return brpop(first, last, timeout.count());
    }

    template <typename T>
    QueuedRedis& brpop(std::initializer_list<T> il,
                        const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return brpop(il.begin(), il.end(), timeout);
    }

    QueuedRedis& brpoplpush(const StringView &source,
                            const StringView &destination,
                            long long timeout) {
        return command(cmd::brpoplpush, source, destination, timeout);
    }

    QueuedRedis& brpoplpush(const StringView &source,
                            const StringView &destination,
                            const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return brpoplpush(source, destination, timeout.count());
    }

    QueuedRedis& lindex(const StringView &key, long long index) {
        return command(cmd::lindex, key, index);
    }

    QueuedRedis& linsert(const StringView &key,
                            InsertPosition position,
                            const StringView &pivot,
                            const StringView &val) {
        return command(cmd::linsert, key, position, pivot, val);
    }

    QueuedRedis& llen(const StringView &key) {
        return command(cmd::llen, key);
    }

    QueuedRedis& lpop(const StringView &key) {
        return command(cmd::lpop, key);
    }

    QueuedRedis& lpush(const StringView &key, const StringView &val) {
        return command(cmd::lpush, key, val);
    }

    template <typename Input>
    QueuedRedis& lpush(const StringView &key, Input first, Input last) {
        return command(cmd::lpush_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& lpush(const StringView &key, std::initializer_list<T> il) {
        return lpush(key, il.begin(), il.end());
    }

    QueuedRedis& lpushx(const StringView &key, const StringView &val) {
        return command(cmd::lpushx, key, val);
    }

    QueuedRedis& lrange(const StringView &key,
                        long long start,
                        long long stop) {
        return command(cmd::lrange, key, start, stop);
    }

    QueuedRedis& lrem(const StringView &key, long long count, const StringView &val) {
        return command(cmd::lrem, key, count, val);
    }

    QueuedRedis& lset(const StringView &key, long long index, const StringView &val) {
        return command(cmd::lset, key, index, val);
    }

    QueuedRedis& ltrim(const StringView &key, long long start, long long stop) {
        return command(cmd::ltrim, key, start, stop);
    }

    QueuedRedis& rpop(const StringView &key) {
        return command(cmd::rpop, key);
    }

    QueuedRedis& rpoplpush(const StringView &source, const StringView &destination) {
        return command(cmd::rpoplpush, source, destination);
    }

    QueuedRedis& rpush(const StringView &key, const StringView &val) {
        return command(cmd::rpush, key, val);
    }

    template <typename Input>
    QueuedRedis& rpush(const StringView &key, Input first, Input last) {
        return command(cmd::rpush_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& rpush(const StringView &key, std::initializer_list<T> il) {
        return rpush(key, il.begin(), il.end());
    }

    QueuedRedis& rpushx(const StringView &key, const StringView &val) {
        return command(cmd::rpushx, key, val);
    }

    // HASH commands.

    QueuedRedis& hdel(const StringView &key, const StringView &field) {
        return command(cmd::hdel, key, field);
    }

    template <typename Input>
    QueuedRedis& hdel(const StringView &key, Input first, Input last) {
        return command(cmd::hdel_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& hdel(const StringView &key, std::initializer_list<T> il) {
        return hdel(key, il.begin(), il.end());
    }

    QueuedRedis& hexists(const StringView &key, const StringView &field) {
        return command(cmd::hexists, key, field);
    }

    QueuedRedis& hget(const StringView &key, const StringView &field) {
        return command(cmd::hget, key, field);
    }

    QueuedRedis& hgetall(const StringView &key) {
        return command(cmd::hgetall, key);
    }

    QueuedRedis& hincrby(const StringView &key,
                            const StringView &field,
                            long long increment) {
        return command(cmd::hincrby, key, field, increment);
    }

    QueuedRedis& hincrbyfloat(const StringView &key,
                                const StringView &field,
                                double increment) {
        return command(cmd::hincrbyfloat, key, field, increment);
    }

    QueuedRedis& hkeys(const StringView &key) {
        return command(cmd::hkeys, key);
    }

    QueuedRedis& hlen(const StringView &key) {
        return command(cmd::hlen, key);
    }

    template <typename Input>
    QueuedRedis& hmget(const StringView &key, Input first, Input last) {
        return command(cmd::hmget<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& hmget(const StringView &key, std::initializer_list<T> il) {
        return hmget(key, il.begin(), il.end());
    }

    template <typename Input>
    QueuedRedis& hmset(const StringView &key, Input first, Input last) {
        return command(cmd::hmset<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& hmset(const StringView &key, std::initializer_list<T> il) {
        return hmset(key, il.begin(), il.end());
    }

    QueuedRedis& hscan(const StringView &key,
                        long long cursor,
                        const StringView &pattern,
                        long long count) {
        return command(cmd::hscan, key, cursor, pattern, count);
    }

    QueuedRedis& hscan(const StringView &key,
                        long long cursor,
                        const StringView &pattern) {
        return hscan(key, cursor, pattern, 10);
    }

    QueuedRedis& hscan(const StringView &key,
                        long long cursor,
                        long long count) {
        return hscan(key, cursor, "*", count);
    }

    QueuedRedis& hscan(const StringView &key,
                        long long cursor) {
        return hscan(key, cursor, "*", 10);
    }

    QueuedRedis& hset(const StringView &key, const StringView &field, const StringView &val) {
        return command(cmd::hset, key, field, val);
    }

    QueuedRedis& hset(const StringView &key, const std::pair<StringView, StringView> &item) {
        return hset(key, item.first, item.second);
    }

    QueuedRedis& hsetnx(const StringView &key, const StringView &field, const StringView &val) {
        return command(cmd::hsetnx, key, field, val);
    }

    QueuedRedis& hsetnx(const StringView &key, const std::pair<StringView, StringView> &item) {
        return hsetnx(key, item.first, item.second);
    }

    QueuedRedis& hstrlen(const StringView &key, const StringView &field) {
        return command(cmd::hstrlen, key, field);
    }

    QueuedRedis& hvals(const StringView &key) {
        return command(cmd::hvals, key);
    }

    // SET commands.

    QueuedRedis& sadd(const StringView &key, const StringView &member) {
        return command(cmd::sadd, key, member);
    }

    template <typename Input>
    QueuedRedis& sadd(const StringView &key, Input first, Input last) {
        return command(cmd::sadd_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& sadd(const StringView &key, std::initializer_list<T> il) {
        return sadd(key, il.begin(), il.end());
    }

    QueuedRedis& scard(const StringView &key) {
        return command(cmd::scard, key);
    }

    template <typename Input>
    QueuedRedis& sdiff(Input first, Input last) {
        return command(cmd::sdiff<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& sdiff(std::initializer_list<T> il) {
        return sdiff(il.begin(), il.end());
    }

    QueuedRedis& sdiffstore(const StringView &destination, const StringView &key) {
        return command(cmd::sdiffstore, destination, key);
    }

    template <typename Input>
    QueuedRedis& sdiffstore(const StringView &destination,
                            Input first,
                            Input last) {
        return command(cmd::sdiffstore_range<Input>, destination, first, last);
    }

    template <typename T>
    QueuedRedis& sdiffstore(const StringView &destination, std::initializer_list<T> il) {
        return sdiffstore(destination, il.begin(), il.end());
    }

    template <typename Input>
    QueuedRedis& sinter(Input first, Input last) {
        return command(cmd::sinter<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& sinter(std::initializer_list<T> il) {
        return sinter(il.begin(), il.end());
    }

    QueuedRedis& sinterstore(const StringView &destination, const StringView &key) {
        return command(cmd::sinterstore, destination, key);
    }

    template <typename Input>
    QueuedRedis& sinterstore(const StringView &destination,
                                Input first,
                                Input last) {
        return command(cmd::sinterstore_range<Input>, destination, first, last);
    }

    template <typename T>
    QueuedRedis& sinterstore(const StringView &destination, std::initializer_list<T> il) {
        return sinterstore(destination, il.begin(), il.end());
    }

    QueuedRedis& sismember(const StringView &key, const StringView &member) {
        return command(cmd::sismember, key, member);
    }

    QueuedRedis& smembers(const StringView &key) {
        return command(cmd::smembers, key);
    }

    QueuedRedis& smove(const StringView &source,
                        const StringView &destination,
                        const StringView &member) {
        return command(cmd::smove, source, destination, member);
    }

    QueuedRedis& spop(const StringView &key) {
        return command(cmd::spop, key);
    }

    QueuedRedis& spop(const StringView &key, long long count) {
        return command(cmd::spop_range, key, count);
    }

    QueuedRedis& srandmember(const StringView &key) {
        return command(cmd::srandmember, key);
    }

    QueuedRedis& srandmember(const StringView &key, long long count) {
        return command(cmd::srandmember_range, key, count);
    }

    QueuedRedis& srem(const StringView &key, const StringView &member) {
        return command(cmd::srem, key, member);
    }

    template <typename Input>
    QueuedRedis& srem(const StringView &key, Input first, Input last) {
        return command(cmd::srem_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& srem(const StringView &key, std::initializer_list<T> il) {
        return srem(key, il.begin(), il.end());
    }

    QueuedRedis& sscan(const StringView &key,
                        long long cursor,
                        const StringView &pattern,
                        long long count) {
        return command(cmd::sscan, key, cursor, pattern, count);
    }

    QueuedRedis& sscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern) {
        return sscan(key, cursor, pattern, 10);
    }

    QueuedRedis& sscan(const StringView &key,
                        long long cursor,
                        long long count) {
        return sscan(key, cursor, "*", count);
    }

    QueuedRedis& sscan(const StringView &key,
                        long long cursor) {
        return sscan(key, cursor, "*", 10);
    }

    template <typename Input>
    QueuedRedis& sunion(Input first, Input last) {
        return command(cmd::sunion<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& sunion(std::initializer_list<T> il) {
        return sunion(il.begin(), il.end());
    }

    QueuedRedis& sunionstore(const StringView &destination, const StringView &key) {
        return command(cmd::sunionstore, destination, key);
    }

    template <typename Input>
    QueuedRedis& sunionstore(const StringView &destination, Input first, Input last) {
        return command(cmd::sunionstore_range<Input>, destination, first, last);
    }

    template <typename T>
    QueuedRedis& sunionstore(const StringView &destination, std::initializer_list<T> il) {
        return sunionstore(destination, il.begin(), il.end());
    }

    // SORTED SET commands.

    QueuedRedis& bzpopmax(const StringView &key, long long timeout) {
        return command(cmd::bzpopmax, key, timeout);
    }

    QueuedRedis& bzpopmax(const StringView &key,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return bzpopmax(key, timeout.count());
    }

    template <typename Input>
    QueuedRedis& bzpopmax(Input first, Input last, long long timeout) {
        return command(cmd::bzpopmax_range<Input>, first, last, timeout);
    }

    template <typename Input>
    QueuedRedis& bzpopmax(Input first,
                            Input last,
                            const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return bzpopmax(first, last, timeout.count());
    }

    template <typename T>
    QueuedRedis& bzpopmax(std::initializer_list<T> il, long long timeout) {
        return bzpopmax(il.begin(), il.end(), timeout);
    }

    template <typename T>
    QueuedRedis& bzpopmax(std::initializer_list<T> il,
                            const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return bzpopmax(il.begin(), il.end(), timeout);
    }

    QueuedRedis& bzpopmin(const StringView &key, long long timeout) {
        return command(cmd::bzpopmin, key, timeout);
    }

    QueuedRedis& bzpopmin(const StringView &key,
                            const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return bzpopmin(key, timeout.count());
    }

    template <typename Input>
    QueuedRedis& bzpopmin(Input first, Input last, long long timeout) {
        return command(cmd::bzpopmin_range<Input>, first, last, timeout);
    }

    template <typename Input>
    QueuedRedis& bzpopmin(Input first,
                            Input last,
                            const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return bzpopmin(first, last, timeout.count());
    }

    template <typename T>
    QueuedRedis& bzpopmin(std::initializer_list<T> il, long long timeout) {
        return bzpopmin(il.begin(), il.end(), timeout);
    }

    template <typename T>
    QueuedRedis& bzpopmin(std::initializer_list<T> il,
                            const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return bzpopmin(il.begin(), il.end(), timeout);
    }

    // We don't support the INCR option, since you can always use ZINCRBY instead.
    QueuedRedis& zadd(const StringView &key,
                        const StringView &member,
                        double score,
                        UpdateType type = UpdateType::ALWAYS,
                        bool changed = false) {
        return command(cmd::zadd, key, member, score, type, changed);
    }

    template <typename Input>
    QueuedRedis& zadd(const StringView &key,
                        Input first,
                        Input last,
                        UpdateType type = UpdateType::ALWAYS,
                        bool changed = false) {
        return command(cmd::zadd_range<Input>, key, first, last, type, changed);
    }

    QueuedRedis& zcard(const StringView &key) {
        return command(cmd::zcard, key);
    }

    template <typename Interval>
    QueuedRedis& zcount(const StringView &key, const Interval &interval) {
        return command(cmd::zcount<Interval>, key, interval);
    }

    QueuedRedis& zincrby(const StringView &key, double increment, const StringView &member) {
        return command(cmd::zincrby, key, increment, member);
    }

    QueuedRedis& zinterstore(const StringView &destination,
                                const StringView &key,
                                double weight) {
        return command(cmd::zinterstore, destination, key, weight);
    }

    template <typename Input>
    QueuedRedis& zinterstore(const StringView &destination,
                                Input first,
                                Input last,
                                Aggregation type = Aggregation::SUM) {
        return command(cmd::zinterstore_range<Input>, destination, first, last, type);
    }

    template <typename T>
    QueuedRedis& zinterstore(const StringView &destination,
                                std::initializer_list<T> il,
                                Aggregation type = Aggregation::SUM) {
        return zinterstore(destination, il.begin(), il.end(), type);
    }

    template <typename Interval>
    QueuedRedis& zlexcount(const StringView &key, const Interval &interval) {
        return command(cmd::zlexcount<Interval>, key, interval);
    }

    QueuedRedis& zpopmax(const StringView &key) {
        return command(cmd::zpopmax, key, 1);
    }

    QueuedRedis& zpopmax(const StringView &key, long long count) {
        return command(cmd::zpopmax, key, count);
    }

    QueuedRedis& zpopmin(const StringView &key) {
        return command(cmd::zpopmin, key, 1);
    }

    QueuedRedis& zpopmin(const StringView &key, long long count) {
        return command(cmd::zpopmin, key, count);
    }

    // NOTE: *QueuedRedis::zrange*'s parameters are different from *Redis::zrange*.
    // *Redis::zrange* is overloaded by the output iterator, however, there's no such
    // iterator in *QueuedRedis::zrange*. So we have to use an extra parameter: *with_scores*,
    // to decide whether we should send *WITHSCORES* option to Redis. This also applies to
    // other commands with the *WITHSCORES* option, e.g. *ZRANGEBYSCORE*, *ZREVRANGE*,
    // *ZREVRANGEBYSCORE*.
    QueuedRedis& zrange(const StringView &key,
                        long long start,
                        long long stop,
                        bool with_scores = false) {
        return command(cmd::zrange, key, start, stop, with_scores);
    }

    template <typename Interval>
    QueuedRedis& zrangebylex(const StringView &key,
                                const Interval &interval,
                                const LimitOptions &opts) {
        return command(cmd::zrangebylex<Interval>, key, interval, opts);
    }

    template <typename Interval>
    QueuedRedis& zrangebylex(const StringView &key, const Interval &interval) {
        return zrangebylex(key, interval, {});
    }

    // See comments on *ZRANGE*.
    template <typename Interval>
    QueuedRedis& zrangebyscore(const StringView &key,
                                const Interval &interval,
                                const LimitOptions &opts,
                                bool with_scores = false) {
        return command(cmd::zrangebyscore<Interval>, key, interval, opts, with_scores);
    }

    // See comments on *ZRANGE*.
    template <typename Interval>
    QueuedRedis& zrangebyscore(const StringView &key,
                                const Interval &interval,
                                bool with_scores = false) {
        return zrangebyscore(key, interval, {}, with_scores);
    }

    QueuedRedis& zrank(const StringView &key, const StringView &member) {
        return command(cmd::zrank, key, member);
    }

    QueuedRedis& zrem(const StringView &key, const StringView &member) {
        return command(cmd::zrem, key, member);
    }

    template <typename Input>
    QueuedRedis& zrem(const StringView &key, Input first, Input last) {
        return command(cmd::zrem_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& zrem(const StringView &key, std::initializer_list<T> il) {
        return zrem(key, il.begin(), il.end());
    }

    template <typename Interval>
    QueuedRedis& zremrangebylex(const StringView &key, const Interval &interval) {
        return command(cmd::zremrangebylex<Interval>, key, interval);
    }

    QueuedRedis& zremrangebyrank(const StringView &key, long long start, long long stop) {
        return command(cmd::zremrangebyrank, key, start, stop);
    }

    template <typename Interval>
    QueuedRedis& zremrangebyscore(const StringView &key, const Interval &interval) {
        return command(cmd::zremrangebyscore<Interval>, key, interval);
    }

    // See comments on *ZRANGE*.
    QueuedRedis& zrevrange(const StringView &key,
                            long long start,
                            long long stop,
                            bool with_scores = false) {
        return command(cmd::zrevrange, key, start, stop, with_scores);
    }

    template <typename Interval>
    QueuedRedis& zrevrangebylex(const StringView &key,
                                const Interval &interval,
                                const LimitOptions &opts) {
        return command(cmd::zrevrangebylex<Interval>, key, interval, opts);
    }

    template <typename Interval>
    QueuedRedis& zrevrangebylex(const StringView &key, const Interval &interval) {
        return zrevrangebylex(key, interval, {});
    }

    // See comments on *ZRANGE*.
    template <typename Interval>
    QueuedRedis& zrevrangebyscore(const StringView &key,
                                    const Interval &interval,
                                    const LimitOptions &opts,
                                    bool with_scores = false) {
        return command(cmd::zrevrangebyscore<Interval>, key, interval, opts, with_scores);
    }

    // See comments on *ZRANGE*.
    template <typename Interval>
    QueuedRedis& zrevrangebyscore(const StringView &key,
                                    const Interval &interval,
                                    bool with_scores = false) {
        return zrevrangebyscore(key, interval, {}, with_scores);
    }

    QueuedRedis& zrevrank(const StringView &key, const StringView &member) {
        return command(cmd::zrevrank, key, member);
    }

    QueuedRedis& zscan(const StringView &key,
                        long long cursor,
                        const StringView &pattern,
                        long long count) {
        return command(cmd::zscan, key, cursor, pattern, count);
    }

    QueuedRedis& zscan(const StringView &key,
                        long long cursor,
                        const StringView &pattern) {
        return zscan(key, cursor, pattern, 10);
    }

    QueuedRedis& zscan(const StringView &key,
                        long long cursor,
                        long long count) {
        return zscan(key, cursor, "*", count);
    }

    QueuedRedis& zscan(const StringView &key,
                        long long cursor) {
        return zscan(key, cursor, "*", 10);
    }

    QueuedRedis& zscore(const StringView &key, const StringView &member) {
        return command(cmd::zscore, key, member);
    }

    QueuedRedis& zunionstore(const StringView &destination,
                                const StringView &key,
                                double weight) {
        return command(cmd::zunionstore, destination, key, weight);
    }

    template <typename Input>
    QueuedRedis& zunionstore(const StringView &destination,
                                Input first,
                                Input last,
                                Aggregation type = Aggregation::SUM) {
        return command(cmd::zunionstore_range<Input>, destination, first, last, type);
    }

    template <typename T>
    QueuedRedis& zunionstore(const StringView &destination,
                                std::initializer_list<T> il,
                                Aggregation type = Aggregation::SUM) {
        return zunionstore(destination, il.begin(), il.end(), type);
    }

    // HYPERLOGLOG commands.

    QueuedRedis& pfadd(const StringView &key, const StringView &element) {
        return command(cmd::pfadd, key, element);
    }

    template <typename Input>
    QueuedRedis& pfadd(const StringView &key, Input first, Input last) {
        return command(cmd::pfadd_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& pfadd(const StringView &key, std::initializer_list<T> il) {
        return pfadd(key, il.begin(), il.end());
    }

    QueuedRedis& pfcount(const StringView &key) {
        return command(cmd::pfcount, key);
    }

    template <typename Input>
    QueuedRedis& pfcount(Input first, Input last) {
        return command(cmd::pfcount_range<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& pfcount(std::initializer_list<T> il) {
        return pfcount(il.begin(), il.end());
    }

    QueuedRedis& pfmerge(const StringView &destination, const StringView &key) {
        return command(cmd::pfmerge, destination, key);
    }

    template <typename Input>
    QueuedRedis& pfmerge(const StringView &destination, Input first, Input last) {
        return command(cmd::pfmerge_range<Input>, destination, first, last);
    }

    template <typename T>
    QueuedRedis& pfmerge(const StringView &destination, std::initializer_list<T> il) {
        return pfmerge(destination, il.begin(), il.end());
    }

    // GEO commands.

    QueuedRedis& geoadd(const StringView &key,
                        const std::tuple<StringView, double, double> &member) {
        return command(cmd::geoadd, key, member);
    }

    template <typename Input>
    QueuedRedis& geoadd(const StringView &key,
                        Input first,
                        Input last) {
        return command(cmd::geoadd_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& geoadd(const StringView &key, std::initializer_list<T> il) {
        return geoadd(key, il.begin(), il.end());
    }

    QueuedRedis& geodist(const StringView &key,
                            const StringView &member1,
                            const StringView &member2,
                            GeoUnit unit = GeoUnit::M) {
        return command(cmd::geodist, key, member1, member2, unit);
    }

    template <typename Input>
    QueuedRedis& geohash(const StringView &key, Input first, Input last) {
        return command(cmd::geohash_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& geohash(const StringView &key, std::initializer_list<T> il) {
        return geohash(key, il.begin(), il.end());
    }

    template <typename Input>
    QueuedRedis& geopos(const StringView &key, Input first, Input last) {
        return command(cmd::geopos_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& geopos(const StringView &key, std::initializer_list<T> il) {
        return geopos(key, il.begin(), il.end());
    }

    // TODO:
    // 1. since we have different overloads for georadius and georadius-store,
    //    we might use the GEORADIUS_RO command in the future.
    // 2. there're too many parameters for this method, we might refactor it.
    QueuedRedis& georadius(const StringView &key,
                            const std::pair<double, double> &loc,
                            double radius,
                            GeoUnit unit,
                            const StringView &destination,
                            bool store_dist,
                            long long count) {
        _georadius_cmd_indexes.push_back(_cmd_num);

        return command(cmd::georadius_store,
                        key,
                        loc,
                        radius,
                        unit,
                        destination,
                        store_dist,
                        count);
    }

    // NOTE: *QueuedRedis::georadius*'s parameters are different from *Redis::georadius*.
    // *Redis::georadius* is overloaded by the output iterator, however, there's no such
    // iterator in *QueuedRedis::georadius*. So we have to use extra parameters to decide
    // whether we should send options to Redis. This also applies to *GEORADIUSBYMEMBER*.
    QueuedRedis& georadius(const StringView &key,
                            const std::pair<double, double> &loc,
                            double radius,
                            GeoUnit unit,
                            long long count,
                            bool asc,
                            bool with_coord,
                            bool with_dist,
                            bool with_hash) {
        return command(cmd::georadius,
                        key,
                        loc,
                        radius,
                        unit,
                        count,
                        asc,
                        with_coord,
                        with_dist,
                        with_hash);
    }

    QueuedRedis& georadiusbymember(const StringView &key,
                                    const StringView &member,
                                    double radius,
                                    GeoUnit unit,
                                    const StringView &destination,
                                    bool store_dist,
                                    long long count) {
        _georadius_cmd_indexes.push_back(_cmd_num);

        return command(cmd::georadiusbymember,
                        key,
                        member,
                        radius,
                        unit,
                        destination,
                        store_dist,
                        count);
    }

    // See the comments on *GEORADIUS*.
    QueuedRedis& georadiusbymember(const StringView &key,
                                    const StringView &member,
                                    double radius,
                                    GeoUnit unit,
                                    long long count,
                                    bool asc,
                                    bool with_coord,
                                    bool with_dist,
                                    bool with_hash) {
        return command(cmd::georadiusbymember,
                        key,
                        member,
                        radius,
                        unit,
                        count,
                        asc,
                        with_coord,
                        with_dist,
                        with_hash);
    }

    // SCRIPTING commands.

    QueuedRedis& eval(const StringView &script,
                        std::initializer_list<StringView> keys,
                        std::initializer_list<StringView> args) {
        return command(cmd::eval, script, keys, args);
    }

    QueuedRedis& evalsha(const StringView &script,
                            std::initializer_list<StringView> keys,
                            std::initializer_list<StringView> args) {
        return command(cmd::evalsha, script, keys, args);
    }

    template <typename Input>
    QueuedRedis& script_exists(Input first, Input last) {
        return command(cmd::script_exists_range<Input>, first, last);
    }

    template <typename T>
    QueuedRedis& script_exists(std::initializer_list<T> il) {
        return script_exists(il.begin(), il.end());
    }

    QueuedRedis& script_flush() {
        return command(cmd::script_flush);
    }

    QueuedRedis& script_kill() {
        return command(cmd::script_kill);
    }

    QueuedRedis& script_load(const StringView &script) {
        return command(cmd::script_load, script);
    }

    // PUBSUB commands.

    QueuedRedis& publish(const StringView &channel, const StringView &message) {
        return command(cmd::publish, channel, message);
    }

    // Stream commands.

    QueuedRedis& xack(const StringView &key, const StringView &group, const StringView &id) {
        return command(cmd::xack, key, group, id);
    }

    template <typename Input>
    QueuedRedis& xack(const StringView &key, const StringView &group, Input first, Input last) {
        return command(cmd::xack_range<Input>, key, group, first, last);
    }

    template <typename T>
    QueuedRedis& xack(const StringView &key, const StringView &group, std::initializer_list<T> il) {
        return xack(key, group, il.begin(), il.end());
    }

    template <typename Input>
    QueuedRedis& xadd(const StringView &key, const StringView &id, Input first, Input last) {
        return command(cmd::xadd_range<Input>, key, id, first, last);
    }

    template <typename T>
    QueuedRedis& xadd(const StringView &key, const StringView &id, std::initializer_list<T> il) {
        return xadd(key, id, il.begin(), il.end());
    }

    template <typename Input>
    QueuedRedis& xadd(const StringView &key,
                        const StringView &id,
                        Input first,
                        Input last,
                        long long count,
                        bool approx = true) {
        return command(cmd::xadd_maxlen_range<Input>, key, id, first, last, count, approx);
    }

    template <typename T>
    QueuedRedis& xadd(const StringView &key,
                        const StringView &id,
                        std::initializer_list<T> il,
                        long long count,
                        bool approx = true) {
        return xadd(key, id, il.begin(), il.end(), count, approx);
    }

    QueuedRedis& xclaim(const StringView &key,
                        const StringView &group,
                        const StringView &consumer,
                        const std::chrono::milliseconds &min_idle_time,
                        const StringView &id) {
        return command(cmd::xclaim, key, group, consumer, min_idle_time.count(), id);
    }

    template <typename Input>
    QueuedRedis& xclaim(const StringView &key,
                const StringView &group,
                const StringView &consumer,
                const std::chrono::milliseconds &min_idle_time,
                Input first,
                Input last) {
        return command(cmd::xclaim_range<Input>,
                        key,
                        group,
                        consumer,
                        min_idle_time.count(),
                        first,
                        last);
    }

    template <typename T>
    QueuedRedis& xclaim(const StringView &key,
                const StringView &group,
                const StringView &consumer,
                const std::chrono::milliseconds &min_idle_time,
                std::initializer_list<T> il) {
        return xclaim(key, group, consumer, min_idle_time, il.begin(), il.end());
    }

    QueuedRedis& xdel(const StringView &key, const StringView &id) {
        return command(cmd::xdel, key, id);
    }

    template <typename Input>
    QueuedRedis& xdel(const StringView &key, Input first, Input last) {
        return command(cmd::xdel_range<Input>, key, first, last);
    }

    template <typename T>
    QueuedRedis& xdel(const StringView &key, std::initializer_list<T> il) {
        return xdel(key, il.begin(), il.end());
    }

    QueuedRedis& xgroup_create(const StringView &key,
                                const StringView &group,
                                const StringView &id,
                                bool mkstream = false) {
        return command(cmd::xgroup_create, key, group, id, mkstream);
    }

    QueuedRedis& xgroup_setid(const StringView &key,
                                const StringView &group,
                                const StringView &id) {
        return command(cmd::xgroup_setid, key, group, id);
    }

    QueuedRedis& xgroup_destroy(const StringView &key, const StringView &group) {
        return command(cmd::xgroup_destroy, key, group);
    }

    QueuedRedis& xgroup_delconsumer(const StringView &key,
                                    const StringView &group,
                                    const StringView &consumer) {
        return command(cmd::xgroup_delconsumer, key, group, consumer);
    }

    QueuedRedis& xlen(const StringView &key) {
        return command(cmd::xlen, key);
    }

    QueuedRedis& xpending(const StringView &key, const StringView &group) {
        return command(cmd::xpending, key, group);
    }

    QueuedRedis& xpending(const StringView &key,
                            const StringView &group,
                            const StringView &start,
                            const StringView &end,
                            long long count) {
        return command(cmd::xpending_detail, key, group, start, end, count);
    }

    QueuedRedis& xpending(const StringView &key,
                            const StringView &group,
                            const StringView &start,
                            const StringView &end,
                            long long count,
                            const StringView &consumer) {
        return command(cmd::xpending_per_consumer, key, group, start, end, count, consumer);
    }

    QueuedRedis& xrange(const StringView &key,
                        const StringView &start,
                        const StringView &end) {
        return command(cmd::xrange, key, start, end);
    }

    QueuedRedis& xrange(const StringView &key,
                        const StringView &start,
                        const StringView &end,
                        long long count) {
        return command(cmd::xrange, key, start, end, count);
    }

    QueuedRedis& xread(const StringView &key, const StringView &id, long long count) {
        return command(cmd::xread, key, id, count);
    }

    QueuedRedis& xread(const StringView &key, const StringView &id) {
        return xread(key, id, 0);
    }

    template <typename Input>
    auto xread(Input first, Input last, long long count)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return command(cmd::xread_range<Input>, first, last, count);
    }

    template <typename Input>
    auto xread(Input first, Input last)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return xread(first, last, 0);
    }

    QueuedRedis& xread(const StringView &key,
                        const StringView &id,
                        const std::chrono::milliseconds &timeout,
                        long long count) {
        return command(cmd::xread_block, key, id, timeout.count(), count);
    }

    QueuedRedis& xread(const StringView &key,
                        const StringView &id,
                        const std::chrono::milliseconds &timeout) {
        return xread(key, id, timeout, 0);
    }

    template <typename Input>
    auto xread(Input first,
                Input last,
                const std::chrono::milliseconds &timeout,
                long long count)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return command(cmd::xread_block_range<Input>, first, last, timeout.count(), count);
    }

    template <typename Input>
    auto xread(Input first,
                Input last,
                const std::chrono::milliseconds &timeout)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return xread(first, last, timeout, 0);
    }

    QueuedRedis& xreadgroup(const StringView &group,
                            const StringView &consumer,
                            const StringView &key,
                            const StringView &id,
                            long long count,
                            bool noack) {
        return command(cmd::xreadgroup, group, consumer, key, id, count, noack);
    }

    QueuedRedis& xreadgroup(const StringView &group,
                            const StringView &consumer,
                            const StringView &key,
                            const StringView &id,
                            long long count) {
        return xreadgroup(group, consumer, key, id, count, false);
    }

    template <typename Input>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    long long count,
                    bool noack)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return command(cmd::xreadgroup_range<Input>, group, consumer, first, last, count, noack);
    }

    template <typename Input>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    long long count)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return xreadgroup(group, consumer, first ,last, count, false);
    }

    template <typename Input>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return xreadgroup(group, consumer, first ,last, 0, false);
    }

    QueuedRedis& xreadgroup(const StringView &group,
                            const StringView &consumer,
                            const StringView &key,
                            const StringView &id,
                            const std::chrono::milliseconds &timeout,
                            long long count,
                            bool noack) {
        return command(cmd::xreadgroup_block,
                        group,
                        consumer,
                        key,
                        id,
                        timeout.count(),
                        count,
                        noack);
    }

    QueuedRedis& xreadgroup(const StringView &group,
                            const StringView &consumer,
                            const StringView &key,
                            const StringView &id,
                            const std::chrono::milliseconds &timeout,
                            long long count) {
        return xreadgroup(group, consumer, key, id, timeout, count, false);
    }

    QueuedRedis& xreadgroup(const StringView &group,
                            const StringView &consumer,
                            const StringView &key,
                            const StringView &id,
                            const std::chrono::milliseconds &timeout) {
        return xreadgroup(group, consumer, key, id, timeout, 0, false);
    }

    template <typename Input>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    bool noack)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return command(cmd::xreadgroup_block_range<Input>,
                        group,
                        consumer,
                        first,
                        last,
                        timeout.count(),
                        count,
                        noack);
    }

    template <typename Input>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout,
                    long long count)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return xreadgroup(group, consumer, first, last, timeout, count, false);
    }

    template <typename Input>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    QueuedRedis&>::type {
        return xreadgroup(group, consumer, first, last, timeout, 0, false);
    }

    QueuedRedis& xrevrange(const StringView &key,
                            const StringView &end,
                            const StringView &start) {
        return command(cmd::xrevrange, key, end, start);
    }

    QueuedRedis& xrevrange(const StringView &key,
                            const StringView &end,
                            const StringView &start,
                            long long count) {
        return command(cmd::xrevrange, key, end, start, count);
    }

    QueuedRedis& xtrim(const StringView &key, long long count, bool approx = true) {
        return command(cmd::xtrim, key, count, approx);
    }

private:
    friend class Redis;

    friend class RedisCluster;

    template <typename ...Args>
    QueuedRedis(const ConnectionSPtr &connection, Args &&...args);

    void _sanity_check() const;

    void _reset();

    void _invalidate();

    void _rewrite_replies(std::vector<ReplyUPtr> &replies) const;

    template <typename Func>
    void _rewrite_replies(const std::vector<std::size_t> &indexes,
                            Func rewriter,
                            std::vector<ReplyUPtr> &replies) const;

    ConnectionSPtr _connection;

    Impl _impl;

    std::size_t _cmd_num = 0;

    std::vector<std::size_t> _set_cmd_indexes;

    std::vector<std::size_t> _georadius_cmd_indexes;

    bool _valid = true;
};

class QueuedReplies {
public:
    std::size_t size() const;

    redisReply& get(std::size_t idx);

    template <typename Result>
    Result get(std::size_t idx);

    template <typename Output>
    void get(std::size_t idx, Output output);

private:
    template <typename Impl>
    friend class QueuedRedis;

    explicit QueuedReplies(std::vector<ReplyUPtr> replies) : _replies(std::move(replies)) {}

    void _index_check(std::size_t idx) const;

    std::vector<ReplyUPtr> _replies;
};

}

}

#include "queued_redis.hpp"

#endif // end SEWENEW_REDISPLUSPLUS_QUEUED_REDIS_H
