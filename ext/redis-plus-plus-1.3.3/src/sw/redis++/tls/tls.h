/**************************************************************************
   Copyright (c) 2020 sewenew

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

#ifndef SEWENEW_REDISPLUSPLUS_TLS_H
#define SEWENEW_REDISPLUSPLUS_TLS_H

#include <string>
#include <memory>
#include <hiredis/hiredis.h>
#include <hiredis/hiredis_ssl.h>

namespace sw {

namespace redis {

namespace tls {

// Disable auto initializing OpenSSL.
// You should call it only once and call it before any sw::redis::Redis operation.
// Otherwise, the behavior is undefined.
void disable_auto_init();

class TlsInit {
public:
    TlsInit();
};

struct TlsOptions {
    bool enabled = false;

    std::string cacert;

    std::string cacertdir;

    std::string cert;

    std::string key;

    std::string sni;
};

inline bool enabled(const TlsOptions &opts) {
    return opts.enabled;
}

struct TlsContextDeleter {
    void operator()(redisSSLContext *ssl) const {
        if (ssl != nullptr) {
            redisFreeSSLContext(ssl);
        }
    }
};

using TlsContextUPtr = std::unique_ptr<redisSSLContext, TlsContextDeleter>;

TlsContextUPtr secure_connection(redisContext &ctx, const TlsOptions &opts);

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TLS_H
