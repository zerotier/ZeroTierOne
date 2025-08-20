/*
 *   httpclientlite.hpp
 *   ===========================================================================================
 *
 *   The MIT License
 *
 *   Copyright (c) 2016 Christian C. Sachs
 *   Copyright (c) 2021 Maxim G.
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */


#pragma once

#if defined (__linux__)
# define PLATFORM_LINUX
#elif defined (_WIN32) || defined (_WIN64)
# define PLATFORM_WINDOWS
#else
/* TODO:
 *  - Added Apple OS */

/* warning: Unknown OS */
#endif


#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <sstream>

#include <sys/types.h>

#if defined (PLATFORM_WINDOWS)
# include <WinSock2.h>
# include <WS2tcpip.h>

  typedef SOCKET socktype_t;
  typedef int socklen_t;

# pragma comment(lib, "ws2_32.lib")

#elif defined (PLATFORM_LINUX)
# include <unistd.h>
# include <sys/socket.h>
# include <netdb.h>

# define INVALID_SOCKET -1
# define closesocket(__sock) close(__sock)

typedef int socktype_t;

#endif /* PLATFORM_WINDOWS or PLATFORM_LINUX */



const std::string content_type = "Content-Type: text/plain; version=0.0.4; charset=utf-8";




namespace jdl {

  void init_socket() {
#if defined (PLATFORM_WINDOWS)
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif /* PLATFORM_WINDOWS */
  }

  void deinit_socket() {
#if defined (PLATFORM_WINDOWS)
    WSACleanup();
#endif /* PLATFORM_WINDOWS */
  }


  class tokenizer {
  public:
    inline tokenizer(std::string &str) : str(str), position(0){}

    inline std::string next(std::string search, bool returnTail = false) {
      size_t hit = str.find(search, position);
      if (hit == std::string::npos) {
        if (returnTail) {
          return tail();
        } else {
          return "";
        }
      }

      size_t oldPosition = position;
      position = hit + search.length();

      return str.substr(oldPosition, hit - oldPosition);
    }

    inline std::string tail() {
      size_t oldPosition = position;
      position = str.length();
      return str.substr(oldPosition);
    }

  private:
    std::string str;
    std::size_t position;
  };

  typedef std::map<std::string, std::string> stringMap;

  struct URI {
    inline void parseParameters() {
      tokenizer qt(querystring);
      do {
        std::string key = qt.next("=");
        if (key == "")
          break;
        parameters[key] = qt.next("&", true);
      } while (true);
    }

    inline URI(std::string input, bool shouldParseParameters = false) {
      tokenizer t = tokenizer(input);
      protocol = t.next("://");
      std::string hostPortString = t.next("/");

      tokenizer hostPort(hostPortString);

      host = hostPort.next(hostPortString[0] == '[' ? "]:" : ":", true);

      if (host[0] == '[')
        host = host.substr(1, host.size() - 1);

      port = hostPort.tail();
      if (port.empty())
        port = "80";

      address = t.next("?", true);
      querystring = t.next("#", true);

      hash = t.tail();

      if (shouldParseParameters) {
        parseParameters();
      }
    }

    std::string protocol, host, port, address, querystring, hash;
    stringMap parameters;
  };

  struct HTTPResponse {
    bool success;
    std::string protocol;
    std::string response;
    std::string responseString;

    stringMap header;

    std::string body;

    inline HTTPResponse() : success(true){}
    inline static HTTPResponse fail() {
      HTTPResponse result;
      result.success = false;
      return result;
    }
  };

  struct HTTPClient {
    typedef enum {
      m_options = 0,
      m_get,
      m_head,
      m_post,
      m_put,
      m_delete,
      m_trace,
      m_connect
    } HTTPMethod;

    inline static const char *method2string(HTTPMethod method) {
      const char *methods[] = {"OPTIONS", "GET",   "HEAD",    "POST", "PUT",
                               "DELETE",  "TRACE", "CONNECT", nullptr};
      return methods[method];
    }

