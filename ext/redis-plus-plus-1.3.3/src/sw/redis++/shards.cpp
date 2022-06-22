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

#include "shards.h"

namespace sw {

namespace redis {

RedirectionError::RedirectionError(const std::string &msg): ReplyError(msg) {
    std::tie(_slot, _node) = _parse_error(msg);
}

std::pair<Slot, Node> RedirectionError::_parse_error(const std::string &msg) const {
    // "slot ip:port"
    auto space_pos = msg.find(" ");
    auto colon_pos = msg.find(":");
    if (space_pos == std::string::npos
            || colon_pos == std::string::npos
            || colon_pos < space_pos) {
        throw ProtoError("Invalid ASK error message: " + msg);
    }

    try {
        // We need to do a cast for x86 build (32 bit) on Windows.
        // See https://github.com/sewenew/redis-plus-plus/issues/115 for detail.
        auto slot = static_cast<Slot>(std::stoull(msg.substr(0, space_pos)));
        auto host = msg.substr(space_pos + 1, colon_pos - space_pos - 1);
        auto port = std::stoi(msg.substr(colon_pos + 1));

        return {slot, {host, port}};
    } catch (const std::exception &e) {
        throw ProtoError("Invalid ASK error message: " + msg);
    }
}

}

}
