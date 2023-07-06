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

#ifndef SEWENEW_REDISPLUSPLUS_SUBSCRIBER_H
#define SEWENEW_REDISPLUSPLUS_SUBSCRIBER_H

#include <unordered_map>
#include <string>
#include <functional>
#include "connection.h"
#include "reply.h"
#include "command.h"
#include "utils.h"

namespace sw {

namespace redis {

// @NOTE: Subscriber is NOT thread-safe.
// Subscriber uses callbacks to handle messages. There are 6 kinds of messages:
// 1) MESSAGE: message sent to a channel.
// 2) PMESSAGE: message sent to channels of a given pattern.
// 3) SUBSCRIBE: meta message sent when we successfully subscribe to a channel.
// 4) UNSUBSCRIBE: meta message sent when we successfully unsubscribe to a channel.
// 5) PSUBSCRIBE: meta message sent when we successfully subscribe to a channel pattern.
// 6) PUNSUBSCRIBE: meta message sent when we successfully unsubscribe to a channel pattern.
//
// Use Subscriber::on_message(MsgCallback) to set the callback function for message of
// *MESSAGE* type, and the callback interface is:
// void (std::string channel, std::string msg)
//
// Use Subscriber::on_pmessage(PatternMsgCallback) to set the callback function for message of
// *PMESSAGE* type, and the callback interface is:
// void (std::string pattern, std::string channel, std::string msg)
//
// Messages of other types are called *META MESSAGE*, they have the same callback interface.
// Use Subscriber::on_meta(MetaCallback) to set the callback function:
// void (Subscriber::MsgType type, OptionalString channel, long long num)
//
// NOTE: If we haven't subscribe/psubscribe to any channel/pattern, and try to
// unsubscribe/punsubscribe without any parameter, i.e. unsubscribe/punsubscribe all
// channels/patterns, *channel* will be null. So the second parameter of meta callback
// is of type *OptionalString*.
//
// All these callback interfaces pass std::string by value, and you can take their ownership
// (i.e. std::move) safely.
//
// If you don't set callback for a specific kind of message, Subscriber::consume() will
// receive the message, and ignore it, i.e. no callback will be called.
class Subscriber {
public:
    Subscriber(const Subscriber &) = delete;
    Subscriber& operator=(const Subscriber &) = delete;

    Subscriber(Subscriber &&) = default;
    Subscriber& operator=(Subscriber &&) = default;

    ~Subscriber() = default;

    enum class MsgType {
        SUBSCRIBE,
        UNSUBSCRIBE,
        PSUBSCRIBE,
        PUNSUBSCRIBE,
        MESSAGE,
        PMESSAGE
    };

    template <typename MsgCb>
    void on_message(MsgCb msg_callback);

    template <typename PMsgCb>
    void on_pmessage(PMsgCb pmsg_callback);

    template <typename MetaCb>
    void on_meta(MetaCb meta_callback);

    void subscribe(const StringView &channel);

    template <typename Input>
    void subscribe(Input first, Input last);

    template <typename T>
    void subscribe(std::initializer_list<T> channels) {
        subscribe(channels.begin(), channels.end());
    }

    void unsubscribe();

    void unsubscribe(const StringView &channel);

    template <typename Input>
    void unsubscribe(Input first, Input last);

    template <typename T>
    void unsubscribe(std::initializer_list<T> channels) {
        unsubscribe(channels.begin(), channels.end());
    }

    void psubscribe(const StringView &pattern);

    template <typename Input>
    void psubscribe(Input first, Input last);

    template <typename T>
    void psubscribe(std::initializer_list<T> channels) {
        psubscribe(channels.begin(), channels.end());
    }

    void punsubscribe();

    void punsubscribe(const StringView &channel);

    template <typename Input>
    void punsubscribe(Input first, Input last);

    template <typename T>
    void punsubscribe(std::initializer_list<T> channels) {
        punsubscribe(channels.begin(), channels.end());
    }

    void consume();

private:
    friend class Redis;

    friend class RedisCluster;

    explicit Subscriber(Connection connection);

    MsgType _msg_type(redisReply *reply) const;

    void _check_connection();

    void _handle_message(redisReply &reply);

    void _handle_pmessage(redisReply &reply);

    void _handle_meta(MsgType type, redisReply &reply);

    using MsgCallback = std::function<void (std::string channel, std::string msg)>;

    using PatternMsgCallback = std::function<void (std::string pattern,
                                                    std::string channel,
                                                    std::string msg)>;

    using MetaCallback = std::function<void (MsgType type,
                                                OptionalString channel,
                                                long long num)>;

    using TypeIndex = std::unordered_map<std::string, MsgType>;
    static const TypeIndex _msg_type_index;

    Connection _connection;

    MsgCallback _msg_callback = nullptr;

    PatternMsgCallback _pmsg_callback = nullptr;

    MetaCallback _meta_callback = nullptr;
};

template <typename MsgCb>
void Subscriber::on_message(MsgCb msg_callback) {
    _msg_callback = msg_callback;
}

template <typename PMsgCb>
void Subscriber::on_pmessage(PMsgCb pmsg_callback) {
    _pmsg_callback = pmsg_callback;
}

template <typename MetaCb>
void Subscriber::on_meta(MetaCb meta_callback) {
    _meta_callback = meta_callback;
}

template <typename Input>
void Subscriber::subscribe(Input first, Input last) {
    if (first == last) {
        return;
    }

    _check_connection();

    cmd::subscribe_range(_connection, first, last);
}

template <typename Input>
void Subscriber::unsubscribe(Input first, Input last) {
    _check_connection();

    cmd::unsubscribe_range(_connection, first, last);
}

template <typename Input>
void Subscriber::psubscribe(Input first, Input last) {
    if (first == last) {
        return;
    }

    _check_connection();

    cmd::psubscribe_range(_connection, first, last);
}

template <typename Input>
void Subscriber::punsubscribe(Input first, Input last) {
    _check_connection();

    cmd::punsubscribe_range(_connection, first, last);
}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_SUBSCRIBER_H