    inline static socktype_t connectToURI(const URI& uri) {
      struct addrinfo hints, *result, *rp;

      memset(&hints, 0, sizeof(addrinfo));

      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;

      int getaddrinfo_result =
          getaddrinfo(uri.host.c_str(), uri.port.c_str(), &hints, &result);

      if (getaddrinfo_result != 0)
        return -1;

      socktype_t fd = INVALID_SOCKET;

      for (rp = result; rp != nullptr; rp = rp->ai_next) {

        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (fd == INVALID_SOCKET) {
          continue;
        }

        int connect_result = connect(fd, rp->ai_addr, static_cast<socklen_t>(rp->ai_addrlen));

        if (connect_result == -1) {
          // successfully created a socket, but connection failed. close it!
          closesocket(fd);
          fd = INVALID_SOCKET;
          continue;
        }

        break;
      }

      freeaddrinfo(result);

      return fd;
    }

    inline static std::string bufferedRead(socktype_t fd) {
      size_t initial_factor = 4, buffer_increment_size = 8192, buffer_size = 0,
             bytes_read = 0;
      std::string buffer;

      buffer.resize(initial_factor * buffer_increment_size);

  //    do {
        bytes_read = recv(fd, ((char*)buffer.c_str()) + buffer_size,
                          static_cast<socklen_t>(buffer.size() - buffer_size), 0);

        buffer_size += bytes_read;

  //      if (bytes_read > 0 &&
  //          (buffer.size() - buffer_size) < buffer_increment_size) {
  //        buffer.resize(buffer.size() + buffer_increment_size);
  //      }
  //    } while (bytes_read > 0);

      buffer.resize(buffer_size);
      return buffer;
    }

  #define HTTP_NEWLINE "\r\n"
  #define HTTP_SPACE " "
  #define HTTP_HEADER_SEPARATOR ": "

    inline static HTTPResponse request(HTTPMethod method, const URI& uri, const std::string& body = "") {

      socktype_t fd = connectToURI(uri);
      if (fd < 0)
        return HTTPResponse::fail();

  //    string request = string(method2string(method)) + string(" /") +
  //                     uri.address + ((uri.querystring == "") ? "" : "?") +
  //                     uri.querystring + " HTTP/1.1" HTTP_NEWLINE "Host: " +
  //                     uri.host + HTTP_NEWLINE
  //                     "Accept: */*" HTTP_NEWLINE
  //                     "Connection: close" HTTP_NEWLINE HTTP_NEWLINE;

      std::string request = std::string(method2string(method)) + std::string(" /") +
                            uri.address + ((uri.querystring == "") ? "" : "?") + uri.querystring + " HTTP/1.1" + HTTP_NEWLINE +
                            "Host: " + uri.host + ":" + uri.port + HTTP_NEWLINE +
                            "Accept: */*" + HTTP_NEWLINE +
                            content_type + HTTP_NEWLINE +
                            "Content-Length: " + std::to_string(body.size()) + HTTP_NEWLINE + HTTP_NEWLINE +
                            body;

      /*int bytes_written = */send(fd, request.c_str(), static_cast<socklen_t>(request.size()), 0);

      std::string buffer = bufferedRead(fd);

      closesocket(fd);

      HTTPResponse result;

      tokenizer bt(buffer);

      result.protocol = bt.next(HTTP_SPACE);
      result.response = bt.next(HTTP_SPACE);
      result.responseString = bt.next(HTTP_NEWLINE);

      std::string header = bt.next(HTTP_NEWLINE HTTP_NEWLINE);

      result.body = bt.tail();

      tokenizer ht(header);

      do {
        std::string key = ht.next(HTTP_HEADER_SEPARATOR);
        if (key == "")
          break;
        result.header[key] = ht.next(HTTP_NEWLINE, true);
      } while (true);

      return result;
    }
  };

} /* jdl:: */
