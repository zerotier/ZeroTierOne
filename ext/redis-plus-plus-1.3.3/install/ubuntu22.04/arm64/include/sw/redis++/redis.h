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

#ifndef SEWENEW_REDISPLUSPLUS_REDIS_H
#define SEWENEW_REDISPLUSPLUS_REDIS_H

#include <string>
#include <chrono>
#include <memory>
#include <initializer_list>
#include <tuple>
#include "connection_pool.h"
#include "reply.h"
#include "command_options.h"
#include "utils.h"
#include "subscriber.h"
#include "pipeline.h"
#include "transaction.h"
#include "sentinel.h"

namespace sw {

namespace redis {

template <typename Impl>
class QueuedRedis;

using Transaction = QueuedRedis<TransactionImpl>;

using Pipeline = QueuedRedis<PipelineImpl>;

class Redis {
public:
    /// @brief Construct `Redis` instance with connection options and connection pool options.
    /// @param connection_opts Connection options.
    /// @param pool_opts Connection pool options.
    /// @see `ConnectionOptions`
    /// @see `ConnectionPoolOptions`
    /// @see https://github.com/sewenew/redis-plus-plus#connection
    explicit Redis(const ConnectionOptions &connection_opts,
            const ConnectionPoolOptions &pool_opts = {}) :
                _pool(std::make_shared<ConnectionPool>(pool_opts, connection_opts)) {}

    /// @brief Construct `Redis` instance with URI.
    /// @param uri URI, e.g. 'tcp://127.0.0.1', 'tcp://127.0.0.1:6379', or 'unix://path/to/socket'.
    ///            Full URI scheme: 'tcp://[[username:]password@]host[:port][/db]' or
    ///            unix://[[username:]password@]path-to-unix-domain-socket[/db]
    /// @see https://github.com/sewenew/redis-plus-plus/issues/37
    /// @see https://github.com/sewenew/redis-plus-plus#connection
    explicit Redis(const std::string &uri);

    /// @brief Construct `Redis` instance with Redis sentinel, i.e. get node info from sentinel.
    /// @param sentinel `Sentinel` instance.
    /// @param master_name Name of master node.
    /// @param role Connect to master node or slave node.
    ///             - Role::MASTER: Connect to master node.
    ///             - Role::SLAVE: Connect to slave node.
    /// @param connection_opts Connection options.
    /// @param pool_opts Connection pool options.
    /// @see `Sentinel`
    /// @see `Role`
    /// @see https://github.com/sewenew/redis-plus-plus#redis-sentinel
    Redis(const std::shared_ptr<Sentinel> &sentinel,
            const std::string &master_name,
            Role role,
            const ConnectionOptions &connection_opts,
            const ConnectionPoolOptions &pool_opts = {}) :
                _pool(std::make_shared<ConnectionPool>(SimpleSentinel(sentinel, master_name, role),
                                                        pool_opts,
                                                        connection_opts)) {}

    /// @brief `Redis` is not copyable.
    Redis(const Redis &) = delete;

    /// @brief `Redis` is not copyable.
    Redis& operator=(const Redis &) = delete;

    /// @brief `Redis` is movable.
    Redis(Redis &&) = default;

    /// @brief `Redis` is movable.
    Redis& operator=(Redis &&) = default;

    /// @brief Create a pipeline.
    /// @param new_connection Whether creating a `Pipeline` object in a new connection.
    /// @return The created pipeline.
    /// @note Instead of picking a connection from the underlying connection pool,
    ///       this method will create a new connection to Redis. So it's not a cheap operation,
    ///       and you'd better reuse the returned object as much as possible.
    /// @see https://github.com/sewenew/redis-plus-plus#pipeline
    Pipeline pipeline(bool new_connection = true);

    /// @brief Create a transaction.
    /// @param piped Whether commands in a transaction should be sent in a pipeline to reduce RTT.
    /// @param new_connection Whether creating a `Pipeline` object in a new connection.
    /// @return The created transaction.
    /// @note Instead of picking a connection from the underlying connection pool,
    ///       this method will create a new connection to Redis. So it's not a cheap operation,
    ///       and you'd better reuse the returned object as much as possible.
    /// @see https://github.com/sewenew/redis-plus-plus#transaction
    Transaction transaction(bool piped = false, bool new_connection = true);

    /// @brief Create a subscriber.
    /// @return The created subscriber.
    /// @note Instead of picking a connection from the underlying connection pool,
    ///       this method will create a new connection to Redis. So it's not a cheap operation,
    ///       and you'd better reuse the returned object as much as possible.
    /// @see https://github.com/sewenew/redis-plus-plus#publishsubscribe
    Subscriber subscriber();

    template <typename Cmd, typename ...Args>
    auto command(Cmd cmd, Args &&...args)
        -> typename std::enable_if<!std::is_convertible<Cmd, StringView>::value, ReplyUPtr>::type;

    template <typename ...Args>
    auto command(const StringView &cmd_name, Args &&...args)
        -> typename std::enable_if<!IsIter<typename LastType<Args...>::type>::value,
                                    ReplyUPtr>::type;

    template <typename ...Args>
    auto command(const StringView &cmd_name, Args &&...args)
        -> typename std::enable_if<IsIter<typename LastType<Args...>::type>::value, void>::type;

    template <typename Result, typename ...Args>
    Result command(const StringView &cmd_name, Args &&...args);

    template <typename Input>
    auto command(Input first, Input last)
        -> typename std::enable_if<IsIter<Input>::value, ReplyUPtr>::type;

    template <typename Result, typename Input>
    auto command(Input first, Input last)
        -> typename std::enable_if<IsIter<Input>::value, Result>::type;

    template <typename Input, typename Output>
    auto command(Input first, Input last, Output output)
        -> typename std::enable_if<IsIter<Input>::value, void>::type;

    // CONNECTION commands.

    /// @brief Send password to Redis.
    /// @param password Password.
    /// @note Normally, you should not call this method.
    ///       Instead, you should set password with `ConnectionOptions` or URI.
    /// @see https://redis.io/commands/auth
    void auth(const StringView &password);

    /// @brief Send user and password to Redis.
    /// @param user User name.
    /// @param password Password.
    /// @note Normally, you should not call this method.
    ///       Instead, you should set password with `ConnectionOptions` or URI.
    ///       Also this overload only works with Redis 6.0 or later.
    /// @see https://redis.io/commands/auth
    void auth(const StringView &user, const StringView &password);

    /// @brief Ask Redis to return the given message.
    /// @param msg Message to be sent.
    /// @return Return the given message.
    /// @see https://redis.io/commands/echo
    std::string echo(const StringView &msg);

    /// @brief Test if the connection is alive.
    /// @return Always return *PONG*.
    /// @see https://redis.io/commands/ping
    std::string ping();

    /// @brief Test if the connection is alive.
    /// @param msg Message sent to Redis.
    /// @return Return the given message.
    /// @see https://redis.io/commands/ping
    std::string ping(const StringView &msg);

    // After sending QUIT, only the current connection will be close, while
    // other connections in the pool is still open. This is a strange behavior.
    // So we DO NOT support the QUIT command. If you want to quit connection to
    // server, just destroy the Redis object.
    //
    // void quit();

    // We get a connection from the pool, and send the SELECT command to switch
    // to a specified DB. However, when we try to send other commands to the
    // given DB, we might get a different connection from the pool, and these
    // commands, in fact, work on other DB. e.g.
    //
    // redis.select(1); // get a connection from the pool and switch to the 1th DB
    // redis.get("key"); // might get another connection from the pool,
    //                   // and try to get 'key' on the default DB
    //
    // Obviously, this is NOT what we expect. So we DO NOT support SELECT command.
    // In order to select a DB, we can specify the DB index with the ConnectionOptions.
    //
    // However, since Pipeline and Transaction always send multiple commands on a
    // single connection, these two classes have a *select* method.
    //
    // void select(long long idx);

    /// @brief Swap two Redis databases.
    /// @param idx1 The index of the first database.
    /// @param idx2 The index of the second database.
    /// @see https://redis.io/commands/swapdb
    void swapdb(long long idx1, long long idx2);

    // SERVER commands.

    /// @brief Rewrite AOF in the background.
    /// @see https://redis.io/commands/bgrewriteaof
    void bgrewriteaof();

    /// @brief Save database in the background.
    /// @see https://redis.io/commands/bgsave
    void bgsave();

    /// @brief Get the size of the currently selected database.
    /// @return Number of keys in currently selected database.
    /// @see https://redis.io/commands/dbsize
    long long dbsize();

    /// @brief Remove keys of all databases.
    /// @param async Whether flushing databases asynchronously, i.e. without blocking the server.
    /// @see https://redis.io/commands/flushall
    void flushall(bool async = false);

    /// @brief Remove keys of current databases.
    /// @param async Whether flushing databases asynchronously, i.e. without blocking the server.
    /// @see https://redis.io/commands/flushdb
    void flushdb(bool async = false);

    /// @brief Get the info about the server.
    /// @return Server info.
    /// @see https://redis.io/commands/info
    std::string info();

    /// @brief Get the info about the server on the given section.
    /// @param section Section.
    /// @return Server info.
    /// @see https://redis.io/commands/info
    std::string info(const StringView &section);

    /// @brief Get the UNIX timestamp in seconds, at which the database was saved successfully.
    /// @return The last saving time.
    /// @see https://redis.io/commands/lastsave
    long long lastsave();

    /// @brief Save databases into RDB file **synchronously**, i.e. block the server during saving.
    /// @see https://redis.io/commands/save
    void save();

    // KEY commands.

    /// @brief Delete the given key.
    /// @param key Key.
    /// @return Number of keys removed.
    /// @retval 1 If the key exists, and has been removed.
    /// @retval 0 If the key does not exist.
    /// @see https://redis.io/commands/del
    long long del(const StringView &key);

    /// @brief Delete the given list of keys.
    /// @param first Iterator to the first key in the range.
    /// @param last Off-the-end iterator to the given range.
    /// @return Number of keys removed.
    /// @see https://redis.io/commands/del
    template <typename Input>
    long long del(Input first, Input last);

    /// @brief Delete the given list of keys.
    /// @param il Initializer list of keys to be deleted.
    /// @return Number of keys removed.
    /// @see https://redis.io/commands/del
    template <typename T>
    long long del(std::initializer_list<T> il) {
        return del(il.begin(), il.end());
    }

    /// @brief Get the serialized valued stored at key.
    /// @param key Key.
    /// @return The serialized value.
    /// @note If key does not exist, `dump` returns `OptionalString{}` (`std::nullopt`).
    /// @see https://redis.io/commands/dump
    OptionalString dump(const StringView &key);

    /// @brief Check if the given key exists.
    /// @param key Key.
    /// @return Whether the given key exists.
    /// @retval 1 If key exists.
    /// @retval 0 If key does not exist.
    /// @see https://redis.io/commands/exists
    long long exists(const StringView &key);

    /// @brief Check if the given keys exist.
    /// @param first Iterator to the first key.
    /// @param last Off-the-end iterator to the given range.
    /// @return Number of keys existing.
    /// @see https://redis.io/commands/exists
    template <typename Input>
    long long exists(Input first, Input last);

    /// @brief Check if the given keys exist.
    /// @param il Initializer list of keys to be checked.
    /// @return Number of keys existing.
    /// @see https://redis.io/commands/exists
    template <typename T>
    long long exists(std::initializer_list<T> il) {
        return exists(il.begin(), il.end());
    }

    /// @brief Set a timeout on key.
    /// @param key Key.
    /// @param timeout Timeout in seconds.
    /// @return Whether timeout has been set.
    /// @retval true If timeout has been set.
    /// @retval false If key does not exist.
    /// @see https://redis.io/commands/expire
    bool expire(const StringView &key, long long timeout);

    /// @brief Set a timeout on key.
    /// @param key Key.
    /// @param timeout Timeout in seconds.
    /// @return Whether timeout has been set.
    /// @retval true If timeout has been set.
    /// @retval false If key does not exist.
    /// @see https://redis.io/commands/expire
    bool expire(const StringView &key, const std::chrono::seconds &timeout);

    /// @brief Set a timeout on key, i.e. expire the key at a future time point.
    /// @param key Key.
    /// @param timestamp Time in seconds since UNIX epoch.
    /// @return Whether timeout has been set.
    /// @retval true If timeout has been set.
    /// @retval false If key does not exist.
    /// @see https://redis.io/commands/expireat
    bool expireat(const StringView &key, long long timestamp);

    /// @brief Set a timeout on key, i.e. expire the key at a future time point.
    /// @param key Key.
    /// @param timestamp Time in seconds since UNIX epoch.
    /// @return Whether timeout has been set.
    /// @retval true If timeout has been set.
    /// @retval false If key does not exist.
    /// @see https://redis.io/commands/expireat
    bool expireat(const StringView &key,
                    const std::chrono::time_point<std::chrono::system_clock,
                                                    std::chrono::seconds> &tp);

    /// @brief Get keys matching the given pattern.
    /// @param pattern Pattern.
    /// @param output Output iterator to the destination where the returned keys are stored.
    /// @note It's always a bad idea to call `keys`, since it might block Redis for a long time,
    ///       especially when the data set is very big.
    /// @see `Redis::scan`
    /// @see https://redis.io/commands/keys
    template <typename Output>
    void keys(const StringView &pattern, Output output);

    /// @brief Move a key to the given database.
    /// @param key Key.
    /// @param db The destination database.
    /// @return Whether key has been moved.
    /// @retval true If key has been moved.
    /// @retval false If key was not moved.
    /// @see https://redis.io/commands/move
    bool move(const StringView &key, long long db);

    /// @brief Remove timeout on key.
    /// @param key Key.
    /// @return Whether timeout has been removed.
    /// @retval true If timeout has been removed.
    /// @retval false If key does not exist, or does not have an associated timeout.
    /// @see https://redis.io/commands/persist
    bool persist(const StringView &key);

    /// @brief Set a timeout on key.
    /// @param key Key.
    /// @param timeout Timeout in milliseconds.
    /// @return Whether timeout has been set.
    /// @retval true If timeout has been set.
    /// @retval false If key does not exist.
    /// @see https://redis.io/commands/pexpire
    bool pexpire(const StringView &key, long long timeout);

    /// @brief Set a timeout on key.
    /// @param key Key.
    /// @param timeout Timeout in milliseconds.
    /// @return Whether timeout has been set.
    /// @retval true If timeout has been set.
    /// @retval false If key does not exist.
    /// @see https://redis.io/commands/pexpire
    bool pexpire(const StringView &key, const std::chrono::milliseconds &timeout);

    /// @brief Set a timeout on key, i.e. expire the key at a future time point.
    /// @param key Key.
    /// @param timestamp Time in milliseconds since UNIX epoch.
    /// @return Whether timeout has been set.
    /// @retval true If timeout has been set.
    /// @retval false If key does not exist.
    /// @see https://redis.io/commands/pexpireat
    bool pexpireat(const StringView &key, long long timestamp);

    /// @brief Set a timeout on key, i.e. expire the key at a future time point.
    /// @param key Key.
    /// @param timestamp Time in milliseconds since UNIX epoch.
    /// @return Whether timeout has been set.
    /// @retval true If timeout has been set.
    /// @retval false If key does not exist.
    /// @see https://redis.io/commands/pexpireat
    bool pexpireat(const StringView &key,
                    const std::chrono::time_point<std::chrono::system_clock,
                                                    std::chrono::milliseconds> &tp);

    /// @brief Get the TTL of a key in milliseconds.
    /// @param key Key.
    /// @return TTL of the key in milliseconds.
    /// @see https://redis.io/commands/pttl
    long long pttl(const StringView &key);

    /// @brief Get a random key from current database.
    /// @return A random key.
    /// @note If the database is empty, `randomkey` returns `OptionalString{}` (`std::nullopt`).
    /// @see https://redis.io/commands/randomkey
    OptionalString randomkey();

    /// @brief Rename `key` to `newkey`.
    /// @param key Key to be renamed.
    /// @param newkey The new name of the key.
    /// @see https://redis.io/commands/rename
    void rename(const StringView &key, const StringView &newkey);

