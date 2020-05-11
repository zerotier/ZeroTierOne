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

#ifndef SEWENEW_REDISPLUSPLUS_REDIS_CLUSTER_HPP
#define SEWENEW_REDISPLUSPLUS_REDIS_CLUSTER_HPP

#include <utility>
#include "command.h"
#include "reply.h"
#include "utils.h"
#include "errors.h"
#include "shards_pool.h"

namespace sw {

namespace redis {

template <typename Cmd, typename Key, typename ...Args>
auto RedisCluster::command(Cmd cmd, Key &&key, Args &&...args)
    -> typename std::enable_if<!std::is_convertible<Cmd, StringView>::value, ReplyUPtr>::type {
    return _command(cmd,
                    std::is_convertible<typename std::decay<Key>::type, StringView>(),
                    std::forward<Key>(key),
                    std::forward<Args>(args)...);
}

template <typename Key, typename ...Args>
auto RedisCluster::command(const StringView &cmd_name, Key &&key, Args &&...args)
    -> typename std::enable_if<(std::is_convertible<Key, StringView>::value
        || std::is_arithmetic<typename std::decay<Key>::type>::value)
        && !IsIter<typename LastType<Key, Args...>::type>::value, ReplyUPtr>::type {
    auto cmd = Command(cmd_name);

    return _generic_command(cmd, std::forward<Key>(key), std::forward<Args>(args)...);
}

template <typename Result, typename Key, typename ...Args>
auto RedisCluster::command(const StringView &cmd_name, Key &&key, Args &&...args)
    -> typename std::enable_if<std::is_convertible<Key, StringView>::value
            || std::is_arithmetic<typename std::decay<Key>::type>::value, Result>::type {
    auto r = command(cmd_name, std::forward<Key>(key), std::forward<Args>(args)...);

    assert(r);

    return reply::parse<Result>(*r);
}

template <typename Key, typename ...Args>
auto RedisCluster::command(const StringView &cmd_name, Key &&key, Args &&...args)
    -> typename std::enable_if<(std::is_convertible<Key, StringView>::value
            || std::is_arithmetic<typename std::decay<Key>::type>::value)
            && IsIter<typename LastType<Key, Args...>::type>::value, void>::type {
    auto r = _command(cmd_name,
                        MakeIndexSequence<sizeof...(Args)>(),
                        std::forward<Key>(key),
                        std::forward<Args>(args)...);

    assert(r);

    reply::to_array(*r, LastValue(std::forward<Args>(args)...));
}

template <typename Input>
auto RedisCluster::command(Input first, Input last)
    -> typename std::enable_if<IsIter<Input>::value, ReplyUPtr>::type {
    if (first == last || std::next(first) == last) {
        throw Error("command: invalid range");
    }

    const auto &key = *first;
    ++first;

    auto cmd = [&key](Connection &connection, Input first, Input last) {
                        CmdArgs cmd_args;
                        cmd_args.append(key);
                        while (first != last) {
                            cmd_args.append(*first);
                            ++first;
                        }
                        connection.send(cmd_args);
    };

    return command(cmd, first, last);
}

template <typename Result, typename Input>
auto RedisCluster::command(Input first, Input last)
    -> typename std::enable_if<IsIter<Input>::value, Result>::type {
    auto r = command(first, last);

    assert(r);

    return reply::parse<Result>(*r);
}

template <typename Input, typename Output>
auto RedisCluster::command(Input first, Input last, Output output)
    -> typename std::enable_if<IsIter<Input>::value, void>::type {
    auto r = command(first, last);

    assert(r);

    reply::to_array(*r, output);
}

// KEY commands.

template <typename Input>
long long RedisCluster::del(Input first, Input last) {
    if (first == last) {
        throw Error("DEL: no key specified");
    }

    auto reply = command(cmd::del_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
long long RedisCluster::exists(Input first, Input last) {
    if (first == last) {
        throw Error("EXISTS: no key specified");
    }

    auto reply = command(cmd::exists_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

inline bool RedisCluster::expire(const StringView &key, const std::chrono::seconds &timeout) {
    return expire(key, timeout.count());
}

inline bool RedisCluster::expireat(const StringView &key,
                                    const std::chrono::time_point<std::chrono::system_clock,
                                                                    std::chrono::seconds> &tp) {
    return expireat(key, tp.time_since_epoch().count());
}

inline bool RedisCluster::pexpire(const StringView &key, const std::chrono::milliseconds &timeout) {
    return pexpire(key, timeout.count());
}

inline bool RedisCluster::pexpireat(const StringView &key,
                                const std::chrono::time_point<std::chrono::system_clock,
                                                                std::chrono::milliseconds> &tp) {
    return pexpireat(key, tp.time_since_epoch().count());
}

inline void RedisCluster::restore(const StringView &key,
                            const StringView &val,
                            const std::chrono::milliseconds &ttl,
                            bool replace) {
    return restore(key, val, ttl.count(), replace);
}

template <typename Input>
long long RedisCluster::touch(Input first, Input last) {
    if (first == last) {
        throw Error("TOUCH: no key specified");
    }

    auto reply = command(cmd::touch_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
long long RedisCluster::unlink(Input first, Input last) {
    if (first == last) {
        throw Error("UNLINK: no key specified");
    }

    auto reply = command(cmd::unlink_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

// STRING commands.

template <typename Input>
long long RedisCluster::bitop(BitOp op, const StringView &destination, Input first, Input last) {
    if (first == last) {
        throw Error("BITOP: no key specified");
    }

    auto reply = _command(cmd::bitop_range<Input>, destination, op, destination, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::mget(Input first, Input last, Output output) {
    if (first == last) {
        throw Error("MGET: no key specified");
    }

    auto reply = command(cmd::mget<Input>, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
void RedisCluster::mset(Input first, Input last) {
    if (first == last) {
        throw Error("MSET: no key specified");
    }

    auto reply = command(cmd::mset<Input>, first, last);

    reply::parse<void>(*reply);
}

template <typename Input>
bool RedisCluster::msetnx(Input first, Input last) {
    if (first == last) {
        throw Error("MSETNX: no key specified");
    }

    auto reply = command(cmd::msetnx<Input>, first, last);

    return reply::parse<bool>(*reply);
}

inline void RedisCluster::psetex(const StringView &key,
                            const std::chrono::milliseconds &ttl,
                            const StringView &val) {
    return psetex(key, ttl.count(), val);
}

inline void RedisCluster::setex(const StringView &key,
                            const std::chrono::seconds &ttl,
                            const StringView &val) {
    setex(key, ttl.count(), val);
}

// LIST commands.

template <typename Input>
OptionalStringPair RedisCluster::blpop(Input first, Input last, long long timeout) {
    if (first == last) {
        throw Error("BLPOP: no key specified");
    }

    auto reply = command(cmd::blpop_range<Input>, first, last, timeout);

    return reply::parse<OptionalStringPair>(*reply);
}

template <typename Input>
OptionalStringPair RedisCluster::blpop(Input first,
                                Input last,
                                const std::chrono::seconds &timeout) {
    return blpop(first, last, timeout.count());
}

template <typename Input>
OptionalStringPair RedisCluster::brpop(Input first, Input last, long long timeout) {
    if (first == last) {
        throw Error("BRPOP: no key specified");
    }

    auto reply = command(cmd::brpop_range<Input>, first, last, timeout);

    return reply::parse<OptionalStringPair>(*reply);
}

template <typename Input>
OptionalStringPair RedisCluster::brpop(Input first,
                                Input last,
                                const std::chrono::seconds &timeout) {
    return brpop(first, last, timeout.count());
}

inline OptionalString RedisCluster::brpoplpush(const StringView &source,
                                        const StringView &destination,
                                        const std::chrono::seconds &timeout) {
    return brpoplpush(source, destination, timeout.count());
}

template <typename Input>
inline long long RedisCluster::lpush(const StringView &key, Input first, Input last) {
    if (first == last) {
        throw Error("LPUSH: no key specified");
    }

    auto reply = command(cmd::lpush_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
inline void RedisCluster::lrange(const StringView &key, long long start, long long stop, Output output) {
    auto reply = command(cmd::lrange, key, start, stop);

    reply::to_array(*reply, output);
}

template <typename Input>
inline long long RedisCluster::rpush(const StringView &key, Input first, Input last) {
    if (first == last) {
        throw Error("RPUSH: no key specified");
    }

    auto reply = command(cmd::rpush_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

// HASH commands.

template <typename Input>
inline long long RedisCluster::hdel(const StringView &key, Input first, Input last) {
    if (first == last) {
        throw Error("HDEL: no key specified");
    }

    auto reply = command(cmd::hdel_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
inline void RedisCluster::hgetall(const StringView &key, Output output) {
    auto reply = command(cmd::hgetall, key);

    reply::to_array(*reply, output);
}

template <typename Output>
inline void RedisCluster::hkeys(const StringView &key, Output output) {
    auto reply = command(cmd::hkeys, key);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
inline void RedisCluster::hmget(const StringView &key, Input first, Input last, Output output) {
    if (first == last) {
        throw Error("HMGET: no key specified");
    }

    auto reply = command(cmd::hmget<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
inline void RedisCluster::hmset(const StringView &key, Input first, Input last) {
    if (first == last) {
        throw Error("HMSET: no key specified");
    }

    auto reply = command(cmd::hmset<Input>, key, first, last);

    reply::parse<void>(*reply);
}

template <typename Output>
long long RedisCluster::hscan(const StringView &key,
                        long long cursor,
                        const StringView &pattern,
                        long long count,
                        Output output) {
    auto reply = command(cmd::hscan, key, cursor, pattern, count);

    return reply::parse_scan_reply(*reply, output);
}

template <typename Output>
inline long long RedisCluster::hscan(const StringView &key,
                                long long cursor,
                                const StringView &pattern,
                                Output output) {
    return hscan(key, cursor, pattern, 10, output);
}

template <typename Output>
inline long long RedisCluster::hscan(const StringView &key,
                                long long cursor,
                                long long count,
                                Output output) {
    return hscan(key, cursor, "*", count, output);
}

template <typename Output>
inline long long RedisCluster::hscan(const StringView &key,
                                long long cursor,
                                Output output) {
    return hscan(key, cursor, "*", 10, output);
}

template <typename Output>
inline void RedisCluster::hvals(const StringView &key, Output output) {
    auto reply = command(cmd::hvals, key);

    reply::to_array(*reply, output);
}

// SET commands.

template <typename Input>
long long RedisCluster::sadd(const StringView &key, Input first, Input last) {
    if (first == last) {
        throw Error("SADD: no key specified");
    }

    auto reply = command(cmd::sadd_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::sdiff(Input first, Input last, Output output) {
    if (first == last) {
        throw Error("SDIFF: no key specified");
    }

    auto reply = command(cmd::sdiff<Input>, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::sdiffstore(const StringView &destination,
                                    Input first,
                                    Input last) {
    if (first == last) {
        throw Error("SDIFFSTORE: no key specified");
    }

    auto reply = command(cmd::sdiffstore_range<Input>, destination, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::sinter(Input first, Input last, Output output) {
    if (first == last) {
        throw Error("SINTER: no key specified");
    }

    auto reply = command(cmd::sinter<Input>, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::sinterstore(const StringView &destination,
                                    Input first,
                                    Input last) {
    if (first == last) {
        throw Error("SINTERSTORE: no key specified");
    }

    auto reply = command(cmd::sinterstore_range<Input>, destination, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
void RedisCluster::smembers(const StringView &key, Output output) {
    auto reply = command(cmd::smembers, key);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::spop(const StringView &key, long long count, Output output) {
    auto reply = command(cmd::spop_range, key, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::srandmember(const StringView &key, long long count, Output output) {
    auto reply = command(cmd::srandmember_range, key, count);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::srem(const StringView &key, Input first, Input last) {
    if (first == last) {
        throw Error("SREM: no key specified");
    }

    auto reply = command(cmd::srem_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
long long RedisCluster::sscan(const StringView &key,
                        long long cursor,
                        const StringView &pattern,
                        long long count,
                        Output output) {
    auto reply = command(cmd::sscan, key, cursor, pattern, count);

    return reply::parse_scan_reply(*reply, output);
}

template <typename Output>
inline long long RedisCluster::sscan(const StringView &key,
                                long long cursor,
                                const StringView &pattern,
                                Output output) {
    return sscan(key, cursor, pattern, 10, output);
}

template <typename Output>
inline long long RedisCluster::sscan(const StringView &key,
                                long long cursor,
                                long long count,
                                Output output) {
    return sscan(key, cursor, "*", count, output);
}

template <typename Output>
inline long long RedisCluster::sscan(const StringView &key,
                                long long cursor,
                                Output output) {
    return sscan(key, cursor, "*", 10, output);
}

template <typename Input, typename Output>
void RedisCluster::sunion(Input first, Input last, Output output) {
    if (first == last) {
        throw Error("SUNION: no key specified");
    }

    auto reply = command(cmd::sunion<Input>, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::sunionstore(const StringView &destination, Input first, Input last) {
    if (first == last) {
        throw Error("SUNIONSTORE: no key specified");
    }

    auto reply = command(cmd::sunionstore_range<Input>, destination, first, last);

    return reply::parse<long long>(*reply);
}

// SORTED SET commands.

inline auto RedisCluster::bzpopmax(const StringView &key, const std::chrono::seconds &timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    return bzpopmax(key, timeout.count());
}

template <typename Input>
auto RedisCluster::bzpopmax(Input first, Input last, long long timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    auto reply = command(cmd::bzpopmax_range<Input>, first, last, timeout);

    return reply::parse<Optional<std::tuple<std::string, std::string, double>>>(*reply);
}

template <typename Input>
inline auto RedisCluster::bzpopmax(Input first,
                                    Input last,
                                    const std::chrono::seconds &timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    return bzpopmax(first, last, timeout.count());
}

inline auto RedisCluster::bzpopmin(const StringView &key, const std::chrono::seconds &timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    return bzpopmin(key, timeout.count());
}

template <typename Input>
auto RedisCluster::bzpopmin(Input first, Input last, long long timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    auto reply = command(cmd::bzpopmin_range<Input>, first, last, timeout);

    return reply::parse<Optional<std::tuple<std::string, std::string, double>>>(*reply);
}

template <typename Input>
inline auto RedisCluster::bzpopmin(Input first,
                                    Input last,
                                    const std::chrono::seconds &timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    return bzpopmin(first, last, timeout.count());
}

template <typename Input>
long long RedisCluster::zadd(const StringView &key,
                        Input first,
                        Input last,
                        UpdateType type,
                        bool changed) {
    if (first == last) {
        throw Error("ZADD: no key specified");
    }

    auto reply = command(cmd::zadd_range<Input>, key, first, last, type, changed);

    return reply::parse<long long>(*reply);
}

template <typename Interval>
long long RedisCluster::zcount(const StringView &key, const Interval &interval) {
    auto reply = command(cmd::zcount<Interval>, key, interval);

    return reply::parse<long long>(*reply);
}

template <typename Input>
long long RedisCluster::zinterstore(const StringView &destination,
                                Input first,
                                Input last,
                                Aggregation type) {
    if (first == last) {
        throw Error("ZINTERSTORE: no key specified");
    }

    auto reply = command(cmd::zinterstore_range<Input>,
                            destination,
                            first,
                            last,
                            type);

    return reply::parse<long long>(*reply);
}

template <typename Interval>
long long RedisCluster::zlexcount(const StringView &key, const Interval &interval) {
    auto reply = command(cmd::zlexcount<Interval>, key, interval);

    return reply::parse<long long>(*reply);
}

template <typename Output>
void RedisCluster::zpopmax(const StringView &key, long long count, Output output) {
    auto reply = command(cmd::zpopmax, key, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::zpopmin(const StringView &key, long long count, Output output) {
    auto reply = command(cmd::zpopmin, key, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::zrange(const StringView &key, long long start, long long stop, Output output) {
    auto reply = _score_command<Output>(cmd::zrange, key, start, stop);

    reply::to_array(*reply, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrangebylex(const StringView &key, const Interval &interval, Output output) {
    zrangebylex(key, interval, {}, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrangebylex(const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        Output output) {
    auto reply = command(cmd::zrangebylex<Interval>, key, interval, opts);

    reply::to_array(*reply, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrangebyscore(const StringView &key,
                            const Interval &interval,
                            Output output) {
    zrangebyscore(key, interval, {}, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrangebyscore(const StringView &key,
                            const Interval &interval,
                            const LimitOptions &opts,
                            Output output) {
    auto reply = _score_command<Output>(cmd::zrangebyscore<Interval>,
                                        key,
                                        interval,
                                        opts);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::zrem(const StringView &key, Input first, Input last) {
    if (first == last) {
        throw Error("ZREM: no key specified");
    }

    auto reply = command(cmd::zrem_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Interval>
long long RedisCluster::zremrangebylex(const StringView &key, const Interval &interval) {
    auto reply = command(cmd::zremrangebylex<Interval>, key, interval);

    return reply::parse<long long>(*reply);
}

template <typename Interval>
long long RedisCluster::zremrangebyscore(const StringView &key, const Interval &interval) {
    auto reply = command(cmd::zremrangebyscore<Interval>, key, interval);

    return reply::parse<long long>(*reply);
}

template <typename Output>
void RedisCluster::zrevrange(const StringView &key, long long start, long long stop, Output output) {
    auto reply = _score_command<Output>(cmd::zrevrange, key, start, stop);

    reply::to_array(*reply, output);
}

template <typename Interval, typename Output>
inline void RedisCluster::zrevrangebylex(const StringView &key,
                                    const Interval &interval,
                                    Output output) {
    zrevrangebylex(key, interval, {}, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrevrangebylex(const StringView &key,
                            const Interval &interval,
                            const LimitOptions &opts,
                            Output output) {
    auto reply = command(cmd::zrevrangebylex<Interval>, key, interval, opts);

    reply::to_array(*reply, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrevrangebyscore(const StringView &key, const Interval &interval, Output output) {
    zrevrangebyscore(key, interval, {}, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrevrangebyscore(const StringView &key,
                                const Interval &interval,
                                const LimitOptions &opts,
                                Output output) {
    auto reply = _score_command<Output>(cmd::zrevrangebyscore<Interval>, key, interval, opts);

    reply::to_array(*reply, output);
}

template <typename Output>
long long RedisCluster::zscan(const StringView &key,
                        long long cursor,
                        const StringView &pattern,
                        long long count,
                        Output output) {
    auto reply = command(cmd::zscan, key, cursor, pattern, count);

    return reply::parse_scan_reply(*reply, output);
}

template <typename Output>
inline long long RedisCluster::zscan(const StringView &key,
                                long long cursor,
                                const StringView &pattern,
                                Output output) {
    return zscan(key, cursor, pattern, 10, output);
}

template <typename Output>
inline long long RedisCluster::zscan(const StringView &key,
                                long long cursor,
                                long long count,
                                Output output) {
    return zscan(key, cursor, "*", count, output);
}

template <typename Output>
inline long long RedisCluster::zscan(const StringView &key,
                                long long cursor,
                                Output output) {
    return zscan(key, cursor, "*", 10, output);
}

template <typename Input>
long long RedisCluster::zunionstore(const StringView &destination,
                                    Input first,
                                    Input last,
                                    Aggregation type) {
    if (first == last) {
        throw Error("ZUNIONSTORE: no key specified");
    }

    auto reply = command(cmd::zunionstore_range<Input>,
                            destination,
                            first,
                            last,
                            type);

    return reply::parse<long long>(*reply);
}

// HYPERLOGLOG commands.

template <typename Input>
bool RedisCluster::pfadd(const StringView &key, Input first, Input last) {
    if (first == last) {
        throw Error("PFADD: no key specified");
    }

    auto reply = command(cmd::pfadd_range<Input>, key, first, last);

    return reply::parse<bool>(*reply);
}

template <typename Input>
long long RedisCluster::pfcount(Input first, Input last) {
    if (first == last) {
        throw Error("PFCOUNT: no key specified");
    }

    auto reply = command(cmd::pfcount_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
void RedisCluster::pfmerge(const StringView &destination,
                    Input first,
                    Input last) {
    if (first == last) {
        throw Error("PFMERGE: no key specified");
    }

    auto reply = command(cmd::pfmerge_range<Input>, destination, first, last);

    reply::parse<void>(*reply);
}

// GEO commands.

template <typename Input>
inline long long RedisCluster::geoadd(const StringView &key,
                                Input first,
                                Input last) {
    if (first == last) {
        throw Error("GEOADD: no key specified");
    }

    auto reply = command(cmd::geoadd_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::geohash(const StringView &key, Input first, Input last, Output output) {
    if (first == last) {
        throw Error("GEOHASH: no key specified");
    }

    auto reply = command(cmd::geohash_range<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::geopos(const StringView &key, Input first, Input last, Output output) {
    if (first == last) {
        throw Error("GEOPOS: no key specified");
    }

    auto reply = command(cmd::geopos_range<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::georadius(const StringView &key,
                        const std::pair<double, double> &loc,
                        double radius,
                        GeoUnit unit,
                        long long count,
                        bool asc,
                        Output output) {
    auto reply = command(cmd::georadius,
                            key,
                            loc,
                            radius,
                            unit,
                            count,
                            asc,
                            WithCoord<typename IterType<Output>::type>::value,
                            WithDist<typename IterType<Output>::type>::value,
                            WithHash<typename IterType<Output>::type>::value);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::georadiusbymember(const StringView &key,
                                const StringView &member,
                                double radius,
                                GeoUnit unit,
                                long long count,
                                bool asc,
                                Output output) {
    auto reply = command(cmd::georadiusbymember,
                            key,
                            member,
                            radius,
                            unit,
                            count,
                            asc,
                            WithCoord<typename IterType<Output>::type>::value,
                            WithDist<typename IterType<Output>::type>::value,
                            WithHash<typename IterType<Output>::type>::value);

    reply::to_array(*reply, output);
}

// SCRIPTING commands.

template <typename Result>
Result RedisCluster::eval(const StringView &script,
                            std::initializer_list<StringView> keys,
                            std::initializer_list<StringView> args) {
    if (keys.size() == 0) {
        throw Error("DO NOT support Lua script without key");
    }

    auto reply = _command(cmd::eval, *keys.begin(), script, keys, args);

    return reply::parse<Result>(*reply);
}

template <typename Output>
void RedisCluster::eval(const StringView &script,
                        std::initializer_list<StringView> keys,
                        std::initializer_list<StringView> args,
                        Output output) {
    if (keys.size() == 0) {
        throw Error("DO NOT support Lua script without key");
    }

    auto reply = _command(cmd::eval, *keys.begin(), script, keys, args);

    reply::to_array(*reply, output);
}

template <typename Result>
Result RedisCluster::evalsha(const StringView &script,
                                std::initializer_list<StringView> keys,
                                std::initializer_list<StringView> args) {
    if (keys.size() == 0) {
        throw Error("DO NOT support Lua script without key");
    }

    auto reply = _command(cmd::evalsha, *keys.begin(), script, keys, args);

    return reply::parse<Result>(*reply);
}

template <typename Output>
void RedisCluster::evalsha(const StringView &script,
                            std::initializer_list<StringView> keys,
                            std::initializer_list<StringView> args,
                            Output output) {
    if (keys.size() == 0) {
        throw Error("DO NOT support Lua script without key");
    }

    auto reply = command(cmd::evalsha, *keys.begin(), script, keys, args);

    reply::to_array(*reply, output);
}

// Stream commands.

template <typename Input>
long long RedisCluster::xack(const StringView &key,
                                const StringView &group,
                                Input first,
                                Input last) {
    auto reply = command(cmd::xack_range<Input>, key, group, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
std::string RedisCluster::xadd(const StringView &key,
                                const StringView &id,
                                Input first,
                                Input last) {
    auto reply = command(cmd::xadd_range<Input>, key, id, first, last);

    return reply::parse<std::string>(*reply);
}

template <typename Input>
std::string RedisCluster::xadd(const StringView &key,
                                const StringView &id,
                                Input first,
                                Input last,
                                long long count,
                                bool approx) {
    auto reply = command(cmd::xadd_maxlen_range<Input>, key, id, first, last, count, approx);

    return reply::parse<std::string>(*reply);
}

template <typename Output>
void RedisCluster::xclaim(const StringView &key,
                            const StringView &group,
                            const StringView &consumer,
                            const std::chrono::milliseconds &min_idle_time,
                            const StringView &id,
                            Output output) {
    auto reply = command(cmd::xclaim, key, group, consumer, min_idle_time.count(), id);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::xclaim(const StringView &key,
                            const StringView &group,
                            const StringView &consumer,
                            const std::chrono::milliseconds &min_idle_time,
                            Input first,
                            Input last,
                            Output output) {
    auto reply = command(cmd::xclaim_range<Input>,
                            key,
                            group,
                            consumer,
                            min_idle_time.count(),
                            first,
                            last);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::xdel(const StringView &key, Input first, Input last) {
    auto reply = command(cmd::xdel_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
auto RedisCluster::xpending(const StringView &key, const StringView &group, Output output)
    -> std::tuple<long long, OptionalString, OptionalString> {
    auto reply = command(cmd::xpending, key, group);

    return reply::parse_xpending_reply(*reply, output);
}

template <typename Output>
void RedisCluster::xpending(const StringView &key,
                            const StringView &group,
                            const StringView &start,
                            const StringView &end,
                            long long count,
                            Output output) {
    auto reply = command(cmd::xpending_detail, key, group, start, end, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xpending(const StringView &key,
                            const StringView &group,
                            const StringView &start,
                            const StringView &end,
                            long long count,
                            const StringView &consumer,
                            Output output) {
    auto reply = command(cmd::xpending_per_consumer, key, group, start, end, count, consumer);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xrange(const StringView &key,
                            const StringView &start,
                            const StringView &end,
                            Output output) {
    auto reply = command(cmd::xrange, key, start, end);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xrange(const StringView &key,
                            const StringView &start,
                            const StringView &end,
                            long long count,
                            Output output) {
    auto reply = command(cmd::xrange_count, key, start, end, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xread(const StringView &key,
                            const StringView &id,
                            long long count,
                            Output output) {
    auto reply = command(cmd::xread, key, id, count);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Input, typename Output>
auto RedisCluster::xread(Input first, Input last, long long count, Output output)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
    if (first == last) {
        throw Error("XREAD: no key specified");
    }

    auto reply = command(cmd::xread_range<Input>, first, last, count);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Output>
void RedisCluster::xread(const StringView &key,
                            const StringView &id,
                            const std::chrono::milliseconds &timeout,
                            long long count,
                            Output output) {
    auto reply = command(cmd::xread_block, key, id, timeout.count(), count);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Input, typename Output>
auto RedisCluster::xread(Input first,
                            Input last,
                            const std::chrono::milliseconds &timeout,
                            long long count,
                            Output output)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
    if (first == last) {
        throw Error("XREAD: no key specified");
    }

    auto reply = command(cmd::xread_block_range<Input>, first, last, timeout.count(), count);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Output>
void RedisCluster::xreadgroup(const StringView &group,
                                const StringView &consumer,
                                const StringView &key,
                                const StringView &id,
                                long long count,
                                bool noack,
                                Output output) {
    auto reply = _command(cmd::xreadgroup, key, group, consumer, key, id, count, noack);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Input, typename Output>
auto RedisCluster::xreadgroup(const StringView &group,
                                const StringView &consumer,
                                Input first,
                                Input last,
                                long long count,
                                bool noack,
                                Output output)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
    if (first == last) {
        throw Error("XREADGROUP: no key specified");
    }

    auto reply = _command(cmd::xreadgroup_range<Input>,
                            first->first,
                            group,
                            consumer,
                            first,
                            last,
                            count,
                            noack);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Output>
void RedisCluster::xreadgroup(const StringView &group,
                                const StringView &consumer,
                                const StringView &key,
                                const StringView &id,
                                const std::chrono::milliseconds &timeout,
                                long long count,
                                bool noack,
                                Output output) {
    auto reply = _command(cmd::xreadgroup_block,
                            key,
                            group,
                            consumer,
                            key,
                            id,
                            timeout.count(),
                            count,
                            noack);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Input, typename Output>
auto RedisCluster::xreadgroup(const StringView &group,
                                const StringView &consumer,
                                Input first,
                                Input last,
                                const std::chrono::milliseconds &timeout,
                                long long count,
                                bool noack,
                                Output output)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
    if (first == last) {
        throw Error("XREADGROUP: no key specified");
    }

    auto reply = _command(cmd::xreadgroup_block_range<Input>,
                            first->first,
                            group,
                            consumer,
                            first,
                            last,
                            timeout.count(),
                            count,
                            noack);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Output>
void RedisCluster::xrevrange(const StringView &key,
                            const StringView &end,
                            const StringView &start,
                            Output output) {
    auto reply = command(cmd::xrevrange, key, end, start);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xrevrange(const StringView &key,
                                const StringView &end,
                                const StringView &start,
                                long long count,
                                Output output) {
    auto reply = command(cmd::xrevrange_count, key, end, start, count);

    reply::to_array(*reply, output);
}

template <typename Cmd, typename Key, typename ...Args>
auto RedisCluster::_generic_command(Cmd cmd, Key &&key, Args &&...args)
    -> typename std::enable_if<std::is_convertible<Key, StringView>::value,
                                ReplyUPtr>::type {
    return command(cmd, std::forward<Key>(key), std::forward<Args>(args)...);
}

template <typename Cmd, typename Key, typename ...Args>
auto RedisCluster::_generic_command(Cmd cmd, Key &&key, Args &&...args)
    -> typename std::enable_if<std::is_arithmetic<typename std::decay<Key>::type>::value,
                                ReplyUPtr>::type {
    auto k = std::to_string(std::forward<Key>(key));
    return command(cmd, k, std::forward<Args>(args)...);
}

template <typename Cmd, typename ...Args>
ReplyUPtr RedisCluster::_command(Cmd cmd, std::true_type, const StringView &key, Args &&...args) {
    return _command(cmd, key, key, std::forward<Args>(args)...);
}

template <typename Cmd, typename Input, typename ...Args>
ReplyUPtr RedisCluster::_command(Cmd cmd, std::false_type, Input &&first, Args &&...args) {
    return _range_command(cmd,
                            std::is_convertible<
                                typename std::decay<
                                    decltype(*std::declval<Input>())>::type, StringView>(),
                            std::forward<Input>(first),
                            std::forward<Args>(args)...);
}

template <typename Cmd, typename Input, typename ...Args>
ReplyUPtr RedisCluster::_range_command(Cmd cmd, std::true_type, Input input, Args &&...args) {
    return _command(cmd, *input, input, std::forward<Args>(args)...);
}

template <typename Cmd, typename Input, typename ...Args>
ReplyUPtr RedisCluster::_range_command(Cmd cmd, std::false_type, Input input, Args &&...args) {
    return _command(cmd, std::get<0>(*input), input, std::forward<Args>(args)...);
}

template <typename Cmd, typename ...Args>
ReplyUPtr RedisCluster::_command(Cmd cmd, Connection &connection, Args &&...args) {
    assert(!connection.broken());

    cmd(connection, std::forward<Args>(args)...);

    return connection.recv();
}

template <typename Cmd, typename ...Args>
ReplyUPtr RedisCluster::_command(Cmd cmd, const StringView &key, Args &&...args) {
    for (auto idx = 0; idx < 2; ++idx) {
        try {
            auto guarded_connection = _pool.fetch(key);

            return _command(cmd, guarded_connection.connection(), std::forward<Args>(args)...);
        } catch (const IoError &err) {
            // When master is down, one of its replicas will be promoted to be the new master.
            // If we try to send command to the old master, we'll get an *IoError*.
            // In this case, we need to update the slots mapping.
            _pool.update();
        } catch (const ClosedError &err) {
            // Node might be removed.
            // 1. Get up-to-date slot mapping to check if the node still exists.
            _pool.update();

            // TODO:
            // 2. If it's NOT exist, update slot mapping, and retry.
            // 3. If it's still exist, that means the node is down, NOT removed, throw exception.
        } catch (const MovedError &err) {
            // Slot mapping has been changed, update it and try again.
            _pool.update();
        } catch (const AskError &err) {
            auto guarded_connection = _pool.fetch(err.node());
            auto &connection = guarded_connection.connection();

            // 1. send ASKING command.
            _asking(connection);

            // 2. resend last command.
            try {
                return _command(cmd, connection, std::forward<Args>(args)...);
            } catch (const MovedError &err) {
                throw Error("Slot migrating... ASKING node hasn't been set to IMPORTING state");
            }
        } // For other exceptions, just throw it.
    }

    // Possible failures:
    // 1. Source node has already run 'CLUSTER SETSLOT xxx NODE xxx',
    //    while the destination node has NOT run it.
    //    In this case, client will be redirected by both nodes with MovedError.
    // 2. Other failures...
    throw Error("Failed to send command with key: " + std::string(key.data(), key.size()));
}

template <typename Cmd, typename ...Args>
inline ReplyUPtr RedisCluster::_score_command(std::true_type, Cmd cmd, Args &&... args) {
    return command(cmd, std::forward<Args>(args)..., true);
}

template <typename Cmd, typename ...Args>
inline ReplyUPtr RedisCluster::_score_command(std::false_type, Cmd cmd, Args &&... args) {
    return command(cmd, std::forward<Args>(args)..., false);
}

template <typename Output, typename Cmd, typename ...Args>
inline ReplyUPtr RedisCluster::_score_command(Cmd cmd, Args &&... args) {
    return _score_command(typename IsKvPairIter<Output>::type(),
                            cmd,
                            std::forward<Args>(args)...);
}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_REDIS_CLUSTER_HPP
