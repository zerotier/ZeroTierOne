/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_HTTP_HPP
#define ZT_HTTP_HPP

#include <map>
#include <stdexcept>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ZeroTier {

/**
 * Simple synchronous HTTP client used for updater and cli
 */
class Http {
  public:
    /**
     * Make HTTP GET request
     *
     * The caller must set all headers, including Host.
     *
     * @return HTTP status code or 0 on error (responseBody will contain error message)
     */
    static inline unsigned int
    GET(unsigned long maxResponseSize,
        unsigned long timeout,
        const struct sockaddr* remoteAddress,
        const char* path,
        const std::map<std::string, std::string>& requestHeaders,
        std::map<std::string, std::string>& responseHeaders,
        std::string& responseBody)
    {
        return _do("GET", maxResponseSize, timeout, remoteAddress, path, requestHeaders, (const void*)0, 0, responseHeaders, responseBody);
    }

    /**
     * Make HTTP DELETE request
     *
     * The caller must set all headers, including Host.
     *
     * @return HTTP status code or 0 on error (responseBody will contain error message)
     */
    static inline unsigned int
    DEL(unsigned long maxResponseSize,
        unsigned long timeout,
        const struct sockaddr* remoteAddress,
        const char* path,
        const std::map<std::string, std::string>& requestHeaders,
        std::map<std::string, std::string>& responseHeaders,
        std::string& responseBody)
    {
        return _do("DELETE", maxResponseSize, timeout, remoteAddress, path, requestHeaders, (const void*)0, 0, responseHeaders, responseBody);
    }

    /**
     * Make HTTP POST request
     *
     * It is the responsibility of the caller to set all headers. With POST, the
     * Content-Length and Content-Type headers must be set or the POST will not
     * work.
     *
     * @return HTTP status code or 0 on error (responseBody will contain error message)
     */
    static inline unsigned int POST(
        unsigned long maxResponseSize,
        unsigned long timeout,
        const struct sockaddr* remoteAddress,
        const char* path,
        const std::map<std::string, std::string>& requestHeaders,
        const void* postData,
        unsigned long postDataLength,
        std::map<std::string, std::string>& responseHeaders,
        std::string& responseBody)
    {
        return _do("POST", maxResponseSize, timeout, remoteAddress, path, requestHeaders, postData, postDataLength, responseHeaders, responseBody);
    }

    /**
     * Make HTTP PUT request
     *
     * It is the responsibility of the caller to set all headers. With PUT, the
     * Content-Length and Content-Type headers must be set or the PUT will not
     * work.
     *
     * @return HTTP status code or 0 on error (responseBody will contain error message)
     */
    static inline unsigned int
    PUT(unsigned long maxResponseSize,
        unsigned long timeout,
        const struct sockaddr* remoteAddress,
        const char* path,
        const std::map<std::string, std::string>& requestHeaders,
        const void* postData,
        unsigned long postDataLength,
        std::map<std::string, std::string>& responseHeaders,
        std::string& responseBody)
    {
        return _do("PUT", maxResponseSize, timeout, remoteAddress, path, requestHeaders, postData, postDataLength, responseHeaders, responseBody);
    }

  private:
    static unsigned int
    _do(const char* method,
        unsigned long maxResponseSize,
        unsigned long timeout,
        const struct sockaddr* remoteAddress,
        const char* path,
        const std::map<std::string, std::string>& requestHeaders,
        const void* requestBody,
        unsigned long requestBodyLength,
        std::map<std::string, std::string>& responseHeaders,
        std::string& responseBody);
};

}   // namespace ZeroTier

#endif