    /// @brief Rename `key` to `newkey` if `newkey` does not exist.
    /// @param key Key to be renamed.
    /// @param newkey The new name of the key.
    /// @return Whether key has been renamed.
    /// @retval true If key has been renamed.
    /// @retval false If newkey already exists.
    /// @see https://redis.io/commands/renamenx
    bool renamenx(const StringView &key, const StringView &newkey);

    /// @brief Create a key with the value obtained by `Redis::dump`.
    /// @param key Key.
    /// @param val Value obtained by `Redis::dump`.
    /// @param ttl Timeout of the created key in milliseconds. If `ttl` is 0, set no timeout.
    /// @param replace Whether to overwrite an existing key.
    ///                If `replace` is `true` and key already exists, throw an exception.
    /// @see https://redis.io/commands/restore
    void restore(const StringView &key,
                    const StringView &val,
                    long long ttl,
                    bool replace = false);

    /// @brief Create a key with the value obtained by `Redis::dump`.
    /// @param key Key.
    /// @param val Value obtained by `Redis::dump`.
    /// @param ttl Timeout of the created key in milliseconds. If `ttl` is 0, set no timeout.
    /// @param replace Whether to overwrite an existing key.
    ///                If `replace` is `true` and key already exists, throw an exception.
    /// @see https://redis.io/commands/restore
    void restore(const StringView &key,
                    const StringView &val,
                    const std::chrono::milliseconds &ttl = std::chrono::milliseconds{0},
                    bool replace = false);

    // TODO: sort

    /// @brief Scan keys of the database matching the given pattern.
    ///
    /// Example:
    /// @code{.cpp}
    /// auto cursor = 0LL;
    /// std::unordered_set<std::string> keys;
    /// while (true) {
    ///     cursor = redis.scan(cursor, "pattern:*", 10, std::inserter(keys, keys.begin()));
    ///     if (cursor == 0) {
    ///         break;
    ///     }
    /// }
    /// @endcode
    /// @param cursor Cursor.
    /// @param pattern Pattern of the keys to be scanned.
    /// @param count A hint for how many keys to be scanned.
    /// @param output Output iterator to the destination where the returned keys are stored.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/scan
    /// TODO: support the TYPE option for Redis 6.0.
    template <typename Output>
    long long scan(long long cursor,
                    const StringView &pattern,
                    long long count,
                    Output output);

    /// @brief Scan all keys of the database.
    /// @param cursor Cursor.
    /// @param output Output iterator to the destination where the returned keys are stored.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/scan
    template <typename Output>
    long long scan(long long cursor,
                    Output output);

    /// @brief Scan keys of the database matching the given pattern.
    /// @param cursor Cursor.
    /// @param pattern Pattern of the keys to be scanned.
    /// @param output Output iterator to the destination where the returned keys are stored.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/scan
    template <typename Output>
    long long scan(long long cursor,
                    const StringView &pattern,
                    Output output);

    /// @brief Scan keys of the database matching the given pattern.
    /// @param cursor Cursor.
    /// @param count A hint for how many keys to be scanned.
    /// @param output Output iterator to the destination where the returned keys are stored.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/scan
    template <typename Output>
    long long scan(long long cursor,
                    long long count,
                    Output output);

    /// @brief Update the last access time of the given key.
    /// @param key Key.
    /// @return Whether last access time of the key has been updated.
    /// @retval 1 If key exists, and last access time has been updated.
    /// @retval 0 If key does not exist.
    /// @see https://redis.io/commands/touch
    long long touch(const StringView &key);

    /// @brief Update the last access time of the given key.
    /// @param first Iterator to the first key to be touched.
    /// @param last Off-the-end iterator to the given range.
    /// @return Whether last access time of the key has been updated.
    /// @retval 1 If key exists, and last access time has been updated.
    /// @retval 0 If key does not exist.
    /// @see https://redis.io/commands/touch
    template <typename Input>
    long long touch(Input first, Input last);

    /// @brief Update the last access time of the given key.
    /// @param il Initializer list of keys to be touched.
    /// @return Whether last access time of the key has been updated.
    /// @retval 1 If key exists, and last access time has been updated.
    /// @retval 0 If key does not exist.
    /// @see https://redis.io/commands/touch
    template <typename T>
    long long touch(std::initializer_list<T> il) {
        return touch(il.begin(), il.end());
    }

    /// @brief Get the remaining Time-To-Live of a key.
    /// @param key Key.
    /// @return TTL in seconds.
    /// @retval TTL If the key has a timeout.
    /// @retval -1 If the key exists but does not have a timeout.
    /// @retval -2 If the key does not exist.
    /// @note In Redis 2.6 or older, `ttl` returns -1 if the key does not exist,
    ///       or if the key exists but does not have a timeout.
    /// @see https://redis.io/commands/ttl
    long long ttl(const StringView &key);

    /// @brief Get the type of the value stored at key.
    /// @param key Key.
    /// @return The type of the value.
    /// @see https://redis.io/commands/type
    std::string type(const StringView &key);

    /// @brief Remove the given key asynchronously, i.e. without blocking Redis.
    /// @param key Key.
    /// @return Whether the key has been removed.
    /// @retval 1 If key exists, and has been removed.
    /// @retval 0 If key does not exist.
    /// @see https://redis.io/commands/unlink
    long long unlink(const StringView &key);

    /// @brief Remove the given keys asynchronously, i.e. without blocking Redis.
    /// @param first Iterator to the first key in the given range.
    /// @param last Off-the-end iterator to the given range.
    /// @return Number of keys that have been removed.
    /// @see https://redis.io/commands/unlink
    template <typename Input>
    long long unlink(Input first, Input last);

    /// @brief Remove the given keys asynchronously, i.e. without blocking Redis.
    /// @param il Initializer list of keys to be unlinked.
    /// @return Number of keys that have been removed.
    /// @see https://redis.io/commands/unlink
    template <typename T>
    long long unlink(std::initializer_list<T> il) {
        return unlink(il.begin(), il.end());
    }

    /// @brief Wait until previous write commands are successfully replicated to at
    ///        least the specified number of replicas or the given timeout has been reached.
    /// @param numslaves Number of replicas.
    /// @param timeout Timeout in milliseconds. If timeout is 0ms, wait forever.
    /// @return Number of replicas that have been successfully replicated these write commands.
    /// @note The return value might be less than `numslaves`, because timeout has been reached.
    /// @see https://redis.io/commands/wait
    long long wait(long long numslaves, long long timeout);

    /// @brief Wait until previous write commands are successfully replicated to at
    ///        least the specified number of replicas or the given timeout has been reached.
    /// @param numslaves Number of replicas.
    /// @param timeout Timeout in milliseconds. If timeout is 0ms, wait forever.
    /// @return Number of replicas that have been successfully replicated these write commands.
    /// @note The return value might be less than `numslaves`, because timeout has been reached.
    /// @see https://redis.io/commands/wait
    /// TODO: Support default parameter for `timeout` to wait forever.
    long long wait(long long numslaves, const std::chrono::milliseconds &timeout);

    // STRING commands.

    /// @brief Append the given string to the string stored at key.
    /// @param key Key.
    /// @param str String to be appended.
    /// @return The length of the string after the append operation.
    /// @see https://redis.io/commands/append
    long long append(const StringView &key, const StringView &str);

    /// @brief Get the number of bits that have been set for the given range of the string.
    /// @param key Key.
    /// @param start Start index (inclusive) of the range. 0 means the beginning of the string.
    /// @param end End index (inclusive) of the range. -1 means the end of the string.
    /// @return Number of bits that have been set.
    /// @note The index can be negative to index from the end of the string.
    /// @see https://redis.io/commands/bitcount
    long long bitcount(const StringView &key, long long start = 0, long long end = -1);

    /// @brief Do bit operation on the string stored at `key`, and save the result to `destination`.
    /// @param op Bit operations.
    /// @param destination The destination key where the result is saved.
    /// @param key The key where the string to be operated is stored.
    /// @return The length of the string saved at `destination`.
    /// @see https://redis.io/commands/bitop
    /// @see `BitOp`
    long long bitop(BitOp op, const StringView &destination, const StringView &key);

    /// @brief Do bit operations on the strings stored at the given keys,
    ///        and save the result to `destination`.
    /// @param op Bit operations.
    /// @param destination The destination key where the result is saved.
    /// @param first Iterator to the first key where the string to be operated is stored.
    /// @param last Off-the-end iterator to the given range of keys.
    /// @return The length of the string saved at `destination`.
    /// @see https://redis.io/commands/bitop
    /// @see `BitOp`
    template <typename Input>
    long long bitop(BitOp op, const StringView &destination, Input first, Input last);

    /// @brief Do bit operations on the strings stored at the given keys,
    ///        and save the result to `destination`.
    /// @param op Bit operations.
    /// @param destination The destination key where the result is saved.
    /// @param il Initializer list of keys where the strings are operated.
    /// @return The length of the string saved at `destination`.
    /// @see https://redis.io/commands/bitop
    /// @see `BitOp`
    template <typename T>
    long long bitop(BitOp op, const StringView &destination, std::initializer_list<T> il) {
        return bitop(op, destination, il.begin(), il.end());
    }

    /// @brief Get the position of the first bit set to 0 or 1 in the given range of the string.
    /// @param key Key.
    /// @param bit 0 or 1.
    /// @param start Start index (inclusive) of the range. 0 means the beginning of the string.
    /// @param end End index (inclusive) of the range. -1 means the end of the string.
    /// @return The position of the first bit set to 0 or 1.
    /// @see https://redis.io/commands/bitpos
    long long bitpos(const StringView &key,
                        long long bit,
                        long long start = 0,
                        long long end = -1);

    /// @brief Decrement the integer stored at key by 1.
    /// @param key Key.
    /// @return The value after the decrement.
    /// @see https://redis.io/commands/decr
    long long decr(const StringView &key);

    /// @brief Decrement the integer stored at key by `decrement`.
    /// @param key Key.
    /// @param decrement Decrement.
    /// @return The value after the decrement.
    /// @see https://redis.io/commands/decrby
    long long decrby(const StringView &key, long long decrement);

    /// @brief Get the string value stored at key.
    ///
    /// Example:
    /// @code{.cpp}
    /// auto val = redis.get("key");
    /// if (val)
    ///     std::cout << *val << std::endl;
    /// else
    ///     std::cout << "key not exist" << std::endl;
    /// @endcode
    /// @param key Key.
    /// @return The value stored at key.
    /// @note If key does not exist, `get` returns `OptionalString{}` (`std::nullopt`).
    /// @see https://redis.io/commands/get
    OptionalString get(const StringView &key);

    /// @brief Get the bit value at offset in the string.
    /// @param key Key.
    /// @param offset Offset.
    /// @return The bit value.
    /// @see https://redis.io/commands/getbit
    long long getbit(const StringView &key, long long offset);

    /// @brief Get the substring of the string stored at key.
    /// @param key Key.
    /// @param start Start index (inclusive) of the range. 0 means the beginning of the string.
    /// @param end End index (inclusive) of the range. -1 means the end of the string.
    /// @return The substring in range [start, end]. If key does not exist, return an empty string.
    /// @see https://redis.io/commands/getrange
    std::string getrange(const StringView &key, long long start, long long end);

    /// @brief Atomically set the string stored at `key` to `val`, and return the old value.
    /// @param key Key.
    /// @param val Value to be set.
    /// @return The old value stored at key.
    /// @note If key does not exist, `getset` returns `OptionalString{}` (`std::nullopt`).
    /// @see https://redis.io/commands/getset
    /// @see `OptionalString`
    OptionalString getset(const StringView &key, const StringView &val);

    /// @brief Increment the integer stored at key by 1.
    /// @param key Key.
    /// @return The value after the increment.
    /// @see https://redis.io/commands/incr
    long long incr(const StringView &key);

    /// @brief Increment the integer stored at key by `increment`.
    /// @param key Key.
    /// @param increment Increment.
    /// @return The value after the increment.
    /// @see https://redis.io/commands/incrby
    long long incrby(const StringView &key, long long increment);

    /// @brief Increment the floating point number stored at key by `increment`.
    /// @param key Key.
    /// @param increment Increment.
    /// @return The value after the increment.
    /// @see https://redis.io/commands/incrbyfloat
    double incrbyfloat(const StringView &key, double increment);

    /// @brief Get the values of multiple keys atomically.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> keys = {"k1", "k2", "k3"};
    /// std::vector<OptionalString> vals;
    /// redis.mget(keys.begin(), keys.end(), std::back_inserter(vals));
    /// for (const auto &val : vals) {
    ///     if (val)
    ///         std::cout << *val << std::endl;
    ///     else
    ///         std::cout << "key does not exist" << std::endl;
    /// }
    /// @endcode
    /// @param first Iterator to the first key of the given range.
    /// @param last Off-the-end iterator to the given range.
    /// @param output Output iterator to the destination where the values are stored.
    /// @note The destination should be a container of `OptionalString` type,
    ///       since the given key might not exist (in this case, the value of the corresponding
    ///       key is `OptionalString{}` (`std::nullopt`)).
    /// @see https://redis.io/commands/mget
    template <typename Input, typename Output>
    void mget(Input first, Input last, Output output);

    /// @brief Get the values of multiple keys atomically.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<OptionalString> vals;
    /// redis.mget({"k1", "k2", "k3"}, std::back_inserter(vals));
    /// for (const auto &val : vals) {
    ///     if (val)
    ///         std::cout << *val << std::endl;
    ///     else
    ///         std::cout << "key does not exist" << std::endl;
    /// }
    /// @endcode
    /// @param il Initializer list of keys.
    /// @param output Output iterator to the destination where the values are stored.
    /// @note The destination should be a container of `OptionalString` type,
    ///       since the given key might not exist (in this case, the value of the corresponding
    ///       key is `OptionalString{}` (`std::nullopt`)).
    /// @see https://redis.io/commands/mget
    template <typename T, typename Output>
    void mget(std::initializer_list<T> il, Output output) {
        mget(il.begin(), il.end(), output);
    }

    /// @brief Set multiple key-value pairs.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::pair<std::string, std::string>> kvs1 = {{"k1", "v1"}, {"k2", "v2"}};
    /// redis.mset(kvs1.begin(), kvs1.end());
    /// std::unordered_map<std::string, std::string> kvs2 = {{"k3", "v3"}, {"k4", "v4"}};
    /// redis.mset(kvs2.begin(), kvs2.end());
    /// @endcode
    /// @param first Iterator to the first key-value pair.
    /// @param last Off-the-end iterator to the given range.
    /// @see https://redis.io/commands/mset
    template <typename Input>
    void mset(Input first, Input last);

    /// @brief Set multiple key-value pairs.
    ///
    /// Example:
    /// @code{.cpp}
    /// redis.mset({std::make_pair("k1", "v1"), std::make_pair("k2", "v2")});
    /// @endcode
    /// @param il Initializer list of key-value pairs.
    /// @see https://redis.io/commands/mset
    template <typename T>
    void mset(std::initializer_list<T> il) {
        mset(il.begin(), il.end());
    }

    /// @brief Set the given key-value pairs if all specified keys do not exist.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::pair<std::string, std::string>> kvs1;
    /// redis.msetnx(kvs1.begin(), kvs1.end());
    /// std::unordered_map<std::string, std::string> kvs2;
    /// redis.msetnx(kvs2.begin(), kvs2.end());
    /// @endcode
    /// @param first Iterator to the first key-value pair.
    /// @param last Off-the-end iterator of the given range.
    /// @return Whether all keys have been set.
    /// @retval true If all keys have been set.
    /// @retval false If no key was set, i.e. at least one key already exist.
    /// @see https://redis.io/commands/msetnx
    template <typename Input>
    bool msetnx(Input first, Input last);

    /// @brief Set the given key-value pairs if all specified keys do not exist.
    ///
    /// Example:
    /// @code{.cpp}
    /// redis.msetnx({make_pair("k1", "v1"), make_pair("k2", "v2")});
    /// @endcode
    /// @param il Initializer list of key-value pairs.
    /// @return Whether all keys have been set.
    /// @retval true If all keys have been set.
    /// @retval false If no key was set, i.e. at least one key already exist.
    /// @see https://redis.io/commands/msetnx
    template <typename T>
    bool msetnx(std::initializer_list<T> il) {
        return msetnx(il.begin(), il.end());
    }

