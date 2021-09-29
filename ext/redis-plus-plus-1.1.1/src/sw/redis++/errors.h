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

#ifndef SEWENEW_REDISPLUSPLUS_ERRORS_H
#define SEWENEW_REDISPLUSPLUS_ERRORS_H

#include <exception>
#include <string>
#include <hiredis/hiredis.h>

namespace sw {

namespace redis {

enum ReplyErrorType {
    ERR,
    MOVED,
    ASK
};

class Error : public std::exception {
public:
    explicit Error(const std::string &msg) : _msg(msg) {}

    Error(const Error &) = default;
    Error& operator=(const Error &) = default;

    Error(Error &&) = default;
    Error& operator=(Error &&) = default;

    virtual ~Error() = default;

    virtual const char* what() const noexcept {
        return _msg.data();
    }

private:
    std::string _msg;
};

class IoError : public Error {
public:
    explicit IoError(const std::string &msg) : Error(msg) {}

    IoError(const IoError &) = default;
    IoError& operator=(const IoError &) = default;

    IoError(IoError &&) = default;
    IoError& operator=(IoError &&) = default;

    virtual ~IoError() = default;
};

class TimeoutError : public IoError {
public:
    explicit TimeoutError(const std::string &msg) : IoError(msg) {}

    TimeoutError(const TimeoutError &) = default;
    TimeoutError& operator=(const TimeoutError &) = default;

    TimeoutError(TimeoutError &&) = default;
    TimeoutError& operator=(TimeoutError &&) = default;

    virtual ~TimeoutError() = default;
};

class ClosedError : public Error {
public:
    explicit ClosedError(const std::string &msg) : Error(msg) {}

    ClosedError(const ClosedError &) = default;
    ClosedError& operator=(const ClosedError &) = default;

    ClosedError(ClosedError &&) = default;
    ClosedError& operator=(ClosedError &&) = default;

    virtual ~ClosedError() = default;
};

class ProtoError : public Error {
public:
    explicit ProtoError(const std::string &msg) : Error(msg) {}

    ProtoError(const ProtoError &) = default;
    ProtoError& operator=(const ProtoError &) = default;

    ProtoError(ProtoError &&) = default;
    ProtoError& operator=(ProtoError &&) = default;

    virtual ~ProtoError() = default;
};

class OomError : public Error {
public:
    explicit OomError(const std::string &msg) : Error(msg) {}

    OomError(const OomError &) = default;
    OomError& operator=(const OomError &) = default;

    OomError(OomError &&) = default;
    OomError& operator=(OomError &&) = default;

    virtual ~OomError() = default;
};

class ReplyError : public Error {
public:
    explicit ReplyError(const std::string &msg) : Error(msg) {}

    ReplyError(const ReplyError &) = default;
    ReplyError& operator=(const ReplyError &) = default;

    ReplyError(ReplyError &&) = default;
    ReplyError& operator=(ReplyError &&) = default;

    virtual ~ReplyError() = default;
};

class WatchError : public Error {
public:
    explicit WatchError() : Error("Watched key has been modified") {}

    WatchError(const WatchError &) = default;
    WatchError& operator=(const WatchError &) = default;

    WatchError(WatchError &&) = default;
    WatchError& operator=(WatchError &&) = default;

    virtual ~WatchError() = default;
};


// MovedError and AskError are defined in shards.h
class MovedError;

class AskError;

void throw_error(redisContext &context, const std::string &err_info);

void throw_error(const redisReply &reply);

}

}

#endif // end SEWENEW_REDISPLUSPLUS_ERRORS_H