    /// @brief Set key-value pair with the given timeout in milliseconds.
    /// @param key Key.
    /// @param ttl Time-To-Live in milliseconds.
    /// @param val Value.
    /// @see https://redis.io/commands/psetex
    void psetex(const StringView &key,
                long long ttl,
                const StringView &val);

    /// @brief Set key-value pair with the given timeout in milliseconds.
    /// @param key Key.
    /// @param ttl Time-To-Live in milliseconds.
    /// @param val Value.
    /// @see https://redis.io/commands/psetex
    void psetex(const StringView &key,
                const std::chrono::milliseconds &ttl,
                const StringView &val);

    /// @brief Set a key-value pair.
    ///
    /// Example:
    /// @code{.cpp}
    /// // Set a key-value pair.
    /// redis.set("key", "value");
    /// // Set a key-value pair, and expire it after 10 seconds.
    /// redis.set("key", "value", std::chrono::seconds(10));
    /// // Set a key-value pair with a timeout, only if the key already exists.
    /// if (redis.set("key", "value", std::chrono::seconds(10), UpdateType::EXIST))
    ///     std::cout << "OK" << std::endl;
    /// else
    ///     std::cout << "key does not exist" << std::endl;
    /// @endcode
    /// @param key Key.
    /// @param val Value.
    /// @param ttl Timeout on the key. If `ttl` is 0ms, do not set timeout.
    /// @param type Options for set command:
    ///             - UpdateType::EXIST: Set the key only if it already exists.
    ///             - UpdateType::NOT_EXIST: Set the key only if it does not exist.
    ///             - UpdateType::ALWAYS: Always set the key no matter whether it exists.
    /// @return Whether the key has been set.
    /// @retval true If the key has been set.
    /// @retval false If the key was not set, because of the given option.
    /// @see https://redis.io/commands/set
    // TODO: Support KEEPTTL option for Redis 6.0
    bool set(const StringView &key,
                const StringView &val,
                const std::chrono::milliseconds &ttl = std::chrono::milliseconds(0),
                UpdateType type = UpdateType::ALWAYS);

    // TODO: add SETBIT command.

    /// @brief Set key-value pair with the given timeout in seconds.
    /// @param key Key.
    /// @param ttl Time-To-Live in seconds.
    /// @param val Value.
    /// @see https://redis.io/commands/setex
    void setex(const StringView &key,
                long long ttl,
                const StringView &val);

    /// @brief Set key-value pair with the given timeout in seconds.
    /// @param key Key.
    /// @param ttl Time-To-Live in seconds.
    /// @param val Value.
    /// @see https://redis.io/commands/setex
    void setex(const StringView &key,
                const std::chrono::seconds &ttl,
                const StringView &val);

    /// @brief Set the key if it does not exist.
    /// @param key Key.
    /// @param val Value.
    /// @return Whether the key has been set.
    /// @retval true If the key has been set.
    /// @retval false If the key was not set, i.e. the key already exists.
    /// @see https://redis.io/commands/setnx
    bool setnx(const StringView &key, const StringView &val);

    /// @brief Set the substring starting from `offset` to the given value.
    /// @param key Key.
    /// @param offset Offset.
    /// @param val Value.
    /// @return The length of the string after this operation.
    /// @see https://redis.io/commands/setrange
    long long setrange(const StringView &key, long long offset, const StringView &val);

    /// @brief Get the length of the string stored at key.
    /// @param key Key.
    /// @return The length of the string.
    /// @note If key does not exist, `strlen` returns 0.
    /// @see https://redis.io/commands/strlen
    long long strlen(const StringView &key);

    // LIST commands.

    /// @brief Pop the first element of the list in a blocking way.
    /// @param key Key where the list is stored.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If list is empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::lpop`
    /// @see https://redis.io/commands/blpop
    OptionalStringPair blpop(const StringView &key, long long timeout);

    /// @brief Pop the first element of the list in a blocking way.
    /// @param key Key where the list is stored.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If list is empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::lpop`
    /// @see https://redis.io/commands/blpop
    OptionalStringPair blpop(const StringView &key,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    /// @brief Pop the first element of multiple lists in a blocking way.
    /// @param first Iterator to the first key.
    /// @param last Off-the-end iterator to the key range.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If all lists are empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::lpop`
    /// @see https://redis.io/commands/blpop
    template <typename Input>
    OptionalStringPair blpop(Input first, Input last, long long timeout);

    /// @brief Pop the first element of multiple lists in a blocking way.
    /// @param il Initializer list of keys.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If all lists are empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::lpop`
    /// @see https://redis.io/commands/blpop
    template <typename T>
    OptionalStringPair blpop(std::initializer_list<T> il, long long timeout) {
        return blpop(il.begin(), il.end(), timeout);
    }

    /// @brief Pop the first element of multiple lists in a blocking way.
    /// @param first Iterator to the first key.
    /// @param last Off-the-end iterator to the key range.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If all lists are empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::lpop`
    /// @see https://redis.io/commands/blpop
    template <typename Input>
    OptionalStringPair blpop(Input first,
                                Input last,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    /// @brief Pop the first element of multiple lists in a blocking way.
    /// @param il Initializer list of keys.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If all lists are empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::lpop`
    /// @see https://redis.io/commands/blpop
    template <typename T>
    OptionalStringPair blpop(std::initializer_list<T> il,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return blpop(il.begin(), il.end(), timeout);
    }

    /// @brief Pop the last element of the list in a blocking way.
    /// @param key Key where the list is stored.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If list is empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::rpop`
    /// @see https://redis.io/commands/brpop
    OptionalStringPair brpop(const StringView &key, long long timeout);

    /// @brief Pop the last element of the list in a blocking way.
    /// @param key Key where the list is stored.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If list is empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::rpop`
    /// @see https://redis.io/commands/brpop
    OptionalStringPair brpop(const StringView &key,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    /// @brief Pop the last element of multiple lists in a blocking way.
    /// @param first Iterator to the first key.
    /// @param last Off-the-end iterator to the key range.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If all lists are empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::rpop`
    /// @see https://redis.io/commands/brpop
    template <typename Input>
    OptionalStringPair brpop(Input first, Input last, long long timeout);

    /// @brief Pop the last element of multiple lists in a blocking way.
    /// @param il Initializer list of lists.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If all lists are empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::rpop`
    /// @see https://redis.io/commands/brpop
    template <typename T>
    OptionalStringPair brpop(std::initializer_list<T> il, long long timeout) {
        return brpop(il.begin(), il.end(), timeout);
    }

    /// @brief Pop the last element of multiple lists in a blocking way.
    /// @param first Iterator to the first list.
    /// @param last Off-the-end iterator to the list range.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If all lists are empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::rpop`
    /// @see https://redis.io/commands/brpop
    template <typename Input>
    OptionalStringPair brpop(Input first,
                                Input last,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    /// @brief Pop the last element of multiple lists in a blocking way.
    /// @param il Initializer list of list keys.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-element pair.
    /// @note If all lists are empty and timeout reaches, return `OptionalStringPair{}` (`std::nullopt`).
    /// @see `Redis::rpop`
    /// @see https://redis.io/commands/brpop
    template <typename T>
    OptionalStringPair brpop(std::initializer_list<T> il,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return brpop(il.begin(), il.end(), timeout);
    }

    /// @brief Pop last element of one list and push it to the left of another list in blocking way.
    /// @param source Key of the source list.
    /// @param destination Key of the destination list.
    /// @param timeout Timeout. 0 means block forever.
    /// @return The popped element.
    /// @note If the source list does not exist, `brpoplpush` returns `OptionalString{}` (`std::nullopt`).
    /// @see `Redis::rpoplpush`
    /// @see https://redis.io/commands/brpoplpush
    OptionalString brpoplpush(const StringView &source,
                                const StringView &destination,
                                long long timeout);

    /// @brief Pop last element of one list and push it to the left of another list in blocking way.
    /// @param source Key of the source list.
    /// @param destination Key of the destination list.
    /// @param timeout Timeout. 0 means block forever.
    /// @return The popped element.
    /// @note If the source list does not exist, `brpoplpush` returns `OptionalString{}` (`std::nullopt`).
    /// @see `Redis::rpoplpush`
    /// @see https://redis.io/commands/brpoplpush
    OptionalString brpoplpush(const StringView &source,
                                const StringView &destination,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    /// @brief Get the element at the given index of the list.
    /// @param key Key where the list is stored.
    /// @param index Zero-base index, and -1 means the last element.
    /// @return The element at the given index.
    /// @see https://redis.io/commands/lindex
    OptionalString lindex(const StringView &key, long long index);

    /// @brief Insert an element to a list before or after the pivot element.
    ///
    /// Example:
    /// @code{.cpp}
    /// // Insert 'hello' before 'world'
    /// auto len = redis.linsert("list", InsertPosition::BEFORE, "world", "hello");
    /// if (len == -1)
    ///     std::cout << "there's no 'world' in the list" << std::endl;
    /// else
    ///     std::cout << "after the operation, the length of the list is " << len << std::endl;
    /// @endcode
    /// @param key Key where the list is stored.
    /// @param position Before or after the pivot element.
    /// @param pivot The pivot value. The `pivot` is the value of the element, not the index.
    /// @param val Element to be inserted.
    /// @return The length of the list after the operation.
    /// @note If the pivot value is not found, `linsert` returns -1.
    /// @see `InsertPosition`
    /// @see https://redis.io/commands/linsert
    long long linsert(const StringView &key,
                        InsertPosition position,
                        const StringView &pivot,
                        const StringView &val);

    /// @brief Get the length of the list.
    /// @param key Key where the list is stored.
    /// @return The length of the list.
    /// @see https://redis.io/commands/llen
    long long llen(const StringView &key);

    /// @brief Pop the first element of the list.
    ///
    /// Example:
    /// @code{.cpp}
    /// auto element = redis.lpop("list");
    /// if (element)
    ///     std::cout << *element << std::endl;
    /// else
    ///     std::cout << "list is empty, i.e. list does not exist" << std::endl;
    /// @endcode
    /// @param key Key where the list is stored.
    /// @return The popped element.
    /// @note If list is empty, i.e. key does not exist, return `OptionalString{}` (`std::nullopt`).
    /// @see https://redis.io/commands/lpop
    OptionalString lpop(const StringView &key);

    /// @brief Push an element to the beginning of the list.
    /// @param key Key where the list is stored.
    /// @param val Element to be pushed.
    /// @return The length of the list after the operation.
    /// @see https://redis.io/commands/lpush
    long long lpush(const StringView &key, const StringView &val);

    /// @brief Push multiple elements to the beginning of the list.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> elements = {"e1", "e2", "e3"};
    /// redis.lpush("list", elements.begin(), elements.end());
    /// @endcode
    /// @param key Key where the list is stored.
    /// @param first Iterator to the first element to be pushed.
    /// @param last Off-the-end iterator to the given element range.
    /// @return The length of the list after the operation.
    /// @see https://redis.io/commands/lpush
    template <typename Input>
    long long lpush(const StringView &key, Input first, Input last);

    /// @brief Push multiple elements to the beginning of the list.
    ///
    /// Example:
    /// @code{.cpp}
    /// redis.lpush("list", {"e1", "e2", "e3"});
    /// @endcode
    /// @param key Key where the list is stored.
    /// @param il Initializer list of elements.
    /// @return The length of the list after the operation.
    /// @see https://redis.io/commands/lpush
    template <typename T>
    long long lpush(const StringView &key, std::initializer_list<T> il) {
        return lpush(key, il.begin(), il.end());
    }

    /// @brief Push an element to the beginning of the list, only if the list already exists.
    /// @param key Key where the list is stored.
    /// @param val Element to be pushed.
    /// @return The length of the list after the operation.
    /// @see https://redis.io/commands/lpushx
    // TODO: add a multiple elements overload.
    long long lpushx(const StringView &key, const StringView &val);

    /// @brief Get elements in the given range of the given list.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> elements;
    /// // Save all elements of a Redis list to a vector of string.
    /// redis.lrange("list", 0, -1, std::back_inserter(elements));
    /// @endcode
    /// @param key Key where the list is stored.
    /// @param start Start index of the range. Index can be negative, which mean index from the end.
    /// @param stop End index of the range.
    /// @param output Output iterator to the destination where the results are saved.
    /// @see https://redis.io/commands/lrange
    template <typename Output>
    void lrange(const StringView &key, long long start, long long stop, Output output);

    /// @brief Remove the first `count` occurrences of elements equal to `val`.
    /// @param key Key where the list is stored.
    /// @param count Number of occurrences to be removed.
    /// @param val Value.
    /// @return Number of elements removed.
    /// @note `count` can be positive, negative and 0. Check the reference for detail.
    /// @see https://redis.io/commands/lrem
    long long lrem(const StringView &key, long long count, const StringView &val);

    /// @brief Set the element at the given index to the specified value.
    /// @param key Key where the list is stored.
    /// @param index Index of the element to be set.
    /// @param val Value.
    /// @see https://redis.io/commands/lset
    void lset(const StringView &key, long long index, const StringView &val);

    /// @brief Trim a list to keep only element in the given range.
    /// @param key Key where the key is stored.
    /// @param start Start of the index.
    /// @param stop End of the index.
    /// @see https://redis.io/commands/ltrim
    void ltrim(const StringView &key, long long start, long long stop);

    /// @brief Pop the last element of a list.
    /// @param key Key where the list is stored.
    /// @return The popped element.
    /// @note If the list is empty, i.e. key does not exist, `rpop` returns `OptionalString{}` (`std::nullopt`).
    /// @see https://redis.io/commands/rpop
    OptionalString rpop(const StringView &key);

    /// @brief Pop last element of one list and push it to the left of another list.
    /// @param source Key of the source list.
    /// @param destination Key of the destination list.
    /// @return The popped element.
    /// @note If the source list does not exist, `rpoplpush` returns `OptionalString{}` (`std::nullopt`).
    /// @see https://redis.io/commands/brpoplpush
    OptionalString rpoplpush(const StringView &source, const StringView &destination);

    /// @brief Push an element to the end of the list.
    /// @param key Key where the list is stored.
    /// @param val Element to be pushed.
    /// @return The length of the list after the operation.
    /// @see https://redis.io/commands/rpush
    long long rpush(const StringView &key, const StringView &val);

    /// @brief Push multiple elements to the end of the list.
    /// @param key Key where the list is stored.
    /// @param first Iterator to the first element to be pushed.
    /// @param last Off-the-end iterator to the given element range.
    /// @return The length of the list after the operation.
    /// @see https://redis.io/commands/rpush
    template <typename Input>
    long long rpush(const StringView &key, Input first, Input last);

    /// @brief Push multiple elements to the end of the list.
    /// @param key Key where the list is stored.
    /// @param il Initializer list of elements to be pushed.
    /// @return The length of the list after the operation.
    /// @see https://redis.io/commands/rpush
    template <typename T>
    long long rpush(const StringView &key, std::initializer_list<T> il) {
        return rpush(key, il.begin(), il.end());
    }

    /// @brief Push an element to the end of the list, only if the list already exists.
    /// @param key Key where the list is stored.
    /// @param val Element to be pushed.
    /// @return The length of the list after the operation.
    /// @see https://redis.io/commands/rpushx
    long long rpushx(const StringView &key, const StringView &val);

    // HASH commands.

    /// @brief Remove the given field from hash.
    /// @param key Key where the hash is stored.
    /// @param field Field to be removed.
    /// @return Whether the field has been removed.
    /// @retval 1 If the field exists, and has been removed.
    /// @retval 0 If the field does not exist.
    /// @see https://redis.io/commands/hdel
    long long hdel(const StringView &key, const StringView &field);

    /// @brief Remove multiple fields from hash.
    /// @param key Key where the hash is stored.
    /// @param first Iterator to the first field to be removed.
    /// @param last Off-the-end iterator to the given field range.
    /// @return Number of fields that has been removed.
    /// @see https://redis.io/commands/hdel
    template <typename Input>
    long long hdel(const StringView &key, Input first, Input last);

    /// @brief Remove multiple fields from hash.
    /// @param key Key where the hash is stored.
    /// @param il Initializer list of fields.
    /// @return Number of fields that has been removed.
    /// @see https://redis.io/commands/hdel
    template <typename T>
    long long hdel(const StringView &key, std::initializer_list<T> il) {
        return hdel(key, il.begin(), il.end());
    }

    /// @brief Check if the given field exists in hash.
    /// @param key Key where the hash is stored.
    /// @param field Field.
    /// @return Whether the field exists.
    /// @retval true If the field exists in hash.
    /// @retval false If the field does not exist.
    /// @see https://redis.io/commands/hexists
    bool hexists(const StringView &key, const StringView &field);

    /// @brief Get the value of the given field.
    /// @param key Key where the hash is stored.
    /// @param field Field.
    /// @return Value of the given field.
    /// @note If field does not exist, `hget` returns `OptionalString{}` (`std::nullopt`).
    /// @see https://redis.io/commands/hget
    OptionalString hget(const StringView &key, const StringView &field);

    /// @brief Get all field-value pairs of the given hash.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::unordered_map<std::string, std::string> results;
    /// // Save all field-value pairs of a Redis hash to an unordered_map<string, string>.
    /// redis.hgetall("hash", std::inserter(results, results.begin()));
    /// @endcode
    /// @param key Key where the hash is stored.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note It's always a bad idea to call `hgetall` on a large hash, since it will block Redis.
    /// @see `Redis::hscan`
    /// @see https://redis.io/commands/hgetall
    template <typename Output>
    void hgetall(const StringView &key, Output output);

    /// @brief Increment the integer stored at the given field.
    /// @param key Key where the hash is stored.
    /// @param field Field.
    /// @param increment Increment.
    /// @return The value of the field after the increment.
    /// @see https://redis.io/commands/hincrby
    long long hincrby(const StringView &key, const StringView &field, long long increment);

    /// @brief Increment the floating point number stored at the given field.
    /// @param key Key where the hash is stored.
    /// @param field Field.
    /// @param increment Increment.
    /// @return The value of the field after the increment.
    /// @see https://redis.io/commands/hincrbyfloat
    double hincrbyfloat(const StringView &key, const StringView &field, double increment);

    /// @brief Get all fields of the given hash.
    /// @param key Key where the hash is stored.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note It's always a bad idea to call `hkeys` on a large hash, since it will block Redis.
    /// @see `Redis::hscan`
    /// @see https://redis.io/commands/hkeys
    template <typename Output>
    void hkeys(const StringView &key, Output output);

    /// @brief Get the number of fields of the given hash.
    /// @param key Key where the hash is stored.
    /// @return Number of fields.
    /// @see https://redis.io/commands/hlen
    long long hlen(const StringView &key);

    /// @brief Get values of multiple fields.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> fields = {"f1", "f2"};
    /// std::vector<OptionalString> vals;
    /// redis.hmget("hash", fields.begin(), fields.end(), std::back_inserter(vals));
    /// for (const auto &val : vals) {
    ///     if (val)
    ///         std::cout << *val << std::endl;
    ///     else
    ///         std::cout << "field not exist" << std::endl;
    /// }
    /// @endcode
    /// @param key Key where the hash is stored.
    /// @param first Iterator to the first field.
    /// @param last Off-the-end iterator to the given field range.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note The destination should be a container of `OptionalString` type,
    ///       since the given field might not exist (in this case, the value of the corresponding
    ///       field is `OptionalString{}` (`std::nullopt`)).
    /// @see https://redis.io/commands/hmget
    template <typename Input, typename Output>
    void hmget(const StringView &key, Input first, Input last, Output output);

    /// @brief Get values of multiple fields.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<OptionalString> vals;
    /// redis.hmget("hash", {"f1", "f2"}, std::back_inserter(vals));
    /// for (const auto &val : vals) {
    ///     if (val)
    ///         std::cout << *val << std::endl;
    ///     else
    ///         std::cout << "field not exist" << std::endl;
    /// }
    /// @endcode
    /// @param key Key where the hash is stored.
    /// @param il Initializer list of fields.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note The destination should be a container of `OptionalString` type,
    ///       since the given field might not exist (in this case, the value of the corresponding
    ///       field is `OptionalString{}` (`std::nullopt`)).
    /// @see https://redis.io/commands/hmget
    template <typename T, typename Output>
    void hmget(const StringView &key, std::initializer_list<T> il, Output output) {
        hmget(key, il.begin(), il.end(), output);
    }

    /// @brief Set multiple field-value pairs of the given hash.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::unordered_map<std::string, std::string> m = {{"f1", "v1"}, {"f2", "v2"}};
    /// redis.hmset("hash", m.begin(), m.end());
    /// @endcode
    /// @param key Key where the hash is stored.
    /// @param first Iterator to the first field-value pair.
    /// @param last Off-the-end iterator to the range.
    /// @see https://redis.io/commands/hmset
    template <typename Input>
    void hmset(const StringView &key, Input first, Input last);

    /// @brief Set multiple field-value pairs of the given hash.
    ///
    /// Example:
    /// @code{.cpp}
    /// redis.hmset("hash", {std::make_pair("f1", "v1"), std::make_pair("f2", "v2")});
    /// @endcode
    /// @param key Key where the hash is stored.
    /// @param il Initializer list of field-value pairs.
    /// @see https://redis.io/commands/hmset
    template <typename T>
    void hmset(const StringView &key, std::initializer_list<T> il) {
        hmset(key, il.begin(), il.end());
    }

    /// @brief Scan fields of the given hash matching the given pattern.
    ///
    /// Example:
    /// @code{.cpp}
    /// auto cursor = 0LL;
    /// std::unordered_map<std::string, std::string> kvs;
    /// while (true) {
    ///     cursor = redis.hscan("hash", cursor, "pattern:*", 10, std::inserter(kvs, kvs.begin()));
    ///     if (cursor == 0) {
    ///         break;
    ///     }
    /// }
    /// @endcode
    /// @param key Key where the hash is stored.
    /// @param cursor Cursor.
    /// @param pattern Pattern of fields to be scanned.
    /// @param count A hint for how many fields to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/hscan
    template <typename Output>
    long long hscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count,
                    Output output);

    /// @brief Scan fields of the given hash matching the given pattern.
    /// @param key Key where the hash is stored.
    /// @param cursor Cursor.
    /// @param pattern Pattern of fields to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/hscan
    template <typename Output>
    long long hscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    Output output);

    /// @brief Scan all fields of the given hash.
    /// @param key Key where the hash is stored.
    /// @param cursor Cursor.
    /// @param count A hint for how many fields to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/hscan
    template <typename Output>
    long long hscan(const StringView &key,
                    long long cursor,
                    long long count,
                    Output output);

    /// @brief Scan all fields of the given hash.
    /// @param key Key where the hash is stored.
    /// @param cursor Cursor.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/hscan
    template <typename Output>
    long long hscan(const StringView &key,
                    long long cursor,
                    Output output);

    /// @brief Set hash field to value.
    /// @param key Key where the hash is stored.
    /// @param field Field.
    /// @param val Value.
    /// @return Whether the given field is a new field.
    /// @retval true If the given field didn't exist, and a new field has been added.
    /// @retval false If the given field already exists, and its value has been overwritten.
    /// @note When `hset` returns false, it does not mean that the method failed to set the field.
    ///       Instead, it means that the field already exists, and we've overwritten its value.
    ///       If `hset` fails, it will throw an exception of `Exception` type.
    /// @see https://github.com/sewenew/redis-plus-plus/issues/9
    /// @see https://redis.io/commands/hset
    bool hset(const StringView &key, const StringView &field, const StringView &val);

    /// @brief Set hash field to value.
    /// @param key Key where the hash is stored.
    /// @param item The field-value pair to be set.
    /// @return Whether the given field is a new field.
    /// @retval true If the given field didn't exist, and a new field has been added.
    /// @retval false If the given field already exists, and its value has been overwritten.
    /// @note When `hset` returns false, it does not mean that the method failed to set the field.
    ///       Instead, it means that the field already exists, and we've overwritten its value.
    ///       If `hset` fails, it will throw an exception of `Exception` type.
    /// @see https://github.com/sewenew/redis-plus-plus/issues/9
    /// @see https://redis.io/commands/hset
    bool hset(const StringView &key, const std::pair<StringView, StringView> &item);

    /// @brief Set multiple fields of the given hash.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::unordered_map<std::string, std::string> m = {{"f1", "v1"}, {"f2", "v2"}};
    /// redis.hset("hash", m.begin(), m.end());
    /// @endcode
    /// @param key Key where the hash is stored.
    /// @param first Iterator to the first field to be set.
    /// @param last Off-the-end iterator to the given range.
    /// @return Number of fields that have been added, i.e. fields that not existed before.
    /// @see https://redis.io/commands/hset
    template <typename Input>
    auto hset(const StringView &key, Input first, Input last)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value, long long>::type;

    /// @brief Set multiple fields of the given hash.
    ///
    /// Example:
    /// @code{.cpp}
    /// redis.hset("hash", {std::make_pair("f1", "v1"), std::make_pair("f2", "v2")});
    /// @endcode
    /// @param key Key where the hash is stored.
    /// @param il Initializer list of field-value pairs.
    /// @return Number of fields that have been added, i.e. fields that not existed before.
    /// @see https://redis.io/commands/hset
    template <typename T>
    long long hset(const StringView &key, std::initializer_list<T> il) {
        return hset(key, il.begin(), il.end());
    }

    /// @brief Set hash field to value, only if the given field does not exist.
    /// @param key Key where the hash is stored.
    /// @param field Field.
    /// @param val Value.
    /// @return Whether the field has been set.
    /// @retval true If the field has been set.
    /// @retval false If failed to set the field, i.e. the field already exists.
    /// @see https://redis.io/commands/hsetnx
    bool hsetnx(const StringView &key, const StringView &field, const StringView &val);

    /// @brief Set hash field to value, only if the given field does not exist.
    /// @param key Key where the hash is stored.
    /// @param item The field-value pair to be set.
    /// @return Whether the field has been set.
    /// @retval true If the field has been set.
    /// @retval false If failed to set the field, i.e. the field already exists.
    /// @see https://redis.io/commands/hsetnx
    bool hsetnx(const StringView &key, const std::pair<StringView, StringView> &item);

    /// @brief Get the length of the string stored at the given field.
    /// @param key Key where the hash is stored.
    /// @param field Field.
    /// @return Length of the string.
    /// @see https://redis.io/commands/hstrlen
    long long hstrlen(const StringView &key, const StringView &field);

    /// @brief Get values of all fields stored at the given hash.
    /// @param key Key where the hash is stored.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note It's always a bad idea to call `hvals` on a large hash, since it might block Redis.
    /// @see `Redis::hscan`
    /// @see https://redis.io/commands/hvals
    template <typename Output>
    void hvals(const StringView &key, Output output);

    // SET commands.

    /// @brief Add a member to the given set.
    /// @param key Key where the set is stored.
    /// @param member Member to be added.
    /// @return Whether the given member is a new member.
    /// @retval 1 The member did not exist before, and it has been added now.
    /// @retval 0 The member already exists before this operation.
    /// @see https://redis.io/commands/sadd
    long long sadd(const StringView &key, const StringView &member);

    /// @brief Add multiple members to the given set.
    /// @param key Key where the set is stored.
    /// @param first Iterator to the first member to be added.
    /// @param last Off-the-end iterator to the member range.
    /// @return Number of new members that have been added, i.e. members did not exist before.
    /// @see https://redis.io/commands/sadd
    template <typename Input>
    long long sadd(const StringView &key, Input first, Input last);

    /// @brief Add multiple members to the given set.
    /// @param key Key where the set is stored.
    /// @param il Initializer list of members to be added.
    /// @return Number of new members that have been added, i.e. members did not exist before.
    /// @see https://redis.io/commands/sadd
    template <typename T>
    long long sadd(const StringView &key, std::initializer_list<T> il) {
        return sadd(key, il.begin(), il.end());
    }

    /// @brief Get the number of members in the set.
    /// @param key Key where the set is stored.
    /// @return Number of members.
    /// @see https://redis.io/commands/scard
    long long scard(const StringView &key);

    /// @brief Get the difference between the first set and all successive sets.
    /// @param first Iterator to the first set.
    /// @param last Off-the-end iterator to the range.
    /// @param output Output iterator to the destination where the result is saved.
    /// @see https://redis.io/commands/sdiff
    // TODO: `void sdiff(const StringView &key, Input first, Input last, Output output)` is better.
    template <typename Input, typename Output>
    void sdiff(Input first, Input last, Output output);

    /// @brief Get the difference between the first set and all successive sets.
    /// @param il Initializer list of sets.
    /// @param output Output iterator to the destination where the result is saved.
    /// @see https://redis.io/commands/sdiff
    template <typename T, typename Output>
    void sdiff(std::initializer_list<T> il, Output output) {
        sdiff(il.begin(), il.end(), output);
    }

    /// @brief Copy set stored at `key` to `destination`.
    /// @param destination Key of the destination set.
    /// @param key Key of the source set.
    /// @return Number of members of the set.
    /// @see https://redis.io/commands/sdiffstore
    long long sdiffstore(const StringView &destination, const StringView &key);

    /// @brief Same as `sdiff`, except that it stores the result to another set.
    /// @param destination Key of the destination set.
    /// @param first Iterator to the first set.
    /// @param last Off-the-end iterator to set range.
    /// @return Number of members in the resulting set.
    /// @see https://redis.io/commands/sdiffstore
    template <typename Input>
    long long sdiffstore(const StringView &destination,
                            Input first,
                            Input last);

    /// @brief Same as `sdiff`, except that it stores the result to another set.
    /// @param destination Key of the destination set.
    /// @param il Initializer list of sets.
    /// @return Number of members in the resulting set.
    /// @see https://redis.io/commands/sdiffstore
    template <typename T>
    long long sdiffstore(const StringView &destination,
                            std::initializer_list<T> il) {
        return sdiffstore(destination, il.begin(), il.end());
    }

    /// @brief Get the intersection between the first set and all successive sets.
    /// @param first Iterator to the first set.
    /// @param last Off-the-end iterator to the range.
    /// @param output Output iterator to the destination where the result is saved.
    /// @see https://redis.io/commands/sinter
    // TODO: `void sinter(const StringView &key, Input first, Input last, Output output)` is better.
    template <typename Input, typename Output>
    void sinter(Input first, Input last, Output output);

    /// @brief Get the intersection between the first set and all successive sets.
    /// @param il Initializer list of sets.
    /// @param output Output iterator to the destination where the result is saved.
    /// @see https://redis.io/commands/sinter
    template <typename T, typename Output>
    void sinter(std::initializer_list<T> il, Output output) {
        sinter(il.begin(), il.end(), output);
    }

    /// @brief Copy set stored at `key` to `destination`.
    /// @param destination Key of the destination set.
    /// @param key Key of the source set.
    /// @return Number of members of the set.
    /// @see https://redis.io/commands/sinter
    long long sinterstore(const StringView &destination, const StringView &key);

    /// @brief Same as `sinter`, except that it stores the result to another set.
    /// @param destination Key of the destination set.
    /// @param first Iterator to the first set.
    /// @param last Off-the-end iterator to set range.
    /// @return Number of members in the resulting set.
    /// @see https://redis.io/commands/sinter
    template <typename Input>
    long long sinterstore(const StringView &destination,
                            Input first,
                            Input last);

    /// @brief Same as `sinter`, except that it stores the result to another set.
    /// @param destination Key of the destination set.
    /// @param il Initializer list of sets.
    /// @return Number of members in the resulting set.
    /// @see https://redis.io/commands/sinter
    template <typename T>
    long long sinterstore(const StringView &destination,
                            std::initializer_list<T> il) {
        return sinterstore(destination, il.begin(), il.end());
    }

    /// @brief Test if `member` exists in the set stored at key.
    /// @param key Key where the set is stored.
    /// @param member Member to be checked.
    /// @return Whether `member` exists in the set.
    /// @retval true If it exists in the set.
    /// @retval false If it does not exist in the set, or the given key does not exist.
    /// @see https://redis.io/commands/sismember
    bool sismember(const StringView &key, const StringView &member);

    /// @brief Get all members in the given set.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::unordered_set<std::string> members1;
    /// redis.smembers("set", std::inserter(members1, members1.begin()));
    /// std::vector<std::string> members2;
    /// redis.smembers("set", std::back_inserter(members2));
    /// @endcode
    /// @param key Key where the set is stored.
    /// @param output Iterator to the destination where the result is saved.
    /// @see https://redis.io/commands/smembers
    template <typename Output>
    void smembers(const StringView &key, Output output);

    /// @brief Move `member` from one set to another.
    /// @param source Key of the set in which the member currently exists.
    /// @param destination Key of the destination set.
    /// @return Whether the member has been moved.
    /// @retval true If the member has been moved.
    /// @retval false If `member` does not exist in `source`.
    /// @see https://redis.io/commands/smove
    bool smove(const StringView &source,
                const StringView &destination,
                const StringView &member);

    /// @brief Remove a random member from the set.
    /// @param key Key where the set is stored.
    /// @return The popped member.
    /// @note If the set is empty, `spop` returns `OptionalString{}` (`std::nullopt`).
    /// @see `Redis::srandmember`
    /// @see https://redis.io/commands/spop
    OptionalString spop(const StringView &key);

    /// @brief Remove multiple random members from the set.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> members;
    /// redis.spop("set", 10, std::back_inserter(members));
    /// @endcode
    /// @param key Key where the set is stored.
    /// @param count Number of members to be popped.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note The number of popped members might be less than `count`.
    /// @see `Redis::srandmember`
    /// @see https://redis.io/commands/spop
    template <typename Output>
    void spop(const StringView &key, long long count, Output output);

    /// @brief Get a random member of the given set.
    /// @param key Key where the set is stored.
    /// @return A random member.
    /// @note If the set is empty, `srandmember` returns `OptionalString{}` (`std::nullopt`).
    /// @note This method won't remove the member from the set.
    /// @see `Redis::spop`
    /// @see https://redis.io/commands/srandmember
    OptionalString srandmember(const StringView &key);

    /// @brief Get multiple random members of the given set.
    /// @param key Key where the set is stored.
    /// @param count Number of members to be returned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note This method won't remove members from the set.
    /// @see `Redis::spop`
    /// @see https://redis.io/commands/srandmember
    template <typename Output>
    void srandmember(const StringView &key, long long count, Output output);

    /// @brief Remove a member from set.
    /// @param key Key where the set is stored.
    /// @param member Member to be removed.
    /// @return Whether the member has been removed.
    /// @retval 1 If the given member exists, and has been removed.
    /// @retval 0 If the given member does not exist.
    /// @see https://redis.io/commands/srem
    long long srem(const StringView &key, const StringView &member);

    /// @brief Remove multiple members from set.
    /// @param key Key where the set is stored.
    /// @param first Iterator to the first member to be removed.
    /// @param last Off-the-end iterator to the range.
    /// @return Number of members that have been removed.
    /// @see https://redis.io/commands/srem
    template <typename Input>
    long long srem(const StringView &key, Input first, Input last);

    /// @brief Remove multiple members from set.
    /// @param key Key where the set is stored.
    /// @param il Initializer list of members to be removed.
    /// @return Number of members that have been removed.
    /// @see https://redis.io/commands/srem
    template <typename T>
    long long srem(const StringView &key, std::initializer_list<T> il) {
        return srem(key, il.begin(), il.end());
    }

    /// @brief Scan members of the set matching the given pattern.
    ///
    /// Example:
    /// @code{.cpp}
    /// auto cursor = 0LL;
    /// std::unordered_set<std::string> members;
    /// while (true) {
    ///     cursor = redis.sscan("set", cursor, "pattern:*",
    ///         10, std::inserter(members, members.begin()));
    ///     if (cursor == 0) {
    ///         break;
    ///     }
    /// }
    /// @endcode
    /// @param key Key where the set is stored.
    /// @param cursor Cursor.
    /// @param pattern Pattern of fields to be scanned.
    /// @param count A hint for how many fields to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/sscan
    template <typename Output>
    long long sscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count,
                    Output output);

    /// @brief Scan members of the set matching the given pattern.
    /// @param key Key where the set is stored.
    /// @param cursor Cursor.
    /// @param pattern Pattern of fields to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/sscan
    template <typename Output>
    long long sscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    Output output);

    /// @brief Scan all members of the given set.
    /// @param key Key where the set is stored.
    /// @param cursor Cursor.
    /// @param count A hint for how many fields to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/sscan
    template <typename Output>
    long long sscan(const StringView &key,
                    long long cursor,
                    long long count,
                    Output output);

    /// @brief Scan all members of the given set.
    /// @param key Key where the set is stored.
    /// @param cursor Cursor.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/sscan
    template <typename Output>
    long long sscan(const StringView &key,
                    long long cursor,
                    Output output);

    /// @brief Get the union between the first set and all successive sets.
    /// @param first Iterator to the first set.
    /// @param last Off-the-end iterator to the range.
    /// @param output Output iterator to the destination where the result is saved.
    /// @see https://redis.io/commands/sunion
    // TODO: `void sunion(const StringView &key, Input first, Input last, Output output)` is better.
    template <typename Input, typename Output>
    void sunion(Input first, Input last, Output output);

    /// @brief Get the union between the first set and all successive sets.
    /// @param il Initializer list of sets.
    /// @param output Output iterator to the destination where the result is saved.
    /// @see https://redis.io/commands/sunion
    template <typename T, typename Output>
    void sunion(std::initializer_list<T> il, Output output) {
        sunion(il.begin(), il.end(), output);
    }

    /// @brief Copy set stored at `key` to `destination`.
    /// @param destination Key of the destination set.
    /// @param key Key of the source set.
    /// @return Number of members of the set.
    /// @see https://redis.io/commands/sunionstore
    long long sunionstore(const StringView &destination, const StringView &key);

    /// @brief Same as `sunion`, except that it stores the result to another set.
    /// @param destination Key of the destination set.
    /// @param first Iterator to the first set.
    /// @param last Off-the-end iterator to set range.
    /// @return Number of members in the resulting set.
    /// @see https://redis.io/commands/sunionstore
    template <typename Input>
    long long sunionstore(const StringView &destination, Input first, Input last);

    /// @brief Same as `sunion`, except that it stores the result to another set.
    /// @param destination Key of the destination set.
    /// @param il Initializer list of sets.
    /// @return Number of members in the resulting set.
    /// @see https://redis.io/commands/sunionstore
    template <typename T>
    long long sunionstore(const StringView &destination, std::initializer_list<T> il) {
        return sunionstore(destination, il.begin(), il.end());
    }

    // SORTED SET commands.

    /// @brief Pop the member with highest score from sorted set in a blocking way.
    /// @param key Key where the sorted set is stored.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the highest score.
    /// @note If sorted set is empty and timeout reaches, `bzpopmax` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmax`
    /// @see https://redis.io/commands/bzpopmax
    auto bzpopmax(const StringView &key, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>>;

    /// @brief Pop the member with highest score from sorted set in a blocking way.
    /// @param key Key where the sorted set is stored.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the highest score.
    /// @note If sorted set is empty and timeout reaches, `bzpopmax` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmax`
    /// @see https://redis.io/commands/bzpopmax
    auto bzpopmax(const StringView &key,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>>;

    /// @brief Pop the member with highest score from multiple sorted set in a blocking way.
    /// @param first Iterator to the first key.
    /// @param last Off-the-end iterator to the key range.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the higest score.
    /// @note If all lists are empty and timeout reaches, `bzpopmax` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmax`
    /// @see https://redis.io/commands/bzpopmax
    template <typename Input>
    auto bzpopmax(Input first, Input last, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>>;

    /// @brief Pop the member with highest score from multiple sorted set in a blocking way.
    /// @param first Iterator to the first key.
    /// @param last Off-the-end iterator to the key range.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the higest score.
    /// @note If all lists are empty and timeout reaches, `bzpopmax` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmax`
    /// @see https://redis.io/commands/bzpopmax
    template <typename Input>
    auto bzpopmax(Input first,
                    Input last,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>>;

    /// @brief Pop the member with highest score from multiple sorted set in a blocking way.
    /// @param il Initializer list of sorted sets.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the higest score.
    /// @note If all lists are empty and timeout reaches, `bzpopmax` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmax`
    /// @see https://redis.io/commands/bzpopmax
    template <typename T>
    auto bzpopmax(std::initializer_list<T> il, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>> {
        return bzpopmax(il.begin(), il.end(), timeout);
    }

    /// @brief Pop the member with highest score from multiple sorted set in a blocking way.
    /// @param il Initializer list of sorted sets.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the higest score.
    /// @note If all lists are empty and timeout reaches, `bzpopmax` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmax`
    /// @see https://redis.io/commands/bzpopmax
    template <typename T>
    auto bzpopmax(std::initializer_list<T> il,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>> {
        return bzpopmax(il.begin(), il.end(), timeout);
    }

    /// @brief Pop the member with lowest score from sorted set in a blocking way.
    /// @param key Key where the sorted set is stored.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the lowest score.
    /// @note If sorted set is empty and timeout reaches, `bzpopmin` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmin`
    /// @see https://redis.io/commands/bzpopmin
    auto bzpopmin(const StringView &key, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>>;

    /// @brief Pop the member with lowest score from sorted set in a blocking way.
    /// @param key Key where the sorted set is stored.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the lowest score.
    /// @note If sorted set is empty and timeout reaches, `bzpopmin` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmin`
    /// @see https://redis.io/commands/bzpopmin
    auto bzpopmin(const StringView &key,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>>;

    /// @brief Pop the member with lowest score from multiple sorted set in a blocking way.
    /// @param first Iterator to the first key.
    /// @param last Off-the-end iterator to the key range.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the lowest score.
    /// @note If all lists are empty and timeout reaches, `bzpopmin` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmin`
    /// @see https://redis.io/commands/bzpopmin
    template <typename Input>
    auto bzpopmin(Input first, Input last, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>>;

    /// @brief Pop the member with lowest score from multiple sorted set in a blocking way.
    /// @param first Iterator to the first key.
    /// @param last Off-the-end iterator to the key range.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the lowest score.
    /// @note If all lists are empty and timeout reaches, `bzpopmin` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmin`
    /// @see https://redis.io/commands/bzpopmin
    template <typename Input>
    auto bzpopmin(Input first,
                    Input last,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>>;

    /// @brief Pop the member with lowest score from multiple sorted set in a blocking way.
    /// @param il Initializer list of sorted sets.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the lowest score.
    /// @note If all lists are empty and timeout reaches, `bzpopmin` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmin`
    /// @see https://redis.io/commands/bzpopmin
    template <typename T>
    auto bzpopmin(std::initializer_list<T> il, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>> {
        return bzpopmin(il.begin(), il.end(), timeout);
    }

    /// @brief Pop the member with lowest score from multiple sorted set in a blocking way.
    /// @param il Initializer list of sorted sets.
    /// @param timeout Timeout in seconds. 0 means block forever.
    /// @return Key-member-score tuple with the lowest score.
    /// @note If all lists are empty and timeout reaches, `bzpopmin` returns
    ///       `Optional<std::tuple<std::string, std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::zpopmin`
    /// @see https://redis.io/commands/bzpopmin
    template <typename T>
    auto bzpopmin(std::initializer_list<T> il,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>> {
        return bzpopmin(il.begin(), il.end(), timeout);
    }

    /// @brief Add or update a member with score to sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param member Member to be added.
    /// @param score Score of the member.
    /// @param type Options for zadd command:
    ///             - UpdateType::EXIST: Add the member only if it already exists.
    ///             - UpdateType::NOT_EXIST: Add the member only if it does not exist.
    ///             - UpdateType::ALWAYS: Always add the member no matter whether it exists.
    /// @param changed Whether change the return value from number of newly added member to
    ///                number of members changed (i.e. added and updated).
    /// @return Number of added members or number of added and updated members depends on `changed`.
    /// @note We don't support the INCR option, because in this case, the return value of zadd
    ///       command is NOT of type long long. However, you can always use the generic interface
    ///       to send zadd command with INCR option:
    ///       `auto score = redis.command<OptionalDouble>("ZADD", "key", "XX", "INCR", 10, "mem");`
    /// @see `UpdateType`
    /// @see https://redis.io/commands/zadd
    long long zadd(const StringView &key,
                    const StringView &member,
                    double score,
                    UpdateType type = UpdateType::ALWAYS,
                    bool changed = false);

    /// @brief Add or update multiple members with score to sorted set.
    ///
    /// Example:
    /// @code{.cpp}
    /// std::unordered_map<std::string, double> m = {{"m1", 1.2}, {"m2", 2.3}};
    /// redis.zadd("zset", m.begin(), m.end());
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param first Iterator to the first member-score pair.
    /// @param last Off-the-end iterator to the member-score pairs range.
    /// @param type Options for zadd command:
    ///             - UpdateType::EXIST: Add the member only if it already exists.
    ///             - UpdateType::NOT_EXIST: Add the member only if it does not exist.
    ///             - UpdateType::ALWAYS: Always add the member no matter whether it exists.
    /// @param changed Whether change the return value from number of newly added member to
    ///                number of members changed (i.e. added and updated).
    /// @return Number of added members or number of added and updated members depends on `changed`.
    /// @note We don't support the INCR option, because in this case, the return value of zadd
    ///       command is NOT of type long long. However, you can always use the generic interface
    ///       to send zadd command with INCR option:
    ///       `auto score = redis.command<OptionalDouble>("ZADD", "key", "XX", "INCR", 10, "mem");`
    /// @see `UpdateType`
    /// @see https://redis.io/commands/zadd
    template <typename Input>
    long long zadd(const StringView &key,
                    Input first,
                    Input last,
                    UpdateType type = UpdateType::ALWAYS,
                    bool changed = false);

    /// @brief Add or update multiple members with score to sorted set.
    ///
    /// Example:
    /// @code{.cpp}
    /// redis.zadd("zset", {std::make_pair("m1", 1.4), std::make_pair("m2", 2.3)});
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param first Iterator to the first member-score pair.
    /// @param last Off-the-end iterator to the member-score pairs range.
    /// @param type Options for zadd command:
    ///             - UpdateType::EXIST: Add the member only if it already exists.
    ///             - UpdateType::NOT_EXIST: Add the member only if it does not exist.
    ///             - UpdateType::ALWAYS: Always add the member no matter whether it exists.
    /// @param changed Whether change the return value from number of newly added member to
    ///                number of members changed (i.e. added and updated).
    /// @return Number of added members or number of added and updated members depends on `changed`.
    /// @note We don't support the INCR option, because in this case, the return value of zadd
    ///       command is NOT of type long long. However, you can always use the generic interface
    ///       to send zadd command with INCR option:
    ///       `auto score = redis.command<OptionalDouble>("ZADD", "key", "XX", "INCR", 10, "mem");`
    /// @see `UpdateType`
    /// @see https://redis.io/commands/zadd
    template <typename T>
    long long zadd(const StringView &key,
                    std::initializer_list<T> il,
                    UpdateType type = UpdateType::ALWAYS,
                    bool changed = false) {
        return zadd(key, il.begin(), il.end(), type, changed);
    }

    /// @brief Get the number of members in the sorted set.
    /// @param key Key where the sorted set is stored.
    /// @return Number of members in the sorted set.
    /// @see https://redis.io/commands/zcard
    long long zcard(const StringView &key);

    /// @brief Get the number of members with score between a min-max score range.
    ///
    /// Example:
    /// @code{.cpp}
    /// // Count members with score between (2.3, 5]
    /// redis.zcount("zset", BoundedInterval<double>(2.3, 5, BoundType::LEFT_OPEN));
    /// // Count members with score between [2.3, 5)
    /// redis.zcount("zset", BoundedInterval<double>(2.3, 5, BoundType::RIGHT_OPEN));
    /// // Count members with score between (2.3, 5)
    /// redis.zcount("zset", BoundedInterval<double>(2.3, 5, BoundType::OPEN));
    /// // Count members with score between [2.3, 5]
    /// redis.zcount("zset", BoundedInterval<double>(2.3, 5, BoundType::CLOSED));
    /// // Count members with score between [2.3, +inf)
    /// redis.zcount("zset", LeftBoundedInterval<double>(2.3, BoundType::RIGHT_OPEN));
    /// // Count members with score between (2.3, +inf)
    /// redis.zcount("zset", LeftBoundedInterval<double>(2.3, BoundType::OPEN));
    /// // Count members with score between (-inf, 5]
    /// redis.zcount("zset", RightBoundedInterval<double>(5, BoundType::LEFT_OPEN));
    /// // Count members with score between (-inf, 5)
    /// redis.zcount("zset", RightBoundedInterval<double>(5, BoundType::OPEN));
    /// // Count members with score between (-inf, +inf)
    /// redis.zcount("zset", UnboundedInterval<double>{});
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval The min-max score range.
    /// @return Number of members with score between a min-max score range.
    /// @see `BoundedInterval`
    /// @see `LeftBoundedInterval`
    /// @see `RightBoundedInterval`
    /// @see `UnboundedInterval`
    /// @see `BoundType`
    /// @see https://redis.io/commands/zcount
    // TODO: add a string version of Interval: zcount("key", "(2.3", "5").
    template <typename Interval>
    long long zcount(const StringView &key, const Interval &interval);

    /// @brief Increment the score of given member.
    /// @param key Key where the sorted set is stored.
    /// @param increment Increment.
    /// @param member Member.
    /// @return The score of the member after the operation.
    /// @see https://redis.io/commands/zincrby
    double zincrby(const StringView &key, double increment, const StringView &member);

    /// @brief Copy a sorted set to another one with the scores being multiplied by a factor.
    /// @param destination Key of the destination sorted set.
    /// @param key Key of the source sorted set.
    /// @param weight Weight to be multiplied to the score of each member.
    /// @return The number of members in the sorted set.
    /// @note  There's no aggregation type parameter for single key overload, since these 3 types
    ///         have the same effect.
    /// @see `Redis::zunionstore`
    /// @see https://redis.io/commands/zinterstore
    long long zinterstore(const StringView &destination, const StringView &key, double weight);

    /// @brief Get intersection of multiple sorted sets, and store the result to another one.
    ///
    /// Example:
    /// @code{.cpp}
    /// // Use the default weight, i.e. 1,
    /// // and use the sum of the all scores as the score of the result:
    /// std::vector<std::string> keys = {"k1", "k2", "k3"};
    /// redis.zinterstore("destination", keys.begin(), keys.end());
    /// // Each sorted set has a different weight,
    /// // and the score of the result is the min of all scores.
    /// std::vector<std::pair<std::string, double>> keys_with_weights = {{"k1", 1}, {"k2", 2}};
    /// redis.zinterstore("destination", keys_with_weights.begin(),
    ///     keys_with_weights.end(), Aggregation::MIN);
    /// // NOTE: `keys_with_weights` can also be of type `std::unordered_map<std::string, double>`.
    /// // However, it will be slower than std::vector<std::pair<std::string, double>>, since we use
    /// // `std::distance(first, last)` to calculate the *numkeys* parameter.
    /// @endcode
    /// @param destination Key of the destination sorted set.
    /// @param first Iterator to the first sorted set (might with weight).
    /// @param last Off-the-end iterator to the sorted set range.
    /// @param type How the scores are aggregated.
    ///             - Aggregation::SUM: Score of a member is the sum of all scores.
    ///             - Aggregation::MIN: Score of a member is the min of all scores.
    ///             - Aggregation::MAX: Score of a member is the max of all scores.
    /// @return The number of members in the resulting sorted set.
    /// @note The score of each member can be multiplied by a factor, i.e. weight. If `Input` is an
    ///       iterator to a container of `std::string`, we use the default weight, i.e. 1, and send
    ///       *ZINTERSTORE dest numkeys key [key ...] [AGGREGATE SUM|MIN|MAX]* command.
    ///        If `Input` is an iterator to a container of `std::pair<std::string, double>`,
    ///        i.e. key-weight pair, we send the command with the given weights:
    ///       *ZINTERSTORE dest numkeys key [key ...] [WEIGHTS weight [weight ...]] [AGGREGATE SUM|MIN|MAX]*.
    ///       See the *Example* part for examples on how to use this command.
    /// @see `Redis::zunionstore`
    /// @see https://redis.io/commands/zinterstore
    template <typename Input>
    long long zinterstore(const StringView &destination,
                            Input first,
                            Input last,
                            Aggregation type = Aggregation::SUM);

    /// @brief Get intersection of multiple sorted sets, and store the result to another one.
    ///
    /// Example:
    /// @code{.cpp}
    /// // Use the default weight, i.e. 1,
    /// // and use the sum of the all scores as the score of the result:
    /// redis.zinterstore("destination", {"k1", "k2"});
    /// // Each sorted set has a different weight,
    /// // and the score of the result is the min of all scores.
    /// redis.zinterstore("destination",
    ///     {std::make_pair("k1", 1), std::make_pair("k2", 2)}, Aggregation::MIN);
    /// @endcode
    /// @param destination Key of the destination sorted set.
    /// @param il Initializer list of sorted set.
    /// @param type How the scores are aggregated.
    ///             - Aggregation::SUM: Score of a member is the sum of all scores.
    ///             - Aggregation::MIN: Score of a member is the min of all scores.
    ///             - Aggregation::MAX: Score of a member is the max of all scores.
    /// @return The number of members in the resulting sorted set.
    /// @note The score of each member can be multiplied by a factor, i.e. weight. If `T` is
    ///       of type `std::string`, we use the default weight, i.e. 1, and send
    ///       *ZINTERSTORE dest numkeys key [key ...] [AGGREGATE SUM|MIN|MAX]* command.
    ///       If `T` is of type `std::pair<std::string, double>`, i.e. key-weight pair,
    ///       we send the command with the given weights:
    ///       *ZINTERSTORE dest numkeys key [key ...] [WEIGHTS weight [weight ...]] [AGGREGATE SUM|MIN|MAX]*.
    ///       See the *Example* part for examples on how to use this command.
    /// @see `Redis::zunionstore`
    /// @see https://redis.io/commands/zinterstore
    template <typename T>
    long long zinterstore(const StringView &destination,
                            std::initializer_list<T> il,
                            Aggregation type = Aggregation::SUM) {
        return zinterstore(destination, il.begin(), il.end(), type);
    }

    /// @brief Get the number of members between a min-max range in lexicographical order.
    ///
    /// Example:
    /// @code{.cpp}
    /// // Count members between (abc, abd]
    /// redis.zlexcount("zset", BoundedInterval<std::string>("abc", "abd", BoundType::LEFT_OPEN));
    /// // Count members between [abc, abd)
    /// redis.zlexcount("zset", BoundedInterval<std::string>("abc", "abd", BoundType::RIGHT_OPEN));
    /// // Count members between (abc, abd)
    /// redis.zlexcount("zset", BoundedInterval<std::string>("abc", "abd", BoundType::OPEN));
    /// // Count members between [abc, abd]
    /// redis.zlexcount("zset", BoundedInterval<std::string>("abc", "abd", BoundType::CLOSED));
    /// // Count members between [abc, +inf)
    /// redis.zlexcount("zset", LeftBoundedInterval<std::string>("abc", BoundType::RIGHT_OPEN));
    /// // Count members between (abc, +inf)
    /// redis.zlexcount("zset", LeftBoundedInterval<std::string>("abc", BoundType::OPEN));
    /// // Count members between (-inf, "abd"]
    /// redis.zlexcount("zset", RightBoundedInterval<std::string>("abd", BoundType::LEFT_OPEN));
    /// // Count members between (-inf, "abd")
    /// redis.zlexcount("zset", RightBoundedInterval<std::string>("abd", BoundType::OPEN));
    /// // Count members between (-inf, +inf)
    /// redis.zlexcount("zset", UnboundedInterval<std::string>{});
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval The min-max range in lexicographical order.
    /// @return Number of members between a min-max range in lexicographical order.
    /// @see `BoundedInterval`
    /// @see `LeftBoundedInterval`
    /// @see `RightBoundedInterval`
    /// @see `UnboundedInterval`
    /// @see `BoundType`
    /// @see https://redis.io/commands/zlexcount
    // TODO: add a string version of Interval: zlexcount("key", "(abc", "abd").
    template <typename Interval>
    long long zlexcount(const StringView &key, const Interval &interval);

    /// @brief Pop the member with highest score from sorted set.
    /// @param key Key where the sorted set is stored.
    /// @return Member-score pair with the highest score.
    /// @note If sorted set is empty `zpopmax` returns
    ///       `Optional<std::pair<std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::bzpopmax`
    /// @see https://redis.io/commands/zpopmax
    Optional<std::pair<std::string, double>> zpopmax(const StringView &key);

    /// @brief Pop multiple members with highest score from sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param count Number of members to be popped.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note The number of returned members might be less than `count`.
    /// @see `Redis::bzpopmax`
    /// @see https://redis.io/commands/zpopmax
    template <typename Output>
    void zpopmax(const StringView &key, long long count, Output output);

    /// @brief Pop the member with lowest score from sorted set.
    /// @param key Key where the sorted set is stored.
    /// @return Member-score pair with the lowest score.
    /// @note If sorted set is empty `zpopmin` returns
    ///       `Optional<std::pair<std::string, double>>{}` (`std::nullopt`).
    /// @see `Redis::bzpopmin`
    /// @see https://redis.io/commands/zpopmin
    Optional<std::pair<std::string, double>> zpopmin(const StringView &key);

    /// @brief Pop multiple members with lowest score from sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param count Number of members to be popped.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note The number of returned members might be less than `count`.
    /// @see `Redis::bzpopmin`
    /// @see https://redis.io/commands/zpopmin
    template <typename Output>
    void zpopmin(const StringView &key, long long count, Output output);

    /// @brief Get a range of members by rank (ordered from lowest to highest).
    ///
    /// Example:
    /// @code{.cpp}
    /// // send *ZRANGE* command without the *WITHSCORES* option:
    /// std::vector<std::string> result;
    /// redis.zrange("zset", 0, -1, std::back_inserter(result));
    /// // send command with *WITHSCORES* option:
    /// std::vector<std::pair<std::string, double>> with_score;
    /// redis.zrange("zset", 0, -1, std::back_inserter(with_score));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param start Start rank. Inclusive and can be negative.
    /// @param stop Stop rank. Inclusive and can be negative.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note This method can also return the score of each member. If `output` is an iterator
    ///       to a container of `std::string`, we send *ZRANGE key start stop* command.
    ///       If it's an iterator to a container of `std::pair<std::string, double>`,
    ///       we send *ZRANGE key start stop WITHSCORES* command. See the *Example* part on
    ///       how to use this method.
    /// @see `Redis::zrevrange`
    /// @see https://redis.io/commands/zrange
    template <typename Output>
    void zrange(const StringView &key, long long start, long long stop, Output output);

    /// @brief Get a range of members by lexicographical order (from lowest to highest).
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> result;
    /// // Get members between [abc, abd].
    /// redis.zrangebylex("zset", BoundedInterval<std::string>("abc", "abd", BoundType::CLOSED),
    ///     std::back_inserter(result));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by lexicographical order.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note See `Redis::zlexcount`'s *Example* part for how to set `interval` parameter.
    /// @see `Redis::zlexcount`
    /// @see `BoundedInterval`
    /// @see `LeftBoundedInterval`
    /// @see `RightBoundedInterval`
    /// @see `UnboundedInterval`
    /// @see `BoundType`
    /// @see `Redis::zrevrangebylex`
    /// @see https://redis.io/commands/zrangebylex
    template <typename Interval, typename Output>
    void zrangebylex(const StringView &key, const Interval &interval, Output output);

    /// @brief Get a range of members by lexicographical order (from lowest to highest).
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> result;
    /// // Limit the result to at most 5 members starting from 10.
    /// LimitOptions opts;
    /// opts.offset = 10;
    /// opts.count = 5;
    /// // Get members between [abc, abd].
    /// redis.zrangebylex("zset", BoundedInterval<std::string>("abc", "abd", BoundType::CLOSED),
    ///     opts, std::back_inserter(result));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by lexicographical order.
    /// @param opts Options to do pagination, i.e. *LIMIT offset count*.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note See `Redis::zlexcount`'s *Example* part for how to set `interval` parameter.
    /// @see `Redis::zlexcount`
    /// @see `BoundedInterval`
    /// @see `LeftBoundedInterval`
    /// @see `RightBoundedInterval`
    /// @see `UnboundedInterval`
    /// @see `BoundType`
    /// @see `LimitOptions`
    /// @see `Redis::zrevrangebylex`
    /// @see https://redis.io/commands/zrangebylex
    template <typename Interval, typename Output>
    void zrangebylex(const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        Output output);

    /// @brief Get a range of members by score (ordered from lowest to highest).
    ///
    /// Example:
    /// @code{.cpp}
    /// // Send *ZRANGEBYSCORE* command without the *WITHSCORES* option:
    /// std::vector<std::string> result;
    /// // Get members whose score between (3, 6).
    /// redis.zrangebyscore("zset", BoundedInterval<double>(3, 6, BoundType::OPEN),
    ///     std::back_inserter(result));
    /// // Send command with *WITHSCORES* option:
    /// std::vector<std::pair<std::string, double>> with_score;
    /// // Get members whose score between [3, +inf).
    /// redis.zrangebyscore("zset", LeftBoundedInterval<double>(3, BoundType::RIGHT_OPEN),
    ///     std::back_inserter(with_score));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by score.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note This method can also return the score of each member. If `output` is an iterator
    ///       to a container of `std::string`, we send *ZRANGEBYSCORE key min max* command.
    ///       If it's an iterator to a container of `std::pair<std::string, double>`,
    ///       we send *ZRANGEBYSCORE key min max WITHSCORES* command. See the *Example* part on
    ///       how to use this method.
    /// @note See `Redis::zcount`'s *Example* part for how to set the `interval` parameter.
    /// @see `Redis::zrevrangebyscore`
    /// @see https://redis.io/commands/zrangebyscore
    template <typename Interval, typename Output>
    void zrangebyscore(const StringView &key, const Interval &interval, Output output);

    /// @brief Get a range of members by score (ordered from lowest to highest).
    ///
    /// Example:
    /// @code{.cpp}
    /// // Send *ZRANGEBYSCORE* command without the *WITHSCORES* option:
    /// std::vector<std::string> result;
    /// // Only return at most 5 members starting from 10.
    /// LimitOptions opts;
    /// opts.offset = 10;
    /// opts.count = 5;
    /// // Get members whose score between (3, 6).
    /// redis.zrangebyscore("zset", BoundedInterval<double>(3, 6, BoundType::OPEN),
    ///     opts, std::back_inserter(result));
    /// // Send command with *WITHSCORES* option:
    /// std::vector<std::pair<std::string, double>> with_score;
    /// // Get members whose score between [3, +inf).
    /// redis.zrangebyscore("zset", LeftBoundedInterval<double>(3, BoundType::RIGHT_OPEN),
    ///     opts, std::back_inserter(with_score));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by score.
    /// @param opts Options to do pagination, i.e. *LIMIT offset count*.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note This method can also return the score of each member. If `output` is an iterator
    ///       to a container of `std::string`, we send *ZRANGEBYSCORE key min max* command.
    ///       If it's an iterator to a container of `std::pair<std::string, double>`,
    ///       we send *ZRANGEBYSCORE key min max WITHSCORES* command. See the *Example* part on
    ///       how to use this method.
    /// @note See `Redis::zcount`'s *Example* part for how to set the `interval` parameter.
    /// @see `Redis::zrevrangebyscore`
    /// @see https://redis.io/commands/zrangebyscore
    template <typename Interval, typename Output>
    void zrangebyscore(const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        Output output);

    /// @brief Get the rank (from low to high) of the given member in the sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param member Member.
    /// @return The rank of the given member.
    /// @note If the member does not exist, `zrank` returns `OptionalLongLong{}` (`std::nullopt`).
    /// @see https://redis.io/commands/zrank
    OptionalLongLong zrank(const StringView &key, const StringView &member);

    /// @brief Remove the given member from sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param member Member to be removed.
    /// @return Whether the member has been removed.
    /// @retval 1 If the member exists, and has been removed.
    /// @retval 0 If the member does not exist.
    /// @see https://redis.io/commands/zrem
    long long zrem(const StringView &key, const StringView &member);

    /// @brief Remove multiple members from sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param first Iterator to the first member.
    /// @param last Off-the-end iterator to the given range.
    /// @return Number of members that have been removed.
    /// @see https://redis.io/commands/zrem
    template <typename Input>
    long long zrem(const StringView &key, Input first, Input last);

    /// @brief Remove multiple members from sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param il Initializer list of members to be removed.
    /// @return Number of members that have been removed.
    /// @see https://redis.io/commands/zrem
    template <typename T>
    long long zrem(const StringView &key, std::initializer_list<T> il) {
        return zrem(key, il.begin(), il.end());
    }

    /// @brief Remove members in the given range of lexicographical order.
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by lexicographical order.
    /// @note See `Redis::zlexcount`'s *Example* part for how to set `interval` parameter.
    /// @return Number of members removed.
    /// @see `Redis::zlexcount`
    /// @see `BoundedInterval`
    /// @see `LeftBoundedInterval`
    /// @see `RightBoundedInterval`
    /// @see `UnboundedInterval`
    /// @see `BoundType`
    /// @see https://redis.io/commands/zremrangebylex
    template <typename Interval>
    long long zremrangebylex(const StringView &key, const Interval &interval);

    /// @brief Remove members in the given range ordered by rank.
    /// @param key Key where the sorted set is stored.
    /// @param start Start rank.
    /// @param stop Stop rank.
    /// @return Number of members removed.
    /// @see https://redis.io/commands/zremrangebyrank
    long long zremrangebyrank(const StringView &key, long long start, long long stop);

    /// @brief Remove members in the given range ordered by score.
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by score.
    /// @return Number of members removed.
    /// @note See `Redis::zcount`'s *Example* part for how to set the `interval` parameter.
    /// @see https://redis.io/commands/zremrangebyscore
    template <typename Interval>
    long long zremrangebyscore(const StringView &key, const Interval &interval);

    /// @brief Get a range of members by rank (ordered from highest to lowest).
    ///
    /// Example:
    /// @code{.cpp}
    /// // send *ZREVRANGE* command without the *WITHSCORES* option:
    /// std::vector<std::string> result;
    /// redis.zrevrange("key", 0, -1, std::back_inserter(result));
    /// // send command with *WITHSCORES* option:
    /// std::vector<std::pair<std::string, double>> with_score;
    /// redis.zrevrange("key", 0, -1, std::back_inserter(with_score));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param start Start rank. Inclusive and can be negative.
    /// @param stop Stop rank. Inclusive and can be negative.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note This method can also return the score of each member. If `output` is an iterator
    ///       to a container of `std::string`, we send *ZREVRANGE key start stop* command.
    ///       If it's an iterator to a container of `std::pair<std::string, double>`,
    ///       we send *ZREVRANGE key start stop WITHSCORES* command. See the *Example* part on
    ///       how to use this method.
    /// @see `Redis::zrange`
    /// @see https://redis.io/commands/zrevrange
    template <typename Output>
    void zrevrange(const StringView &key, long long start, long long stop, Output output);

    /// @brief Get a range of members by lexicographical order (from highest to lowest).
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> result;
    /// // Get members between [abc, abd] in reverse order.
    /// redis.zrevrangebylex("zset", BoundedInterval<std::string>("abc", "abd", BoundType::CLOSED),
    ///     std::back_inserter(result));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by lexicographical order.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note See `Redis::zlexcount`'s *Example* part for how to set `interval` parameter.
    /// @see `Redis::zlexcount`
    /// @see `BoundedInterval`
    /// @see `LeftBoundedInterval`
    /// @see `RightBoundedInterval`
    /// @see `UnboundedInterval`
    /// @see `BoundType`
    /// @see `Redis::zrangebylex`
    /// @see https://redis.io/commands/zrevrangebylex
    template <typename Interval, typename Output>
    void zrevrangebylex(const StringView &key, const Interval &interval, Output output);

    /// @brief Get a range of members by lexicographical order (from highest to lowest).
    ///
    /// Example:
    /// @code{.cpp}
    /// std::vector<std::string> result;
    /// // Limit the result to at most 5 members starting from 10.
    /// LimitOptions opts;
    /// opts.offset = 10;
    /// opts.count = 5;
    /// // Get members between [abc, abd] in reverse order.
    /// redis.zrevrangebylex("zset", BoundedInterval<std::string>("abc", "abd", BoundType::CLOSED),
    ///     opts, std::back_inserter(result));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by lexicographical order.
    /// @param opts Options to do pagination, i.e. *LIMIT offset count*.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note See `Redis::zlexcount`'s *Example* part for how to set `interval` parameter.
    /// @see `Redis::zlexcount`
    /// @see `BoundedInterval`
    /// @see `LeftBoundedInterval`
    /// @see `RightBoundedInterval`
    /// @see `UnboundedInterval`
    /// @see `BoundType`
    /// @see `LimitOptions`
    /// @see `Redis::zrangebylex`
    /// @see https://redis.io/commands/zrevrangebylex
    template <typename Interval, typename Output>
    void zrevrangebylex(const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        Output output);

    /// @brief Get a range of members by score (ordered from highest to lowest).
    ///
    /// Example:
    /// @code{.cpp}
    /// // Send *ZREVRANGEBYSCORE* command without the *WITHSCORES* option:
    /// std::vector<std::string> result;
    /// // Get members whose score between (3, 6) in reverse order.
    /// redis.zrevrangebyscore("zset", BoundedInterval<double>(3, 6, BoundType::OPEN),
    ///     std::back_inserter(result));
    /// // Send command with *WITHSCORES* option:
    /// std::vector<std::pair<std::string, double>> with_score;
    /// // Get members whose score between [3, +inf) in reverse order.
    /// redis.zrevrangebyscore("zset", LeftBoundedInterval<double>(3, BoundType::RIGHT_OPEN),
    ///     std::back_inserter(with_score));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by score.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note This method can also return the score of each member. If `output` is an iterator
    ///       to a container of `std::string`, we send *ZREVRANGEBYSCORE key min max* command.
    ///       If it's an iterator to a container of `std::pair<std::string, double>`,
    ///       we send *ZREVRANGEBYSCORE key min max WITHSCORES* command. See the *Example* part on
    ///       how to use this method.
    /// @note See `Redis::zcount`'s *Example* part for how to set the `interval` parameter.
    /// @see `Redis::zrangebyscore`
    /// @see https://redis.io/commands/zrevrangebyscore
    template <typename Interval, typename Output>
    void zrevrangebyscore(const StringView &key, const Interval &interval, Output output);

    /// @brief Get a range of members by score (ordered from highest to lowest).
    ///
    /// Example:
    /// @code{.cpp}
    /// // Send *ZREVRANGEBYSCORE* command without the *WITHSCORES* option:
    /// std::vector<std::string> result;
    /// // Only return at most 5 members starting from 10.
    /// LimitOptions opts;
    /// opts.offset = 10;
    /// opts.count = 5;
    /// // Get members whose score between (3, 6) in reverse order.
    /// redis.zrevrangebyscore("zset", BoundedInterval<double>(3, 6, BoundType::OPEN),
    ///     opts, std::back_inserter(result));
    /// // Send command with *WITHSCORES* option:
    /// std::vector<std::pair<std::string, double>> with_score;
    /// // Get members whose score between [3, +inf) in reverse order.
    /// redis.zrevrangebyscore("zset", LeftBoundedInterval<double>(3, BoundType::RIGHT_OPEN),
    ///     opts, std::back_inserter(with_score));
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param interval the min-max range by score.
    /// @param opts Options to do pagination, i.e. *LIMIT offset count*.
    /// @param output Output iterator to the destination where the result is saved.
    /// @note This method can also return the score of each member. If `output` is an iterator
    ///       to a container of `std::string`, we send *ZREVRANGEBYSCORE key min max* command.
    ///       If it's an iterator to a container of `std::pair<std::string, double>`,
    ///       we send *ZREVRANGEBYSCORE key min max WITHSCORES* command. See the *Example* part on
    ///       how to use this method.
    /// @note See `Redis::zcount`'s *Example* part for how to set the `interval` parameter.
    /// @see `Redis::zrangebyscore`
    /// @see https://redis.io/commands/zrevrangebyscore
    template <typename Interval, typename Output>
    void zrevrangebyscore(const StringView &key,
                            const Interval &interval,
                            const LimitOptions &opts,
                            Output output);

    /// @brief Get the rank (from high to low) of the given member in the sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param member Member.
    /// @return The rank of the given member.
    /// @note If the member does not exist, `zrevrank` returns `OptionalLongLong{}` (`std::nullopt`).
    /// @see https://redis.io/commands/zrevrank
    OptionalLongLong zrevrank(const StringView &key, const StringView &member);

    /// @brief Scan members of the given sorted set matching the given pattern.
    ///
    /// Example:
    /// @code{.cpp}
    /// auto cursor = 0LL;
    /// std::vector<std::pair<std::string, double>> members;
    /// while (true) {
    ///     cursor = redis.zscan("zset", cursor, "pattern:*",
    ///         10, std::back_inserter(members));
    ///     if (cursor == 0) {
    ///         break;
    ///     }
    /// }
    /// @endcode
    /// @param key Key where the sorted set is stored.
    /// @param cursor Cursor.
    /// @param pattern Pattern of members to be scanned.
    /// @param count A hint for how many members to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/zscan
    template <typename Output>
    long long zscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count,
                    Output output);

    /// @brief Scan members of the given sorted set matching the given pattern.
    /// @param key Key where the sorted set is stored.
    /// @param cursor Cursor.
    /// @param pattern Pattern of members to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/zscan
    template <typename Output>
    long long zscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    Output output);

    /// @brief Scan all members of the given sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param cursor Cursor.
    /// @param count A hint for how many members to be scanned.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/zscan
    template <typename Output>
    long long zscan(const StringView &key,
                    long long cursor,
                    long long count,
                    Output output);

    /// @brief Scan all members of the given sorted set.
    /// @param key Key where the sorted set is stored.
    /// @param cursor Cursor.
    /// @param output Output iterator to the destination where the result is saved.
    /// @return The cursor to be used for the next scan operation.
    /// @see https://redis.io/commands/zscan
    template <typename Output>
    long long zscan(const StringView &key,
                    long long cursor,
                    Output output);

    /// @brief Get the score of the given member.
    /// @param key Key where the sorted set is stored.
    /// @param member Member.
    /// @return The score of the member.
    /// @note If member does not exist, `zscore` returns `OptionalDouble{}` (`std::nullopt`).
    /// @see https://redis.io/commands/zscore
    OptionalDouble zscore(const StringView &key, const StringView &member);

    /// @brief Copy a sorted set to another one with the scores being multiplied by a factor.
    /// @param destination Key of the destination sorted set.
    /// @param key Key of the source sorted set.
    /// @param weight Weight to be multiplied to the score of each member.
    /// @return The number of members in the sorted set.
    /// @note  There's no aggregation type parameter for single key overload, since these 3 types
    ///         have the same effect.
    /// @see `Redis::zinterstore`
    /// @see https://redis.io/commands/zinterstore
    long long zunionstore(const StringView &destination, const StringView &key, double weight);

    /// @brief Get union of multiple sorted sets, and store the result to another one.
    ///
    /// Example:
    /// @code{.cpp}
    /// // Use the default weight, i.e. 1,
    /// // and use the sum of the all scores as the score of the result:
    /// std::vector<std::string> keys = {"k1", "k2", "k3"};
    /// redis.zunionstore("destination", keys.begin(), keys.end());
    /// // Each sorted set has a different weight,
    /// // and the score of the result is the min of all scores.
    /// std::vector<std::pair<std::string, double>> keys_with_weights = {{"k1", 1}, {"k2", 2}};
    /// redis.zunionstore("destination", keys_with_weights.begin(),
    ///     keys_with_weights.end(), Aggregation::MIN);
    /// // NOTE: `keys_with_weights` can also be of type `std::unordered_map<std::string, double>`.
    /// // However, it will be slower than std::vector<std::pair<std::string, double>>, since we use
    /// // `std::distance(first, last)` to calculate the *numkeys* parameter.
    /// @endcode
    /// @param destination Key of the destination sorted set.
    /// @param first Iterator to the first sorted set (might with weight).
    /// @param last Off-the-end iterator to the sorted set range.
    /// @param type How the scores are aggregated.
    ///             - Aggregation::SUM: Score of a member is the sum of all scores.
    ///             - Aggregation::MIN: Score of a member is the min of all scores.
    ///             - Aggregation::MAX: Score of a member is the max of all scores.
    /// @return The number of members in the resulting sorted set.
    /// @note The score of each member can be multiplied by a factor, i.e. weight. If `Input` is an
    ///       iterator to a container of `std::string`, we use the default weight, i.e. 1, and send
    ///       *ZUNIONSTORE dest numkeys key [key ...] [AGGREGATE SUM|MIN|MAX]* command.
    ///        If `Input` is an iterator to a container of `std::pair<std::string, double>`,
    ///        i.e. key-weight pair, we send the command with the given weights:
    ///       *ZUNIONSTORE dest numkeys key [key ...] [WEIGHTS weight [weight ...]] [AGGREGATE SUM|MIN|MAX]*.
    ///       See the *Example* part for examples on how to use this command.
    /// @see `Redis::zinterstore`
    /// @see https://redis.io/commands/zunionstore
    template <typename Input>
    long long zunionstore(const StringView &destination,
                            Input first,
                            Input last,
                            Aggregation type = Aggregation::SUM);

    /// @brief Get union of multiple sorted sets, and store the result to another one.
    ///
    /// Example:
    /// @code{.cpp}
    /// // Use the default weight, i.e. 1,
    /// // and use the sum of the all scores as the score of the result:
    /// redis.zunionstore("destination", {"k1", "k2"});
    /// // Each sorted set has a different weight,
    /// // and the score of the result is the min of all scores.
    /// redis.zunionstore("destination",
    ///     {std::make_pair("k1", 1), std::make_pair("k2", 2)}, Aggregation::MIN);
    /// @endcode
    /// @param destination Key of the destination sorted set.
    /// @param il Initializer list of sorted set.
    /// @param type How the scores are aggregated.
    ///             - Aggregation::SUM: Score of a member is the sum of all scores.
    ///             - Aggregation::MIN: Score of a member is the min of all scores.
    ///             - Aggregation::MAX: Score of a member is the max of all scores.
    /// @return The number of members in the resulting sorted set.
    /// @note The score of each member can be multiplied by a factor, i.e. weight. If `T` is
    ///       of type `std::string`, we use the default weight, i.e. 1, and send
    ///       *ZUNIONSTORE dest numkeys key [key ...] [AGGREGATE SUM|MIN|MAX]* command.
    ///       If `T` is of type `std::pair<std::string, double>`, i.e. key-weight pair,
    ///       we send the command with the given weights:
    ///       *ZUNIONSTORE dest numkeys key [key ...] [WEIGHTS weight [weight ...]] [AGGREGATE SUM|MIN|MAX]*.
    ///       See the *Example* part for examples on how to use this command.
    /// @see `Redis::zinterstore`
    /// @see https://redis.io/commands/zunionstore
    template <typename T>
    long long zunionstore(const StringView &destination,
                            std::initializer_list<T> il,
                            Aggregation type = Aggregation::SUM) {
        return zunionstore(destination, il.begin(), il.end(), type);
    }

    // HYPERLOGLOG commands.

    /// @brief Add the given element to a hyperloglog.
    /// @param key Key of the hyperloglog.
    /// @param element Element to be added.
    /// @return Whether any of hyperloglog's internal register has been altered.
    /// @retval true If at least one internal register has been altered.
    /// @retval false If none of internal registers has been altered.
    /// @note When `pfadd` returns false, it does not mean that this method failed to add
    ///       an element to the hyperloglog. Instead it means that the internal registers
    ///       were not altered. If `pfadd` fails, it will throw an exception of `Exception` type.
    /// @see https://redis.io/commands/pfadd
    bool pfadd(const StringView &key, const StringView &element);

    /// @brief Add the given elements to a hyperloglog.
    /// @param key Key of the hyperloglog.
    /// @param first Iterator to the first element.
    /// @param last Off-the-end iterator to the given range.
    /// @return Whether any of hyperloglog's internal register has been altered.
    /// @retval true If at least one internal register has been altered.
    /// @retval false If none of internal registers has been altered.
    /// @note When `pfadd` returns false, it does not mean that this method failed to add
    ///       an element to the hyperloglog. Instead it means that the internal registers
    ///       were not altered. If `pfadd` fails, it will throw an exception of `Exception` type.
    /// @see https://redis.io/commands/pfadd
    template <typename Input>
    bool pfadd(const StringView &key, Input first, Input last);

    /// @brief Add the given elements to a hyperloglog.
    /// @param key Key of the hyperloglog.
    /// @param il Initializer list of elements to be added.
    /// @return Whether any of hyperloglog's internal register has been altered.
    /// @retval true If at least one internal register has been altered.
    /// @retval false If none of internal registers has been altered.
    /// @note When `pfadd` returns false, it does not mean that this method failed to add
    ///       an element to the hyperloglog. Instead it means that the internal registers
    ///       were not altered. If `pfadd` fails, it will throw an exception of `Exception` type.
    /// @see https://redis.io/commands/pfadd
    template <typename T>
    bool pfadd(const StringView &key, std::initializer_list<T> il) {
        return pfadd(key, il.begin(), il.end());
    }

    long long pfcount(const StringView &key);

    template <typename Input>
    long long pfcount(Input first, Input last);

    template <typename T>
    long long pfcount(std::initializer_list<T> il) {
        return pfcount(il.begin(), il.end());
    }

    void pfmerge(const StringView &destination, const StringView &key);

    template <typename Input>
    void pfmerge(const StringView &destination, Input first, Input last);

    template <typename T>
    void pfmerge(const StringView &destination, std::initializer_list<T> il) {
        pfmerge(destination, il.begin(), il.end());
    }

    // GEO commands.

    long long geoadd(const StringView &key,
                        const std::tuple<StringView, double, double> &member);

    template <typename Input>
    long long geoadd(const StringView &key,
                        Input first,
                        Input last);

    template <typename T>
    long long geoadd(const StringView &key,
                        std::initializer_list<T> il) {
        return geoadd(key, il.begin(), il.end());
    }

    OptionalDouble geodist(const StringView &key,
                            const StringView &member1,
                            const StringView &member2,
                            GeoUnit unit = GeoUnit::M);

    OptionalString geohash(const StringView &key, const StringView &member);

    template <typename Input, typename Output>
    void geohash(const StringView &key, Input first, Input last, Output output);

    template <typename T, typename Output>
    void geohash(const StringView &key, std::initializer_list<T> il, Output output) {
        geohash(key, il.begin(), il.end(), output);
    }

    Optional<std::pair<double, double>> geopos(const StringView &key, const StringView &member);

    template <typename Input, typename Output>
    void geopos(const StringView &key, Input first, Input last, Output output);

    template <typename T, typename Output>
    void geopos(const StringView &key, std::initializer_list<T> il, Output output) {
        geopos(key, il.begin(), il.end(), output);
    }

    // TODO:
    // 1. since we have different overloads for georadius and georadius-store,
    //    we might use the GEORADIUS_RO command in the future.
    // 2. there're too many parameters for this method, we might refactor it.

    /// @brief Get members in geo range, i.e. a circle, and store them in a sorted set.
    /// @param key Key of the GEO set.
    /// @param loc Location encoded with <longitude, latitude> pair.
    /// @param radius Radius of the range.
    /// @param unit Radius unit.
    /// @param destination Key of the destination sorted set.
    /// @param store_dist Whether store distance info instead of geo info to destination.
    /// @param count Limit the first N members.
    /// @return Number of members stored in destination.
    /// @note Before Redis 6.2.6, if key does not exist, returns `OptionalLongLong{}` (`std::nullopt`).
    ///       Since Redis 6.2.6, if key does not exist, returns 0.
    /// @see `GeoUnit`
    /// @see `Redis::georadiusbymember`
    /// @see https://redis.io/commands/georadius
    OptionalLongLong georadius(const StringView &key,
                                const std::pair<double, double> &loc,
                                double radius,
                                GeoUnit unit,
                                const StringView &destination,
                                bool store_dist,
                                long long count);

    // If *output* is an iterator of a container of string, we send *GEORADIUS* command
    // without any options and only get the members in the specified geo range.
    // If *output* is an iterator of a container of a tuple, the type of the tuple decides
    // options we send with the *GEORADIUS* command. If the tuple has an element of type
    // double, we send the *WITHDIST* option. If it has an element of type string, we send
    // the *WITHHASH* option. If it has an element of type pair<double, double>, we send
    // the *WITHCOORD* option. For example:
    //
    // The following code only gets the members in range, i.e. without any option.
    //
    // vector<string> members;
    // redis.georadius("key", make_pair(10.1, 10.2), 10, GeoUnit::KM, 10, true,
    //                  back_inserter(members))
    //
    // The following code sends the command with *WITHDIST* option.
    //
    // vector<tuple<string, double>> with_dist;
    // redis.georadius("key", make_pair(10.1, 10.2), 10, GeoUnit::KM, 10, true,
    //                  back_inserter(with_dist))
    //
    // The following code sends the command with *WITHDIST* and *WITHHASH* options.
    //
    // vector<tuple<string, double, string>> with_dist_hash;
    // redis.georadius("key", make_pair(10.1, 10.2), 10, GeoUnit::KM, 10, true,
    //                  back_inserter(with_dist_hash))
    //
    // The following code sends the command with *WITHDIST*, *WITHCOORD* and *WITHHASH* options.
    //
    // vector<tuple<string, double, pair<double, double>, string>> with_dist_coord_hash;
    // redis.georadius("key", make_pair(10.1, 10.2), 10, GeoUnit::KM, 10, true,
    //                  back_inserter(with_dist_coord_hash))
    //
    // This also applies to *GEORADIUSBYMEMBER*.
    template <typename Output>
    void georadius(const StringView &key,
                    const std::pair<double, double> &loc,
                    double radius,
                    GeoUnit unit,
                    long long count,
                    bool asc,
                    Output output);

    /// @brief Get members in geo range, i.e. a circle, and store them in a sorted set.
    /// @param key Key of the GEO set.
    /// @param member Member which is the center of the circle.
    /// @param radius Radius of the range.
    /// @param unit Radius unit.
    /// @param destination Key of the destination sorted set.
    /// @param store_dist Whether store distance info instead of geo info to destination.
    /// @param count Limit the first N members.
    /// @return Number of members stored in destination.
    /// @note Before Redis 6.2.6, if key does not exist, returns `OptionalLongLong{}` (`std::nullopt`).
    ///       Since Redis 6.2.6, if key does not exist, returns 0.
    /// @note If member does not exist, throw an `ReplyError`.
    /// @see `GeoUnit`
    /// @see `Redis::georadius`
    /// @see https://redis.io/commands/georadiusbymember
    OptionalLongLong georadiusbymember(const StringView &key,
                                        const StringView &member,
                                        double radius,
                                        GeoUnit unit,
                                        const StringView &destination,
                                        bool store_dist,
                                        long long count);

    // See comments on *GEORADIUS*.
    template <typename Output>
    void georadiusbymember(const StringView &key,
                            const StringView &member,
                            double radius,
                            GeoUnit unit,
                            long long count,
                            bool asc,
                            Output output);

    // SCRIPTING commands.

    template <typename Result, typename Keys, typename Args>
    Result eval(const StringView &script,
                Keys keys_first,
                Keys keys_last,
                Args args_first,
                Args args_last);

    template <typename Result>
    Result eval(const StringView &script,
                std::initializer_list<StringView> keys,
                std::initializer_list<StringView> args);

    template <typename Keys, typename Args, typename Output>
    void eval(const StringView &script,
                Keys keys_first,
                Keys keys_last,
                Args args_first,
                Args args_last,
                Output output);

    template <typename Output>
    void eval(const StringView &script,
                std::initializer_list<StringView> keys,
                std::initializer_list<StringView> args,
                Output output);

    template <typename Result, typename Keys, typename Args>
    Result evalsha(const StringView &script,
                    Keys keys_first,
                    Keys keys_last,
                    Args args_first,
                    Args args_last);

    template <typename Result>
    Result evalsha(const StringView &script,
                    std::initializer_list<StringView> keys,
                    std::initializer_list<StringView> args);

    template <typename Keys, typename Args, typename Output>
    void evalsha(const StringView &script,
                    Keys keys_first,
                    Keys keys_last,
                    Args args_first,
                    Args args_last,
                    Output output);

    template <typename Output>
    void evalsha(const StringView &script,
                    std::initializer_list<StringView> keys,
                    std::initializer_list<StringView> args,
                    Output output);

    /// @brief Check if the given script exists.
    /// @param sha1 SHA1 digest of the script.
    /// @return Whether the script exists.
    /// @retval true If the script exists.
    /// @retval false If the script does not exist.
    /// @see https://redis.io/commands/script-exists
    bool script_exists(const StringView &sha1);

    template <typename Input, typename Output>
    void script_exists(Input first, Input last, Output output);

    template <typename T, typename Output>
    void script_exists(std::initializer_list<T> il, Output output) {
        script_exists(il.begin(), il.end(), output);
    }

    void script_flush();

    void script_kill();

    std::string script_load(const StringView &script);

    // PUBSUB commands.

    long long publish(const StringView &channel, const StringView &message);

    // Transaction commands.
    void watch(const StringView &key);

    template <typename Input>
    void watch(Input first, Input last);

    template <typename T>
    void watch(std::initializer_list<T> il) {
        watch(il.begin(), il.end());
    }

    void unwatch();

    // Stream commands.

    long long xack(const StringView &key, const StringView &group, const StringView &id);

    template <typename Input>
    long long xack(const StringView &key, const StringView &group, Input first, Input last);

    template <typename T>
    long long xack(const StringView &key, const StringView &group, std::initializer_list<T> il) {
        return xack(key, group, il.begin(), il.end());
    }

    template <typename Input>
    std::string xadd(const StringView &key, const StringView &id, Input first, Input last);

    template <typename T>
    std::string xadd(const StringView &key, const StringView &id, std::initializer_list<T> il) {
        return xadd(key, id, il.begin(), il.end());
    }

    template <typename Input>
    std::string xadd(const StringView &key,
                        const StringView &id,
                        Input first,
                        Input last,
                        long long count,
                        bool approx = true);

    template <typename T>
    std::string xadd(const StringView &key,
                        const StringView &id,
                        std::initializer_list<T> il,
                        long long count,
                        bool approx = true) {
        return xadd(key, id, il.begin(), il.end(), count, approx);
    }

    template <typename Output>
    void xclaim(const StringView &key,
                const StringView &group,
                const StringView &consumer,
                const std::chrono::milliseconds &min_idle_time,
                const StringView &id,
                Output output);

    template <typename Input, typename Output>
    void xclaim(const StringView &key,
                const StringView &group,
                const StringView &consumer,
                const std::chrono::milliseconds &min_idle_time,
                Input first,
                Input last,
                Output output);

    template <typename T, typename Output>
    void xclaim(const StringView &key,
                const StringView &group,
                const StringView &consumer,
                const std::chrono::milliseconds &min_idle_time,
                std::initializer_list<T> il,
                Output output) {
        xclaim(key, group, consumer, min_idle_time, il.begin(), il.end(), output);
    }

    long long xdel(const StringView &key, const StringView &id);

    template <typename Input>
    long long xdel(const StringView &key, Input first, Input last);

    template <typename T>
    long long xdel(const StringView &key, std::initializer_list<T> il) {
        return xdel(key, il.begin(), il.end());
    }

    void xgroup_create(const StringView &key,
                        const StringView &group,
                        const StringView &id,
                        bool mkstream = false);

    void xgroup_setid(const StringView &key, const StringView &group, const StringView &id);

    long long xgroup_destroy(const StringView &key, const StringView &group);

    long long xgroup_delconsumer(const StringView &key,
                                    const StringView &group,
                                    const StringView &consumer);

    long long xlen(const StringView &key);

    template <typename Output>
    auto xpending(const StringView &key, const StringView &group, Output output)
        -> std::tuple<long long, OptionalString, OptionalString>;

    template <typename Output>
    void xpending(const StringView &key,
                    const StringView &group,
                    const StringView &start,
                    const StringView &end,
                    long long count,
                    Output output);

    template <typename Output>
    void xpending(const StringView &key,
                    const StringView &group,
                    const StringView &start,
                    const StringView &end,
                    long long count,
                    const StringView &consumer,
                    Output output);

    template <typename Output>
    void xrange(const StringView &key,
                const StringView &start,
                const StringView &end,
                Output output);

    template <typename Output>
    void xrange(const StringView &key,
                const StringView &start,
                const StringView &end,
                long long count,
                Output output);

    template <typename Output>
    void xread(const StringView &key,
                const StringView &id,
                long long count,
                Output output);

    template <typename Output>
    void xread(const StringView &key,
                const StringView &id,
                Output output) {
        xread(key, id, 0, output);
    }

    template <typename Input, typename Output>
    auto xread(Input first, Input last, long long count, Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type;

    template <typename Input, typename Output>
    auto xread(Input first, Input last, Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xread(first ,last, 0, output);
    }

    template <typename Output>
    void xread(const StringView &key,
                const StringView &id,
                const std::chrono::milliseconds &timeout,
                long long count,
                Output output);

    template <typename Output>
    void xread(const StringView &key,
                const StringView &id,
                const std::chrono::milliseconds &timeout,
                Output output) {
        xread(key, id, timeout, 0, output);
    }

    template <typename Input, typename Output>
    auto xread(Input first,
                Input last,
                const std::chrono::milliseconds &timeout,
                long long count,
                Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type;

    template <typename Input, typename Output>
    auto xread(Input first,
                Input last,
                const std::chrono::milliseconds &timeout,
                Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xread(first, last, timeout, 0, output);
    }

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    long long count,
                    bool noack,
                    Output output);

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    long long count,
                    Output output) {
        xreadgroup(group, consumer, key, id, count, false, output);
    }

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    Output output) {
        xreadgroup(group, consumer, key, id, 0, false, output);
    }

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    long long count,
                    bool noack,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type;

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    long long count,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xreadgroup(group, consumer, first ,last, count, false, output);
    }

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xreadgroup(group, consumer, first ,last, 0, false, output);
    }

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    bool noack,
                    Output output);

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    Output output) {
        xreadgroup(group, consumer, key, id, timeout, count, false, output);
    }

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    const std::chrono::milliseconds &timeout,
                    Output output) {
        xreadgroup(group, consumer, key, id, timeout, 0, false, output);
    }

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    bool noack,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type;

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xreadgroup(group, consumer, first, last, timeout, count, false, output);
    }

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xreadgroup(group, consumer, first, last, timeout, 0, false, output);
    }

    template <typename Output>
    void xrevrange(const StringView &key,
                    const StringView &end,
                    const StringView &start,
                    Output output);

    template <typename Output>
    void xrevrange(const StringView &key,
                    const StringView &end,
                    const StringView &start,
                    long long count,
                    Output output);

    long long xtrim(const StringView &key, long long count, bool approx = true);

private:
    template <typename Impl>
    friend class QueuedRedis;

    friend class RedisCluster;

    // For internal use.
    explicit Redis(const GuardedConnectionSPtr &connection);

    template <std::size_t ...Is, typename ...Args>
    ReplyUPtr _command(const StringView &cmd_name, const IndexSequence<Is...> &, Args &&...args) {
        return command(cmd_name, NthValue<Is>(std::forward<Args>(args)...)...);
    }

    template <typename Cmd, typename ...Args>
    ReplyUPtr _command(Connection &connection, Cmd cmd, Args &&...args);

    template <typename Cmd, typename ...Args>
    ReplyUPtr _score_command(std::true_type, Cmd cmd, Args &&... args);

    template <typename Cmd, typename ...Args>
    ReplyUPtr _score_command(std::false_type, Cmd cmd, Args &&... args);

    template <typename Output, typename Cmd, typename ...Args>
    ReplyUPtr _score_command(Cmd cmd, Args &&... args);

    // Pool Mode.
    // Public constructors create a *Redis* instance with a pool.
    // In this case, *_connection* is a null pointer, and is never used.
    ConnectionPoolSPtr _pool;

    // Single Connection Mode.
    // Private constructor creates a *Redis* instance with a single connection.
    // This is used when we create Transaction, Pipeline and Subscriber.
    // In this case, *_pool* is empty, and is never used.
    GuardedConnectionSPtr _connection;
};

}

}

#include "redis.hpp"

#endif // end SEWENEW_REDISPLUSPLUS_REDIS_H
