//
//  httplib.h
//
//  Copyright (c) 2019 Yuji Hirose. All rights reserved.
//  MIT License
//

#ifndef CPPHTTPLIB_HTTPLIB_H
#define CPPHTTPLIB_HTTPLIB_H

#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif //_CRT_SECURE_NO_WARNINGS

#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif //_CRT_NONSTDC_NO_DEPRECATE

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf_s
#endif // _MSC_VER

#ifndef S_ISREG
#define S_ISREG(m) (((m)&S_IFREG) == S_IFREG)
#endif // S_ISREG

#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFDIR) == S_IFDIR)
#endif // S_ISDIR

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif // strcasecmp

typedef SOCKET socket_t;
#else
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef int socket_t;
#define INVALID_SOCKET (-1)
#endif //_WIN32

#include <assert.h>
#include <atomic>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <thread>

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
inline const unsigned char *ASN1_STRING_get0_data(const ASN1_STRING *asn1) {
  return M_ASN1_STRING_data(asn1);
}
#endif
#endif

#ifdef CPPHTTPLIB_ZLIB_SUPPORT
#include <zlib.h>
#endif

/*
 * Configuration
 */
#define CPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND 5
#define CPPHTTPLIB_KEEPALIVE_TIMEOUT_USECOND 0
#define CPPHTTPLIB_KEEPALIVE_MAX_COUNT 5
#define CPPHTTPLIB_READ_TIMEOUT_SECOND 5
#define CPPHTTPLIB_READ_TIMEOUT_USECOND 0
#define CPPHTTPLIB_REQUEST_URI_MAX_LENGTH 8192
#define CPPHTTPLIB_PAYLOAD_MAX_LENGTH (std::numeric_limits<size_t>::max)()
#define CPPHTTPLIB_RECV_BUFSIZ size_t(4096u)

namespace httplib {

namespace detail {

struct ci {
  bool operator()(const std::string &s1, const std::string &s2) const {
    return std::lexicographical_compare(
        s1.begin(), s1.end(), s2.begin(), s2.end(),
        [](char c1, char c2) { return ::tolower(c1) < ::tolower(c2); });
  }
};

} // namespace detail

enum class HttpVersion { v1_0 = 0, v1_1 };

typedef std::multimap<std::string, std::string, detail::ci> Headers;

template <typename uint64_t, typename... Args>
std::pair<std::string, std::string> make_range_header(uint64_t value,
                                                      Args... args);

typedef std::multimap<std::string, std::string> Params;
typedef std::smatch Match;

typedef std::function<std::string(uint64_t offset)> ContentProducer;
typedef std::function<void(const char *data, size_t len)> ContentReceiver;
typedef std::function<bool(uint64_t current, uint64_t total)> Progress;

struct MultipartFile {
  std::string filename;
  std::string content_type;
  size_t offset = 0;
  size_t length = 0;
};
typedef std::multimap<std::string, MultipartFile> MultipartFiles;

struct Request {
  std::string version;
  std::string method;
  std::string target;
  std::string path;
  Headers headers;
  std::string body;
  Params params;
  MultipartFiles files;
  Match matches;

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  const SSL *ssl;
#endif

  bool has_header(const char *key) const;
  std::string get_header_value(const char *key, size_t id = 0) const;
  size_t get_header_value_count(const char *key) const;
  void set_header(const char *key, const char *val);

  bool has_param(const char *key) const;
  std::string get_param_value(const char *key, size_t id = 0) const;
  size_t get_param_value_count(const char *key) const;

  bool has_file(const char *key) const;
  MultipartFile get_file_value(const char *key) const;
};

struct Response {
  std::string version;
  int status;
  Headers headers;
  std::string body;

  ContentProducer content_producer;
  ContentReceiver content_receiver;
  Progress progress;

  bool has_header(const char *key) const;
  std::string get_header_value(const char *key, size_t id = 0) const;
  size_t get_header_value_count(const char *key) const;
  void set_header(const char *key, const char *val);

  void set_redirect(const char *uri);
  void set_content(const char *s, size_t n, const char *content_type);
  void set_content(const std::string &s, const char *content_type);

  Response() : status(-1) {}
};

class Stream {
public:
  virtual ~Stream() {}
  virtual int read(char *ptr, size_t size) = 0;
  virtual int write(const char *ptr, size_t size1) = 0;
  virtual int write(const char *ptr) = 0;
  virtual std::string get_remote_addr() const = 0;

  template <typename... Args>
  void write_format(const char *fmt, const Args &... args);
};

class SocketStream : public Stream {
public:
  SocketStream(socket_t sock);
  virtual ~SocketStream();

  virtual int read(char *ptr, size_t size);
  virtual int write(const char *ptr, size_t size);
  virtual int write(const char *ptr);
  virtual std::string get_remote_addr() const;

private:
  socket_t sock_;
};

class BufferStream : public Stream {
public:
  BufferStream() {}
  virtual ~BufferStream() {}

  virtual int read(char *ptr, size_t size);
  virtual int write(const char *ptr, size_t size);
  virtual int write(const char *ptr);
  virtual std::string get_remote_addr() const;

  const std::string &get_buffer() const;

private:
  std::string buffer;
};

class Server {
public:
  typedef std::function<void(const Request &, Response &)> Handler;
  typedef std::function<void(const Request &, const Response &)> Logger;

  Server();

  virtual ~Server();

  virtual bool is_valid() const;

  Server &Get(const char *pattern, Handler handler);
  Server &Post(const char *pattern, Handler handler);

  Server &Put(const char *pattern, Handler handler);
  Server &Patch(const char *pattern, Handler handler);
  Server &Delete(const char *pattern, Handler handler);
  Server &Options(const char *pattern, Handler handler);

  bool set_base_dir(const char *path);

  void set_error_handler(Handler handler);
  void set_logger(Logger logger);

  void set_keep_alive_max_count(size_t count);
  void set_payload_max_length(uint64_t length);

  int bind_to_any_port(const char *host, int socket_flags = 0);
  bool listen_after_bind();

  bool listen(const char *host, int port, int socket_flags = 0);

  bool is_running() const;
  void stop();

protected:
  bool process_request(Stream &strm, bool last_connection,
                       bool &connection_close,
                       std::function<void(Request &)> setup_request = nullptr);

  size_t keep_alive_max_count_;
  size_t payload_max_length_;

private:
  typedef std::vector<std::pair<std::regex, Handler>> Handlers;

  socket_t create_server_socket(const char *host, int port,
                                int socket_flags) const;
  int bind_internal(const char *host, int port, int socket_flags);
  bool listen_internal();

  bool routing(Request &req, Response &res);
  bool handle_file_request(Request &req, Response &res);
  bool dispatch_request(Request &req, Response &res, Handlers &handlers);

  bool parse_request_line(const char *s, Request &req);
  void write_response(Stream &strm, bool last_connection, const Request &req,
                      Response &res);

  virtual bool read_and_close_socket(socket_t sock);

  std::atomic<bool> is_running_;
  std::atomic<socket_t> svr_sock_;
  std::string base_dir_;
  Handlers get_handlers_;
  Handlers post_handlers_;
  Handlers put_handlers_;
  Handlers patch_handlers_;
  Handlers delete_handlers_;
  Handlers options_handlers_;
  Handler error_handler_;
  Logger logger_;

  // TODO: Use thread pool...
  std::mutex running_threads_mutex_;
  int running_threads_;
};

class Client {
public:
  Client(const char *host, int port = 80, time_t timeout_sec = 300);

  virtual ~Client();

  virtual bool is_valid() const;

  std::shared_ptr<Response> Get(const char *path, Progress progress = nullptr);
  std::shared_ptr<Response> Get(const char *path, const Headers &headers,
                                Progress progress = nullptr);

  std::shared_ptr<Response> Get(const char *path,
                                ContentReceiver content_receiver,
                                Progress progress = nullptr);
  std::shared_ptr<Response> Get(const char *path, const Headers &headers,
                                ContentReceiver content_receiver,
                                Progress progress = nullptr);

  std::shared_ptr<Response> Head(const char *path);
  std::shared_ptr<Response> Head(const char *path, const Headers &headers);

  std::shared_ptr<Response> Post(const char *path, const std::string &body,
                                 const char *content_type);
  std::shared_ptr<Response> Post(const char *path, const Headers &headers,
                                 const std::string &body,
                                 const char *content_type);

  std::shared_ptr<Response> Post(const char *path, const Params &params);
  std::shared_ptr<Response> Post(const char *path, const Headers &headers,
                                 const Params &params);

  std::shared_ptr<Response> Put(const char *path, const std::string &body,
                                const char *content_type);
  std::shared_ptr<Response> Put(const char *path, const Headers &headers,
                                const std::string &body,
                                const char *content_type);

  std::shared_ptr<Response> Patch(const char *path, const std::string &body,
                                  const char *content_type);
  std::shared_ptr<Response> Patch(const char *path, const Headers &headers,
                                  const std::string &body,
                                  const char *content_type);

  std::shared_ptr<Response> Delete(const char *path,
                                   const std::string &body = std::string(),
                                   const char *content_type = nullptr);
  std::shared_ptr<Response> Delete(const char *path, const Headers &headers,
                                   const std::string &body = std::string(),
                                   const char *content_type = nullptr);

  std::shared_ptr<Response> Options(const char *path);
  std::shared_ptr<Response> Options(const char *path, const Headers &headers);

  bool send(Request &req, Response &res);

protected:
  bool process_request(Stream &strm, Request &req, Response &res,
                       bool &connection_close);

  const std::string host_;
  const int port_;
  time_t timeout_sec_;
  const std::string host_and_port_;

private:
  socket_t create_client_socket() const;
  bool read_response_line(Stream &strm, Response &res);
  void write_request(Stream &strm, Request &req);

  virtual bool read_and_close_socket(socket_t sock, Request &req,
                                     Response &res);
  virtual bool is_ssl() const;
};

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
class SSLSocketStream : public Stream {
public:
  SSLSocketStream(socket_t sock, SSL *ssl);
  virtual ~SSLSocketStream();

  virtual int read(char *ptr, size_t size);
  virtual int write(const char *ptr, size_t size);
  virtual int write(const char *ptr);
  virtual std::string get_remote_addr() const;

private:
  socket_t sock_;
  SSL *ssl_;
};

class SSLServer : public Server {
public:
  SSLServer(const char *cert_path, const char *private_key_path,
            const char *client_ca_cert_file_path = nullptr,
            const char *client_ca_cert_dir_path = nullptr);

  virtual ~SSLServer();

  virtual bool is_valid() const;

private:
  virtual bool read_and_close_socket(socket_t sock);

  SSL_CTX *ctx_;
  std::mutex ctx_mutex_;
};

class SSLClient : public Client {
public:
  SSLClient(const char *host, int port = 443, time_t timeout_sec = 300,
            const char *client_cert_path = nullptr,
            const char *client_key_path = nullptr);

  virtual ~SSLClient();

  virtual bool is_valid() const;

  void set_ca_cert_path(const char *ca_ceert_file_path,
                        const char *ca_cert_dir_path = nullptr);
  void enable_server_certificate_verification(bool enabled);

  long get_openssl_verify_result() const;

private:
  virtual bool read_and_close_socket(socket_t sock, Request &req,
                                     Response &res);
  virtual bool is_ssl() const;

  bool verify_host(X509 *server_cert) const;
  bool verify_host_with_subject_alt_name(X509 *server_cert) const;
  bool verify_host_with_common_name(X509 *server_cert) const;
  bool check_host_name(const char *pattern, size_t pattern_len) const;

  SSL_CTX *ctx_;
  std::mutex ctx_mutex_;
  std::vector<std::string> host_components_;
  std::string ca_cert_file_path_;
  std::string ca_cert_dir_path_;
  bool server_certificate_verification_ = false;
  long verify_result_ = 0;
};
#endif

/*
 * Implementation
 */
namespace detail {

inline bool is_hex(char c, int &v) {
  if (0x20 <= c && isdigit(c)) {
    v = c - '0';
    return true;
  } else if ('A' <= c && c <= 'F') {
    v = c - 'A' + 10;
    return true;
  } else if ('a' <= c && c <= 'f') {
    v = c - 'a' + 10;
    return true;
  }
  return false;
}

inline bool from_hex_to_i(const std::string &s, size_t i, size_t cnt,
                          int &val) {
  if (i >= s.size()) { return false; }

  val = 0;
  for (; cnt; i++, cnt--) {
    if (!s[i]) { return false; }
    int v = 0;
    if (is_hex(s[i], v)) {
      val = val * 16 + v;
    } else {
      return false;
    }
  }
  return true;
}

inline std::string from_i_to_hex(uint64_t n) {
  const char *charset = "0123456789abcdef";
  std::string ret;
  do {
    ret = charset[n & 15] + ret;
    n >>= 4;
  } while (n > 0);
  return ret;
}

inline size_t to_utf8(int code, char *buff) {
  if (code < 0x0080) {
    buff[0] = (code & 0x7F);
    return 1;
  } else if (code < 0x0800) {
    buff[0] = (0xC0 | ((code >> 6) & 0x1F));
    buff[1] = (0x80 | (code & 0x3F));
    return 2;
  } else if (code < 0xD800) {
    buff[0] = (0xE0 | ((code >> 12) & 0xF));
    buff[1] = (0x80 | ((code >> 6) & 0x3F));
    buff[2] = (0x80 | (code & 0x3F));
    return 3;
  } else if (code < 0xE000) { // D800 - DFFF is invalid...
    return 0;
  } else if (code < 0x10000) {
    buff[0] = (0xE0 | ((code >> 12) & 0xF));
    buff[1] = (0x80 | ((code >> 6) & 0x3F));
    buff[2] = (0x80 | (code & 0x3F));
    return 3;
  } else if (code < 0x110000) {
    buff[0] = (0xF0 | ((code >> 18) & 0x7));
    buff[1] = (0x80 | ((code >> 12) & 0x3F));
    buff[2] = (0x80 | ((code >> 6) & 0x3F));
    buff[3] = (0x80 | (code & 0x3F));
    return 4;
  }

  // NOTREACHED
  return 0;
}

// NOTE: This code came up with the following stackoverflow post:
// https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c
inline std::string base64_encode(const std::string &in) {
  static const auto lookup =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string out;
  out.reserve(in.size());

  int val = 0;
  int valb = -6;

  for (uint8_t c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(lookup[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }

  if (valb > -6) {
    out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
  }

  while (out.size() % 4) {
    out.push_back('=');
  }

  return out;
}

inline bool is_file(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) >= 0 && S_ISREG(st.st_mode);
}

inline bool is_dir(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) >= 0 && S_ISDIR(st.st_mode);
}

inline bool is_valid_path(const std::string &path) {
  size_t level = 0;
  size_t i = 0;

  // Skip slash
  while (i < path.size() && path[i] == '/') {
    i++;
  }

  while (i < path.size()) {
    // Read component
    auto beg = i;
    while (i < path.size() && path[i] != '/') {
      i++;
    }

    auto len = i - beg;
    assert(len > 0);

    if (!path.compare(beg, len, ".")) {
      ;
    } else if (!path.compare(beg, len, "..")) {
      if (level == 0) { return false; }
      level--;
    } else {
      level++;
    }

    // Skip slash
    while (i < path.size() && path[i] == '/') {
      i++;
    }
  }

  return true;
}

inline void read_file(const std::string &path, std::string &out) {
  std::ifstream fs(path, std::ios_base::binary);
  fs.seekg(0, std::ios_base::end);
  auto size = fs.tellg();
  fs.seekg(0);
  out.resize(static_cast<size_t>(size));
  fs.read(&out[0], size);
}

inline std::string file_extension(const std::string &path) {
  std::smatch m;
  auto pat = std::regex("\\.([a-zA-Z0-9]+)$");
  if (std::regex_search(path, m, pat)) { return m[1].str(); }
  return std::string();
}

template <class Fn> void split(const char *b, const char *e, char d, Fn fn) {
  int i = 0;
  int beg = 0;

  while (e ? (b + i != e) : (b[i] != '\0')) {
    if (b[i] == d) {
      fn(&b[beg], &b[i]);
      beg = i + 1;
    }
    i++;
  }

  if (i) { fn(&b[beg], &b[i]); }
}

// NOTE: until the read size reaches `fixed_buffer_size`, use `fixed_buffer`
// to store data. The call can set memory on stack for performance.
class stream_line_reader {
public:
  stream_line_reader(Stream &strm, char *fixed_buffer, size_t fixed_buffer_size)
      : strm_(strm), fixed_buffer_(fixed_buffer),
        fixed_buffer_size_(fixed_buffer_size) {}

  const char *ptr() const {
    if (glowable_buffer_.empty()) {
      return fixed_buffer_;
    } else {
      return glowable_buffer_.data();
    }
  }

  size_t size() const {
    if (glowable_buffer_.empty()) {
      return fixed_buffer_used_size_;
    } else {
      return glowable_buffer_.size();
    }
  }

  bool getline() {
    fixed_buffer_used_size_ = 0;
    glowable_buffer_.clear();

    for (size_t i = 0;; i++) {
      char byte;
      auto n = strm_.read(&byte, 1);

      if (n < 0) {
        return false;
      } else if (n == 0) {
        if (i == 0) {
          return false;
        } else {
          break;
        }
      }

      append(byte);

      if (byte == '\n') { break; }
    }

    return true;
  }

private:
  void append(char c) {
    if (fixed_buffer_used_size_ < fixed_buffer_size_ - 1) {
      fixed_buffer_[fixed_buffer_used_size_++] = c;
      fixed_buffer_[fixed_buffer_used_size_] = '\0';
    } else {
      if (glowable_buffer_.empty()) {
        assert(fixed_buffer_[fixed_buffer_used_size_] == '\0');
        glowable_buffer_.assign(fixed_buffer_, fixed_buffer_used_size_);
      }
      glowable_buffer_ += c;
    }
  }

  Stream &strm_;
  char *fixed_buffer_;
  const size_t fixed_buffer_size_;
  size_t fixed_buffer_used_size_;
  std::string glowable_buffer_;
};

inline int close_socket(socket_t sock) {
#ifdef _WIN32
  return closesocket(sock);
#else
  return close(sock);
#endif
}

inline int select_read(socket_t sock, time_t sec, time_t usec) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  timeval tv;
  tv.tv_sec = static_cast<long>(sec);
  tv.tv_usec = static_cast<long>(usec);

  return select(static_cast<int>(sock + 1), &fds, nullptr, nullptr, &tv);
}

inline bool wait_until_socket_is_ready(socket_t sock, time_t sec, time_t usec) {
  fd_set fdsr;
  FD_ZERO(&fdsr);
  FD_SET(sock, &fdsr);

  auto fdsw = fdsr;
  auto fdse = fdsr;

  timeval tv;
  tv.tv_sec = static_cast<long>(sec);
  tv.tv_usec = static_cast<long>(usec);

  if (select(static_cast<int>(sock + 1), &fdsr, &fdsw, &fdse, &tv) < 0) {
    return false;
  } else if (FD_ISSET(sock, &fdsr) || FD_ISSET(sock, &fdsw)) {
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0 ||
        error) {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

template <typename T>
inline bool read_and_close_socket(socket_t sock, size_t keep_alive_max_count,
                                  T callback) {
  bool ret = false;

  if (keep_alive_max_count > 0) {
    auto count = keep_alive_max_count;
    while (count > 0 &&
           detail::select_read(sock, CPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND,
                               CPPHTTPLIB_KEEPALIVE_TIMEOUT_USECOND) > 0) {
      SocketStream strm(sock);
      auto last_connection = count == 1;
      auto connection_close = false;

      ret = callback(strm, last_connection, connection_close);
      if (!ret || connection_close) { break; }

      count--;
    }
  } else {
    SocketStream strm(sock);
    auto dummy_connection_close = false;
    ret = callback(strm, true, dummy_connection_close);
  }

  close_socket(sock);
  return ret;
}

inline int shutdown_socket(socket_t sock) {
#ifdef _WIN32
  return shutdown(sock, SD_BOTH);
#else
  return shutdown(sock, SHUT_RDWR);
#endif
}

template <typename Fn>
socket_t create_socket(const char *host, int port, Fn fn,
                       int socket_flags = 0) {
#ifdef _WIN32
#define SO_SYNCHRONOUS_NONALERT 0x20
#define SO_OPENTYPE 0x7008

  int opt = SO_SYNCHRONOUS_NONALERT;
  setsockopt(INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE, (char *)&opt,
             sizeof(opt));
#endif

  // Get address info
  struct addrinfo hints;
  struct addrinfo *result;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = socket_flags;
  hints.ai_protocol = 0;

  auto service = std::to_string(port);

  if (getaddrinfo(host, service.c_str(), &hints, &result)) {
    return INVALID_SOCKET;
  }

  for (auto rp = result; rp; rp = rp->ai_next) {
    // Create a socket
#ifdef _WIN32
    auto sock = WSASocketW(rp->ai_family, rp->ai_socktype, rp->ai_protocol,
                           nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
#else
    auto sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
#endif
    if (sock == INVALID_SOCKET) { continue; }

#ifndef _WIN32
    if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) { continue; }
#endif

    // Make 'reuse address' option available
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&yes, sizeof(yes));
#endif

    // bind or connect
    if (fn(sock, *rp)) {
      freeaddrinfo(result);
      return sock;
    }

    close_socket(sock);
  }

  freeaddrinfo(result);
  return INVALID_SOCKET;
}

inline void set_nonblocking(socket_t sock, bool nonblocking) {
#ifdef _WIN32
  auto flags = nonblocking ? 1UL : 0UL;
  ioctlsocket(sock, FIONBIO, &flags);
#else
  auto flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL,
        nonblocking ? (flags | O_NONBLOCK) : (flags & (~O_NONBLOCK)));
#endif
}

inline bool is_connection_error() {
#ifdef _WIN32
  return WSAGetLastError() != WSAEWOULDBLOCK;
#else
  return errno != EINPROGRESS;
#endif
}

inline std::string get_remote_addr(socket_t sock) {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);

  if (!getpeername(sock, (struct sockaddr *)&addr, &len)) {
    char ipstr[NI_MAXHOST];

    if (!getnameinfo((struct sockaddr *)&addr, len, ipstr, sizeof(ipstr),
                     nullptr, 0, NI_NUMERICHOST)) {
      return ipstr;
    }
  }

  return std::string();
}

inline const char *find_content_type(const std::string &path) {
  auto ext = file_extension(path);
  if (ext == "txt") {
    return "text/plain";
  } else if (ext == "html") {
    return "text/html";
  } else if (ext == "css") {
    return "text/css";
  } else if (ext == "jpeg" || ext == "jpg") {
    return "image/jpg";
  } else if (ext == "png") {
    return "image/png";
  } else if (ext == "gif") {
    return "image/gif";
  } else if (ext == "svg") {
    return "image/svg+xml";
  } else if (ext == "ico") {
    return "image/x-icon";
  } else if (ext == "json") {
    return "application/json";
  } else if (ext == "pdf") {
    return "application/pdf";
  } else if (ext == "js") {
    return "application/javascript";
  } else if (ext == "xml") {
    return "application/xml";
  } else if (ext == "xhtml") {
    return "application/xhtml+xml";
  }
  return nullptr;
}

inline const char *status_message(int status) {
  switch (status) {
  case 200: return "OK";
  case 301: return "Moved Permanently";
  case 302: return "Found";
  case 303: return "See Other";
  case 304: return "Not Modified";
  case 400: return "Bad Request";
  case 403: return "Forbidden";
  case 404: return "Not Found";
  case 413: return "Payload Too Large";
  case 414: return "Request-URI Too Long";
  case 415: return "Unsupported Media Type";
  default:
  case 500: return "Internal Server Error";
  }
}

#ifdef CPPHTTPLIB_ZLIB_SUPPORT
inline bool can_compress(const std::string &content_type) {
  return !content_type.find("text/") || content_type == "image/svg+xml" ||
         content_type == "application/javascript" ||
         content_type == "application/json" ||
         content_type == "application/xml" ||
         content_type == "application/xhtml+xml";
}

inline bool compress(std::string &content) {
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  auto ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8,
                          Z_DEFAULT_STRATEGY);
  if (ret != Z_OK) { return false; }

  strm.avail_in = content.size();
  strm.next_in = (Bytef *)content.data();

  std::string compressed;

  const auto bufsiz = 16384;
  char buff[bufsiz];
  do {
    strm.avail_out = bufsiz;
    strm.next_out = (Bytef *)buff;
    ret = deflate(&strm, Z_FINISH);
    assert(ret != Z_STREAM_ERROR);
    compressed.append(buff, bufsiz - strm.avail_out);
  } while (strm.avail_out == 0);

  assert(ret == Z_STREAM_END);
  assert(strm.avail_in == 0);

  content.swap(compressed);

  deflateEnd(&strm);
  return true;
}

class decompressor {
public:
  decompressor() {
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    // 15 is the value of wbits, which should be at the maximum possible value
    // to ensure that any gzip stream can be decoded. The offset of 16 specifies
    // that the stream to decompress will be formatted with a gzip wrapper.
    is_valid_ = inflateInit2(&strm, 16 + 15) == Z_OK;
  }

  ~decompressor() { inflateEnd(&strm); }

  bool is_valid() const { return is_valid_; }

  template <typename T>
  bool decompress(const char *data, size_t data_len, T callback) {
    int ret = Z_OK;
    std::string decompressed;

    // strm.avail_in = content.size();
    // strm.next_in = (Bytef *)content.data();
    strm.avail_in = data_len;
    strm.next_in = (Bytef *)data;

    const auto bufsiz = 16384;
    char buff[bufsiz];
    do {
      strm.avail_out = bufsiz;
      strm.next_out = (Bytef *)buff;

      ret = inflate(&strm, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);
      switch (ret) {
      case Z_NEED_DICT:
      case Z_DATA_ERROR:
      case Z_MEM_ERROR: inflateEnd(&strm); return false;
      }

      decompressed.append(buff, bufsiz - strm.avail_out);
    } while (strm.avail_out == 0);

    if (ret == Z_STREAM_END) {
      callback(decompressed.data(), decompressed.size());
      return true;
    }

    return false;
  }

private:
  bool is_valid_;
  z_stream strm;
};
#endif

inline bool has_header(const Headers &headers, const char *key) {
  return headers.find(key) != headers.end();
}

inline const char *get_header_value(const Headers &headers, const char *key,
                                    size_t id = 0, const char *def = nullptr) {
  auto it = headers.find(key);
  std::advance(it, id);
  if (it != headers.end()) { return it->second.c_str(); }
  return def;
}

inline uint64_t get_header_value_uint64(const Headers &headers, const char *key,
                                        int def = 0) {
  auto it = headers.find(key);
  if (it != headers.end()) {
    return std::strtoull(it->second.data(), nullptr, 10);
  }
  return def;
}

inline bool read_headers(Stream &strm, Headers &headers) {
  static std::regex re(R"((.+?):\s*(.+?)\s*\r\n)");

  const auto bufsiz = 2048;
  char buf[bufsiz];

  stream_line_reader reader(strm, buf, bufsiz);

  for (;;) {
    if (!reader.getline()) { return false; }
    if (!strcmp(reader.ptr(), "\r\n")) { break; }
    std::cmatch m;
    if (std::regex_match(reader.ptr(), m, re)) {
      auto key = std::string(m[1]);
      auto val = std::string(m[2]);
      headers.emplace(key, val);
    }
  }

  return true;
}

template <typename T>
inline bool read_content_with_length(Stream &strm, size_t len,
                                     Progress progress, T callback) {
  char buf[CPPHTTPLIB_RECV_BUFSIZ];

  size_t r = 0;
  while (r < len) {
    auto n = strm.read(buf, std::min((len - r), CPPHTTPLIB_RECV_BUFSIZ));
    if (n <= 0) { return false; }

    callback(buf, n);

    r += n;

    if (progress) {
      if (!progress(r, len)) { return false; }
    }
  }

  return true;
}

inline void skip_content_with_length(Stream &strm, size_t len) {
  char buf[CPPHTTPLIB_RECV_BUFSIZ];
  size_t r = 0;
  while (r < len) {
    auto n = strm.read(buf, std::min((len - r), CPPHTTPLIB_RECV_BUFSIZ));
    if (n <= 0) { return; }
    r += n;
  }
}

template <typename T>
inline bool read_content_without_length(Stream &strm, T callback) {
  char buf[CPPHTTPLIB_RECV_BUFSIZ];
  for (;;) {
    auto n = strm.read(buf, CPPHTTPLIB_RECV_BUFSIZ);
    if (n < 0) {
      return false;
    } else if (n == 0) {
      return true;
    }
    callback(buf, n);
  }

  return true;
}

template <typename T>
inline bool read_content_chunked(Stream &strm, T callback) {
  const auto bufsiz = 16;
  char buf[bufsiz];

  stream_line_reader reader(strm, buf, bufsiz);

  if (!reader.getline()) { return false; }

  auto chunk_len = std::stoi(reader.ptr(), 0, 16);

  while (chunk_len > 0) {
    if (!read_content_with_length(strm, chunk_len, nullptr, callback)) {
      return false;
    }

    if (!reader.getline()) { return false; }

    if (strcmp(reader.ptr(), "\r\n")) { break; }

    if (!reader.getline()) { return false; }

    chunk_len = std::stoi(reader.ptr(), 0, 16);
  }

  if (chunk_len == 0) {
    // Reader terminator after chunks
    if (!reader.getline() || strcmp(reader.ptr(), "\r\n")) return false;
  }

  return true;
}

inline bool is_chunked_transfer_encoding(const Headers &headers) {
  return !strcasecmp(get_header_value(headers, "Transfer-Encoding", 0, ""),
                     "chunked");
}

template <typename T, typename U>
bool read_content(Stream &strm, T &x, uint64_t payload_max_length, int &status,
                  Progress progress, U callback) {

  ContentReceiver out = [&](const char *buf, size_t n) { callback(buf, n); };

#ifdef CPPHTTPLIB_ZLIB_SUPPORT
  detail::decompressor decompressor;

  if (!decompressor.is_valid()) {
    status = 500;
    return false;
  }

  if (x.get_header_value("Content-Encoding") == "gzip") {
    out = [&](const char *buf, size_t n) {
      decompressor.decompress(
          buf, n, [&](const char *buf, size_t n) { callback(buf, n); });
    };
  }
#else
  if (x.get_header_value("Content-Encoding") == "gzip") {
    status = 415;
    return false;
  }
#endif

  auto ret = true;
  auto exceed_payload_max_length = false;

  if (is_chunked_transfer_encoding(x.headers)) {
    ret = read_content_chunked(strm, out);
  } else if (!has_header(x.headers, "Content-Length")) {
    ret = read_content_without_length(strm, out);
  } else {
    auto len = get_header_value_uint64(x.headers, "Content-Length", 0);
    if (len > 0) {
      if ((len > payload_max_length) ||
          // For 32-bit platform
          (sizeof(size_t) < sizeof(uint64_t) &&
           len > std::numeric_limits<size_t>::max())) {
        exceed_payload_max_length = true;
        skip_content_with_length(strm, len);
        ret = false;
      } else {
        ret = read_content_with_length(strm, len, progress, out);
      }
    }
  }

  if (!ret) { status = exceed_payload_max_length ? 413 : 400; }

  return ret;
}

template <typename T> inline void write_headers(Stream &strm, const T &info) {
  for (const auto &x : info.headers) {
    strm.write_format("%s: %s\r\n", x.first.c_str(), x.second.c_str());
  }
  strm.write("\r\n");
}

template <typename T>
inline void write_content_chunked(Stream &strm, const T &x) {
  auto chunked_response = !x.has_header("Content-Length");
  uint64_t offset = 0;
  auto data_available = true;
  while (data_available) {
    auto chunk = x.content_producer(offset);
    offset += chunk.size();
    data_available = !chunk.empty();

    // Emit chunked response header and footer for each chunk
    if (chunked_response) {
      chunk = from_i_to_hex(chunk.size()) + "\r\n" + chunk + "\r\n";
    }

    if (strm.write(chunk.c_str(), chunk.size()) < 0) {
      break; // Stop on error
    }
  }
}

inline std::string encode_url(const std::string &s) {
  std::string result;

  for (auto i = 0; s[i]; i++) {
    switch (s[i]) {
    case ' ': result += "%20"; break;
    case '+': result += "%2B"; break;
    case '\r': result += "%0D"; break;
    case '\n': result += "%0A"; break;
    case '\'': result += "%27"; break;
    case ',': result += "%2C"; break;
    case ':': result += "%3A"; break;
    case ';': result += "%3B"; break;
    default:
      auto c = static_cast<uint8_t>(s[i]);
      if (c >= 0x80) {
        result += '%';
        char hex[4];
        size_t len = snprintf(hex, sizeof(hex) - 1, "%02X", c);
        assert(len == 2);
        result.append(hex, len);
      } else {
        result += s[i];
      }
      break;
    }
  }

  return result;
}

inline std::string decode_url(const std::string &s) {
  std::string result;

  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 1 < s.size()) {
      if (s[i + 1] == 'u') {
        int val = 0;
        if (from_hex_to_i(s, i + 2, 4, val)) {
          // 4 digits Unicode codes
          char buff[4];
          size_t len = to_utf8(val, buff);
          if (len > 0) { result.append(buff, len); }
          i += 5; // 'u0000'
        } else {
          result += s[i];
        }
      } else {
        int val = 0;
        if (from_hex_to_i(s, i + 1, 2, val)) {
          // 2 digits hex codes
          result += val;
          i += 2; // '00'
        } else {
          result += s[i];
        }
      }
    } else if (s[i] == '+') {
      result += ' ';
    } else {
      result += s[i];
    }
  }

  return result;
}

inline void parse_query_text(const std::string &s, Params &params) {
  split(&s[0], &s[s.size()], '&', [&](const char *b, const char *e) {
    std::string key;
    std::string val;
    split(b, e, '=', [&](const char *b, const char *e) {
      if (key.empty()) {
        key.assign(b, e);
      } else {
        val.assign(b, e);
      }
    });
    params.emplace(key, decode_url(val));
  });
}

inline bool parse_multipart_boundary(const std::string &content_type,
                                     std::string &boundary) {
  auto pos = content_type.find("boundary=");
  if (pos == std::string::npos) { return false; }

  boundary = content_type.substr(pos + 9);
  return true;
}

inline bool parse_multipart_formdata(const std::string &boundary,
                                     const std::string &body,
                                     MultipartFiles &files) {
  static std::string dash = "--";
  static std::string crlf = "\r\n";

  static std::regex re_content_type("Content-Type: (.*?)",
                                    std::regex_constants::icase);

  static std::regex re_content_disposition(
      "Content-Disposition: form-data; name=\"(.*?)\"(?:; filename=\"(.*?)\")?",
      std::regex_constants::icase);

  auto dash_boundary = dash + boundary;

  auto pos = body.find(dash_boundary);
  if (pos != 0) { return false; }

  pos += dash_boundary.size();

  auto next_pos = body.find(crlf, pos);
  if (next_pos == std::string::npos) { return false; }

  pos = next_pos + crlf.size();

  while (pos < body.size()) {
    next_pos = body.find(crlf, pos);
    if (next_pos == std::string::npos) { return false; }

    std::string name;
    MultipartFile file;

    auto header = body.substr(pos, (next_pos - pos));

    while (pos != next_pos) {
      std::smatch m;
      if (std::regex_match(header, m, re_content_type)) {
        file.content_type = m[1];
      } else if (std::regex_match(header, m, re_content_disposition)) {
        name = m[1];
        file.filename = m[2];
      }

      pos = next_pos + crlf.size();

      next_pos = body.find(crlf, pos);
      if (next_pos == std::string::npos) { return false; }

      header = body.substr(pos, (next_pos - pos));
    }

    pos = next_pos + crlf.size();

    next_pos = body.find(crlf + dash_boundary, pos);

    if (next_pos == std::string::npos) { return false; }

    file.offset = pos;
    file.length = next_pos - pos;

    pos = next_pos + crlf.size() + dash_boundary.size();

    next_pos = body.find(crlf, pos);
    if (next_pos == std::string::npos) { return false; }

    files.emplace(name, file);

    pos = next_pos + crlf.size();
  }

  return true;
}

inline std::string to_lower(const char *beg, const char *end) {
  std::string out;
  auto it = beg;
  while (it != end) {
    out += ::tolower(*it);
    it++;
  }
  return out;
}

inline void make_range_header_core(std::string &) {}

template <typename uint64_t>
inline void make_range_header_core(std::string &field, uint64_t value) {
  if (!field.empty()) { field += ", "; }
  field += std::to_string(value) + "-";
}

template <typename uint64_t, typename... Args>
inline void make_range_header_core(std::string &field, uint64_t value1,
                                   uint64_t value2, Args... args) {
  if (!field.empty()) { field += ", "; }
  field += std::to_string(value1) + "-" + std::to_string(value2);
  make_range_header_core(field, args...);
}

#ifdef _WIN32
class WSInit {
public:
  WSInit() {
    WSADATA wsaData;
    WSAStartup(0x0002, &wsaData);
  }

  ~WSInit() { WSACleanup(); }
};

static WSInit wsinit_;
#endif

} // namespace detail

// Header utilities
template <typename uint64_t, typename... Args>
inline std::pair<std::string, std::string> make_range_header(uint64_t value,
                                                             Args... args) {
  std::string field;
  detail::make_range_header_core(field, value, args...);
  field.insert(0, "bytes=");
  return std::make_pair("Range", field);
}


inline std::pair<std::string, std::string> 
make_basic_authentication_header(const std::string& username, const std::string& password) {
  auto field = "Basic " + detail::base64_encode(username + ":" + password);
  return std::make_pair("Authorization", field);
}
// Request implementation
inline bool Request::has_header(const char *key) const {
  return detail::has_header(headers, key);
}

inline std::string Request::get_header_value(const char *key, size_t id) const {
  return detail::get_header_value(headers, key, id, "");
}

inline size_t Request::get_header_value_count(const char *key) const {
  auto r = headers.equal_range(key);
  return std::distance(r.first, r.second);
}

inline void Request::set_header(const char *key, const char *val) {
  headers.emplace(key, val);
}

inline bool Request::has_param(const char *key) const {
  return params.find(key) != params.end();
}

inline std::string Request::get_param_value(const char *key, size_t id) const {
  auto it = params.find(key);
  std::advance(it, id);
  if (it != params.end()) { return it->second; }
  return std::string();
}

inline size_t Request::get_param_value_count(const char *key) const {
  auto r = params.equal_range(key);
  return std::distance(r.first, r.second);
}

inline bool Request::has_file(const char *key) const {
  return files.find(key) != files.end();
}

inline MultipartFile Request::get_file_value(const char *key) const {
  auto it = files.find(key);
  if (it != files.end()) { return it->second; }
  return MultipartFile();
}

// Response implementation
inline bool Response::has_header(const char *key) const {
  return headers.find(key) != headers.end();
}

inline std::string Response::get_header_value(const char *key,
                                              size_t id) const {
  return detail::get_header_value(headers, key, id, "");
}

inline size_t Response::get_header_value_count(const char *key) const {
  auto r = headers.equal_range(key);
  return std::distance(r.first, r.second);
}

inline void Response::set_header(const char *key, const char *val) {
  headers.emplace(key, val);
}

inline void Response::set_redirect(const char *url) {
  set_header("Location", url);
  status = 302;
}

inline void Response::set_content(const char *s, size_t n,
                                  const char *content_type) {
  body.assign(s, n);
  set_header("Content-Type", content_type);
}

inline void Response::set_content(const std::string &s,
                                  const char *content_type) {
  body = s;
  set_header("Content-Type", content_type);
}

// Rstream implementation
template <typename... Args>
inline void Stream::write_format(const char *fmt, const Args &... args) {
  const auto bufsiz = 2048;
  char buf[bufsiz];

#if defined(_MSC_VER) && _MSC_VER < 1900
  auto n = _snprintf_s(buf, bufsiz, bufsiz - 1, fmt, args...);
#else
  auto n = snprintf(buf, bufsiz - 1, fmt, args...);
#endif
  if (n > 0) {
    if (n >= bufsiz - 1) {
      std::vector<char> glowable_buf(bufsiz);

      while (n >= static_cast<int>(glowable_buf.size() - 1)) {
        glowable_buf.resize(glowable_buf.size() * 2);
#if defined(_MSC_VER) && _MSC_VER < 1900
        n = _snprintf_s(&glowable_buf[0], glowable_buf.size(),
                        glowable_buf.size() - 1, fmt, args...);
#else
        n = snprintf(&glowable_buf[0], glowable_buf.size() - 1, fmt, args...);
#endif
      }
      write(&glowable_buf[0], n);
    } else {
      write(buf, n);
    }
  }
}

// Socket stream implementation
inline SocketStream::SocketStream(socket_t sock) : sock_(sock) {}

inline SocketStream::~SocketStream() {}

inline int SocketStream::read(char *ptr, size_t size) {
  if (detail::select_read(sock_, CPPHTTPLIB_READ_TIMEOUT_SECOND,
                          CPPHTTPLIB_READ_TIMEOUT_USECOND) > 0) {
    return recv(sock_, ptr, static_cast<int>(size), 0);
  }
  return -1;
}

inline int SocketStream::write(const char *ptr, size_t size) {
  return send(sock_, ptr, static_cast<int>(size), 0);
}

inline int SocketStream::write(const char *ptr) {
  return write(ptr, strlen(ptr));
}

inline std::string SocketStream::get_remote_addr() const {
  return detail::get_remote_addr(sock_);
}

// Buffer stream implementation
inline int BufferStream::read(char *ptr, size_t size) {
#if defined(_MSC_VER) && _MSC_VER < 1900
  return static_cast<int>(buffer._Copy_s(ptr, size, size));
#else
  return static_cast<int>(buffer.copy(ptr, size));
#endif
}

inline int BufferStream::write(const char *ptr, size_t size) {
  buffer.append(ptr, size);
  return static_cast<int>(size);
}

inline int BufferStream::write(const char *ptr) {
  size_t size = strlen(ptr);
  buffer.append(ptr, size);
  return static_cast<int>(size);
}

inline std::string BufferStream::get_remote_addr() const { return ""; }

inline const std::string &BufferStream::get_buffer() const { return buffer; }

// HTTP server implementation
inline Server::Server()
    : keep_alive_max_count_(CPPHTTPLIB_KEEPALIVE_MAX_COUNT),
      payload_max_length_(CPPHTTPLIB_PAYLOAD_MAX_LENGTH), is_running_(false),
      svr_sock_(INVALID_SOCKET), running_threads_(0) {
#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif
}

inline Server::~Server() {}

inline Server &Server::Get(const char *pattern, Handler handler) {
  get_handlers_.push_back(std::make_pair(std::regex(pattern), handler));
  return *this;
}

inline Server &Server::Post(const char *pattern, Handler handler) {
  post_handlers_.push_back(std::make_pair(std::regex(pattern), handler));
  return *this;
}

inline Server &Server::Put(const char *pattern, Handler handler) {
  put_handlers_.push_back(std::make_pair(std::regex(pattern), handler));
  return *this;
}

inline Server &Server::Patch(const char *pattern, Handler handler) {
  patch_handlers_.push_back(std::make_pair(std::regex(pattern), handler));
  return *this;
}

inline Server &Server::Delete(const char *pattern, Handler handler) {
  delete_handlers_.push_back(std::make_pair(std::regex(pattern), handler));
  return *this;
}

inline Server &Server::Options(const char *pattern, Handler handler) {
  options_handlers_.push_back(std::make_pair(std::regex(pattern), handler));
  return *this;
}

inline bool Server::set_base_dir(const char *path) {
  if (detail::is_dir(path)) {
    base_dir_ = path;
    return true;
  }
  return false;
}

inline void Server::set_error_handler(Handler handler) {
  error_handler_ = handler;
}

inline void Server::set_logger(Logger logger) { logger_ = logger; }

inline void Server::set_keep_alive_max_count(size_t count) {
  keep_alive_max_count_ = count;
}

inline void Server::set_payload_max_length(uint64_t length) {
  payload_max_length_ = length;
}

inline int Server::bind_to_any_port(const char *host, int socket_flags) {
  return bind_internal(host, 0, socket_flags);
}

inline bool Server::listen_after_bind() { return listen_internal(); }

inline bool Server::listen(const char *host, int port, int socket_flags) {
  if (bind_internal(host, port, socket_flags) < 0) return false;
  return listen_internal();
}

inline bool Server::is_running() const { return is_running_; }

inline void Server::stop() {
  if (is_running_) {
    assert(svr_sock_ != INVALID_SOCKET);
    std::atomic<socket_t> sock(svr_sock_.exchange(INVALID_SOCKET));
    detail::shutdown_socket(sock);
    detail::close_socket(sock);
  }
}

inline bool Server::parse_request_line(const char *s, Request &req) {
  static std::regex re("(GET|HEAD|POST|PUT|PATCH|DELETE|OPTIONS) "
                       "(([^?]+)(?:\\?(.+?))?) (HTTP/1\\.[01])\r\n");

  std::cmatch m;
  if (std::regex_match(s, m, re)) {
    req.version = std::string(m[5]);
    req.method = std::string(m[1]);
    req.target = std::string(m[2]);
    req.path = detail::decode_url(m[3]);

    // Parse query text
    auto len = std::distance(m[4].first, m[4].second);
    if (len > 0) { detail::parse_query_text(m[4], req.params); }

    return true;
  }

  return false;
}

inline void Server::write_response(Stream &strm, bool last_connection,
                                   const Request &req, Response &res) {
  assert(res.status != -1);

  if (400 <= res.status && error_handler_) { error_handler_(req, res); }

  // Response line
  strm.write_format("HTTP/1.1 %d %s\r\n", res.status,
                    detail::status_message(res.status));

  // Headers
  if (last_connection || req.get_header_value("Connection") == "close") {
    res.set_header("Connection", "close");
  }

  if (!last_connection && req.get_header_value("Connection") == "Keep-Alive") {
    res.set_header("Connection", "Keep-Alive");
  }

  if (res.body.empty()) {
    if (!res.has_header("Content-Length")) {
      if (res.content_producer) {
        // Streamed response
        res.set_header("Transfer-Encoding", "chunked");
      } else {
        res.set_header("Content-Length", "0");
      }
    }
  } else {
#ifdef CPPHTTPLIB_ZLIB_SUPPORT
    // TODO: 'Accpet-Encoding' has gzip, not gzip;q=0
    const auto &encodings = req.get_header_value("Accept-Encoding");
    if (encodings.find("gzip") != std::string::npos &&
        detail::can_compress(res.get_header_value("Content-Type"))) {
      if (detail::compress(res.body)) {
        res.set_header("Content-Encoding", "gzip");
      }
    }
#endif

    if (!res.has_header("Content-Type")) {
      res.set_header("Content-Type", "text/plain");
    }

    auto length = std::to_string(res.body.size());
    res.set_header("Content-Length", length.c_str());
  }

  detail::write_headers(strm, res);

  // Body
  if (req.method != "HEAD") {
    if (!res.body.empty()) {
      strm.write(res.body.c_str(), res.body.size());
    } else if (res.content_producer) {
      detail::write_content_chunked(strm, res);
    }
  }

  // Log
  if (logger_) { logger_(req, res); }
}

inline bool Server::handle_file_request(Request &req, Response &res) {
  if (!base_dir_.empty() && detail::is_valid_path(req.path)) {
    std::string path = base_dir_ + req.path;

    if (!path.empty() && path.back() == '/') { path += "index.html"; }

    if (detail::is_file(path)) {
      detail::read_file(path, res.body);
      auto type = detail::find_content_type(path);
      if (type) { res.set_header("Content-Type", type); }
      res.status = 200;
      return true;
    }
  }

  return false;
}

inline socket_t Server::create_server_socket(const char *host, int port,
                                             int socket_flags) const {
  return detail::create_socket(
      host, port,
      [](socket_t sock, struct addrinfo &ai) -> bool {
        if (::bind(sock, ai.ai_addr, static_cast<int>(ai.ai_addrlen))) {
          return false;
        }
        if (::listen(sock, 5)) { // Listen through 5 channels
          return false;
        }
        return true;
      },
      socket_flags);
}

inline int Server::bind_internal(const char *host, int port, int socket_flags) {
  if (!is_valid()) { return -1; }

  svr_sock_ = create_server_socket(host, port, socket_flags);
  if (svr_sock_ == INVALID_SOCKET) { return -1; }

  if (port == 0) {
    struct sockaddr_storage address;
    socklen_t len = sizeof(address);
    if (getsockname(svr_sock_, reinterpret_cast<struct sockaddr *>(&address),
                    &len) == -1) {
      return -1;
    }
    if (address.ss_family == AF_INET) {
      return ntohs(reinterpret_cast<struct sockaddr_in *>(&address)->sin_port);
    } else if (address.ss_family == AF_INET6) {
      return ntohs(
          reinterpret_cast<struct sockaddr_in6 *>(&address)->sin6_port);
    } else {
      return -1;
    }
  } else {
    return port;
  }
}

inline bool Server::listen_internal() {
  auto ret = true;

  is_running_ = true;

  for (;;) {
    if (svr_sock_ == INVALID_SOCKET) {
      // The server socket was closed by 'stop' method.
      break;
    }

    auto val = detail::select_read(svr_sock_, 0, 100000);

    if (val == 0) { // Timeout
      continue;
    }

    socket_t sock = accept(svr_sock_, nullptr, nullptr);

    if (sock == INVALID_SOCKET) {
      if (svr_sock_ != INVALID_SOCKET) {
        detail::close_socket(svr_sock_);
        ret = false;
      } else {
        ; // The server socket was closed by user.
      }
      break;
    }

    // TODO: Use thread pool...
    std::thread([=]() {
      {
        std::lock_guard<std::mutex> guard(running_threads_mutex_);
        running_threads_++;
      }

      read_and_close_socket(sock);

      {
        std::lock_guard<std::mutex> guard(running_threads_mutex_);
        running_threads_--;
      }
    }).detach();
  }

  // TODO: Use thread pool...
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::lock_guard<std::mutex> guard(running_threads_mutex_);
    if (!running_threads_) { break; }
  }

  is_running_ = false;

  return ret;
}

inline bool Server::routing(Request &req, Response &res) {
  if (req.method == "GET" && handle_file_request(req, res)) { return true; }

  if (req.method == "GET" || req.method == "HEAD") {
    return dispatch_request(req, res, get_handlers_);
  } else if (req.method == "POST") {
    return dispatch_request(req, res, post_handlers_);
  } else if (req.method == "PUT") {
    return dispatch_request(req, res, put_handlers_);
  } else if (req.method == "PATCH") {
    return dispatch_request(req, res, patch_handlers_);
  } else if (req.method == "DELETE") {
    return dispatch_request(req, res, delete_handlers_);
  } else if (req.method == "OPTIONS") {
    return dispatch_request(req, res, options_handlers_);
  }
  return false;
}

inline bool Server::dispatch_request(Request &req, Response &res,
                                     Handlers &handlers) {
  for (const auto &x : handlers) {
    const auto &pattern = x.first;
    const auto &handler = x.second;

    if (std::regex_match(req.path, req.matches, pattern)) {
      handler(req, res);
      return true;
    }
  }
  return false;
}

inline bool
Server::process_request(Stream &strm, bool last_connection,
                        bool &connection_close,
                        std::function<void(Request &)> setup_request) {
  const auto bufsiz = 2048;
  char buf[bufsiz];

  detail::stream_line_reader reader(strm, buf, bufsiz);

  // Connection has been closed on client
  if (!reader.getline()) { return false; }

  Request req;
  Response res;

  res.version = "HTTP/1.1";

  // Check if the request URI doesn't exceed the limit
  if (reader.size() > CPPHTTPLIB_REQUEST_URI_MAX_LENGTH) {
    res.status = 414;
    write_response(strm, last_connection, req, res);
    return true;
  }

  // Request line and headers
  if (!parse_request_line(reader.ptr(), req) ||
      !detail::read_headers(strm, req.headers)) {
    res.status = 400;
    write_response(strm, last_connection, req, res);
    return true;
  }

  if (req.get_header_value("Connection") == "close") {
    connection_close = true;
  }

  req.set_header("REMOTE_ADDR", strm.get_remote_addr().c_str());

  // Body
  if (req.method == "POST" || req.method == "PUT" || req.method == "PATCH") {
    if (!detail::read_content(
            strm, req, payload_max_length_, res.status, Progress(),
            [&](const char *buf, size_t n) { req.body.append(buf, n); })) {
      write_response(strm, last_connection, req, res);
      return true;
    }

    const auto &content_type = req.get_header_value("Content-Type");

    if (!content_type.find("application/x-www-form-urlencoded")) {
      detail::parse_query_text(req.body, req.params);
    } else if (!content_type.find("multipart/form-data")) {
      std::string boundary;
      if (!detail::parse_multipart_boundary(content_type, boundary) ||
          !detail::parse_multipart_formdata(boundary, req.body, req.files)) {
        res.status = 400;
        write_response(strm, last_connection, req, res);
        return true;
      }
    }
  }

  // TODO: Add additional request info
  if (setup_request) { setup_request(req); }

  if (routing(req, res)) {
    if (res.status == -1) { res.status = 200; }
  } else {
    res.status = 404;
  }

  write_response(strm, last_connection, req, res);
  return true;
}

inline bool Server::is_valid() const { return true; }

inline bool Server::read_and_close_socket(socket_t sock) {
  return detail::read_and_close_socket(
      sock, keep_alive_max_count_,
      [this](Stream &strm, bool last_connection, bool &connection_close) {
        return process_request(strm, last_connection, connection_close);
      });
}

// HTTP client implementation
inline Client::Client(const char *host, int port, time_t timeout_sec)
    : host_(host), port_(port), timeout_sec_(timeout_sec),
      host_and_port_(host_ + ":" + std::to_string(port_)) {}

inline Client::~Client() {}

inline bool Client::is_valid() const { return true; }

inline socket_t Client::create_client_socket() const {
  return detail::create_socket(
      host_.c_str(), port_, [=](socket_t sock, struct addrinfo &ai) -> bool {
        detail::set_nonblocking(sock, true);

        auto ret = connect(sock, ai.ai_addr, static_cast<int>(ai.ai_addrlen));
        if (ret < 0) {
          if (detail::is_connection_error() ||
              !detail::wait_until_socket_is_ready(sock, timeout_sec_, 0)) {
            detail::close_socket(sock);
            return false;
          }
        }

        detail::set_nonblocking(sock, false);
        return true;
      });
}

inline bool Client::read_response_line(Stream &strm, Response &res) {
  const auto bufsiz = 2048;
  char buf[bufsiz];

  detail::stream_line_reader reader(strm, buf, bufsiz);

  if (!reader.getline()) { return false; }

  const static std::regex re("(HTTP/1\\.[01]) (\\d+?) .*\r\n");

  std::cmatch m;
  if (std::regex_match(reader.ptr(), m, re)) {
    res.version = std::string(m[1]);
    res.status = std::stoi(std::string(m[2]));
  }

  return true;
}

inline bool Client::send(Request &req, Response &res) {
  if (req.path.empty()) { return false; }

  auto sock = create_client_socket();
  if (sock == INVALID_SOCKET) { return false; }

  return read_and_close_socket(sock, req, res);
}

inline void Client::write_request(Stream &strm, Request &req) {
  BufferStream bstrm;

  // Request line
  auto path = detail::encode_url(req.path);

  bstrm.write_format("%s %s HTTP/1.1\r\n", req.method.c_str(), path.c_str());

  // Headers
  if (!req.has_header("Host")) {
    if (is_ssl()) {
      if (port_ == 443) {
        req.set_header("Host", host_.c_str());
      } else {
        req.set_header("Host", host_and_port_.c_str());
      }
    } else {
      if (port_ == 80) {
        req.set_header("Host", host_.c_str());
      } else {
        req.set_header("Host", host_and_port_.c_str());
      }
    }
  }

  if (!req.has_header("Accept")) { req.set_header("Accept", "*/*"); }

  if (!req.has_header("User-Agent")) {
    req.set_header("User-Agent", "cpp-httplib/0.2");
  }

  // TODO: Support KeepAlive connection
  // if (!req.has_header("Connection")) {
  req.set_header("Connection", "close");
  // }

  if (req.body.empty()) {
    if (req.method == "POST" || req.method == "PUT" || req.method == "PATCH") {
      req.set_header("Content-Length", "0");
    }
  } else {
    if (!req.has_header("Content-Type")) {
      req.set_header("Content-Type", "text/plain");
    }

    if (!req.has_header("Content-Length")) {
      auto length = std::to_string(req.body.size());
      req.set_header("Content-Length", length.c_str());
    }
  }

  detail::write_headers(bstrm, req);

  // Body
  if (!req.body.empty()) { bstrm.write(req.body.c_str(), req.body.size()); }

  // Flush buffer
  auto &data = bstrm.get_buffer();
  strm.write(data.data(), data.size());
}

inline bool Client::process_request(Stream &strm, Request &req, Response &res,
                                    bool &connection_close) {
  // Send request
  write_request(strm, req);

  // Receive response and headers
  if (!read_response_line(strm, res) ||
      !detail::read_headers(strm, res.headers)) {
    return false;
  }

  if (res.get_header_value("Connection") == "close" ||
      res.version == "HTTP/1.0") {
    connection_close = true;
  }

  // Body
  if (req.method != "HEAD") {
    ContentReceiver out = [&](const char *buf, size_t n) {
      res.body.append(buf, n);
    };

    if (res.content_receiver) {
      out = [&](const char *buf, size_t n) { res.content_receiver(buf, n); };
    }

    int dummy_status;
    if (!detail::read_content(strm, res, std::numeric_limits<uint64_t>::max(),
                              dummy_status, res.progress, out)) {
      return false;
    }
  }

  return true;
}

inline bool Client::read_and_close_socket(socket_t sock, Request &req,
                                          Response &res) {
  return detail::read_and_close_socket(
      sock, 0,
      [&](Stream &strm, bool /*last_connection*/, bool &connection_close) {
        return process_request(strm, req, res, connection_close);
      });
}

inline bool Client::is_ssl() const { return false; }

inline std::shared_ptr<Response> Client::Get(const char *path,
                                             Progress progress) {
  return Get(path, Headers(), progress);
}

inline std::shared_ptr<Response>
Client::Get(const char *path, const Headers &headers, Progress progress) {
  Request req;
  req.method = "GET";
  req.path = path;
  req.headers = headers;

  auto res = std::make_shared<Response>();
  res->progress = progress;

  return send(req, *res) ? res : nullptr;
}

inline std::shared_ptr<Response> Client::Get(const char *path,
                                             ContentReceiver content_receiver,
                                             Progress progress) {
  return Get(path, Headers(), content_receiver, progress);
}

inline std::shared_ptr<Response> Client::Get(const char *path,
                                             const Headers &headers,
                                             ContentReceiver content_receiver,
                                             Progress progress) {
  Request req;
  req.method = "GET";
  req.path = path;
  req.headers = headers;

  auto res = std::make_shared<Response>();
  res->content_receiver = content_receiver;
  res->progress = progress;

  return send(req, *res) ? res : nullptr;
}

inline std::shared_ptr<Response> Client::Head(const char *path) {
  return Head(path, Headers());
}

inline std::shared_ptr<Response> Client::Head(const char *path,
                                              const Headers &headers) {
  Request req;
  req.method = "HEAD";
  req.headers = headers;
  req.path = path;

  auto res = std::make_shared<Response>();

  return send(req, *res) ? res : nullptr;
}

inline std::shared_ptr<Response> Client::Post(const char *path,
                                              const std::string &body,
                                              const char *content_type) {
  return Post(path, Headers(), body, content_type);
}

inline std::shared_ptr<Response> Client::Post(const char *path,
                                              const Headers &headers,
                                              const std::string &body,
                                              const char *content_type) {
  Request req;
  req.method = "POST";
  req.headers = headers;
  req.path = path;

  req.headers.emplace("Content-Type", content_type);
  req.body = body;

  auto res = std::make_shared<Response>();

  return send(req, *res) ? res : nullptr;
}

inline std::shared_ptr<Response> Client::Post(const char *path,
                                              const Params &params) {
  return Post(path, Headers(), params);
}

inline std::shared_ptr<Response>
Client::Post(const char *path, const Headers &headers, const Params &params) {
  std::string query;
  for (auto it = params.begin(); it != params.end(); ++it) {
    if (it != params.begin()) { query += "&"; }
    query += it->first;
    query += "=";
    query += detail::encode_url(it->second);
  }

  return Post(path, headers, query, "application/x-www-form-urlencoded");
}

inline std::shared_ptr<Response> Client::Put(const char *path,
                                             const std::string &body,
                                             const char *content_type) {
  return Put(path, Headers(), body, content_type);
}

inline std::shared_ptr<Response> Client::Put(const char *path,
                                             const Headers &headers,
                                             const std::string &body,
                                             const char *content_type) {
  Request req;
  req.method = "PUT";
  req.headers = headers;
  req.path = path;

  req.headers.emplace("Content-Type", content_type);
  req.body = body;

  auto res = std::make_shared<Response>();

  return send(req, *res) ? res : nullptr;
}

inline std::shared_ptr<Response> Client::Patch(const char *path,
                                               const std::string &body,
                                               const char *content_type) {
  return Patch(path, Headers(), body, content_type);
}

inline std::shared_ptr<Response> Client::Patch(const char *path,
                                               const Headers &headers,
                                               const std::string &body,
                                               const char *content_type) {
  Request req;
  req.method = "PATCH";
  req.headers = headers;
  req.path = path;

  req.headers.emplace("Content-Type", content_type);
  req.body = body;

  auto res = std::make_shared<Response>();

  return send(req, *res) ? res : nullptr;
}

inline std::shared_ptr<Response> Client::Delete(const char *path,
                                                const std::string &body,
                                                const char *content_type) {
  return Delete(path, Headers(), body, content_type);
}

inline std::shared_ptr<Response> Client::Delete(const char *path,
                                                const Headers &headers,
                                                const std::string &body,
                                                const char *content_type) {
  Request req;
  req.method = "DELETE";
  req.headers = headers;
  req.path = path;

  if (content_type) { req.headers.emplace("Content-Type", content_type); }
  req.body = body;

  auto res = std::make_shared<Response>();

  return send(req, *res) ? res : nullptr;
}

inline std::shared_ptr<Response> Client::Options(const char *path) {
  return Options(path, Headers());
}

inline std::shared_ptr<Response> Client::Options(const char *path,
                                                 const Headers &headers) {
  Request req;
  req.method = "OPTIONS";
  req.path = path;
  req.headers = headers;

  auto res = std::make_shared<Response>();

  return send(req, *res) ? res : nullptr;
}

/*
 * SSL Implementation
 */
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
namespace detail {

template <typename U, typename V, typename T>
inline bool
read_and_close_socket_ssl(socket_t sock, size_t keep_alive_max_count,
                          // TODO: OpenSSL 1.0.2 occasionally crashes...
                          // The upcoming 1.1.0 is going to be thread safe.
                          SSL_CTX *ctx, std::mutex &ctx_mutex,
                          U SSL_connect_or_accept, V setup, T callback) {
  SSL *ssl = nullptr;
  {
    std::lock_guard<std::mutex> guard(ctx_mutex);
    ssl = SSL_new(ctx);
  }

  if (!ssl) {
    close_socket(sock);
    return false;
  }

  auto bio = BIO_new_socket(sock, BIO_NOCLOSE);
  SSL_set_bio(ssl, bio, bio);

  if (!setup(ssl)) {
    SSL_shutdown(ssl);
    {
      std::lock_guard<std::mutex> guard(ctx_mutex);
      SSL_free(ssl);
    }

    close_socket(sock);
    return false;
  }

  bool ret = false;

  if (SSL_connect_or_accept(ssl) == 1) {
    if (keep_alive_max_count > 0) {
      auto count = keep_alive_max_count;
      while (count > 0 &&
             detail::select_read(sock, CPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND,
                                 CPPHTTPLIB_KEEPALIVE_TIMEOUT_USECOND) > 0) {
        SSLSocketStream strm(sock, ssl);
        auto last_connection = count == 1;
        auto connection_close = false;

        ret = callback(ssl, strm, last_connection, connection_close);
        if (!ret || connection_close) { break; }

        count--;
      }
    } else {
      SSLSocketStream strm(sock, ssl);
      auto dummy_connection_close = false;
      ret = callback(ssl, strm, true, dummy_connection_close);
    }
  }

  SSL_shutdown(ssl);
  {
    std::lock_guard<std::mutex> guard(ctx_mutex);
    SSL_free(ssl);
  }

  close_socket(sock);

  return ret;
}

class SSLInit {
public:
  SSLInit() {
    SSL_load_error_strings();
    SSL_library_init();
  }

  ~SSLInit() { ERR_free_strings(); }
};

static SSLInit sslinit_;

} // namespace detail

// SSL socket stream implementation
inline SSLSocketStream::SSLSocketStream(socket_t sock, SSL *ssl)
    : sock_(sock), ssl_(ssl) {}

inline SSLSocketStream::~SSLSocketStream() {}

inline int SSLSocketStream::read(char *ptr, size_t size) {
  if (SSL_pending(ssl_) > 0 ||
      detail::select_read(sock_, CPPHTTPLIB_READ_TIMEOUT_SECOND,
                          CPPHTTPLIB_READ_TIMEOUT_USECOND) > 0) {
    return SSL_read(ssl_, ptr, size);
  }
  return -1;
}

inline int SSLSocketStream::write(const char *ptr, size_t size) {
  return SSL_write(ssl_, ptr, size);
}

inline int SSLSocketStream::write(const char *ptr) {
  return write(ptr, strlen(ptr));
}

inline std::string SSLSocketStream::get_remote_addr() const {
  return detail::get_remote_addr(sock_);
}

// SSL HTTP server implementation
inline SSLServer::SSLServer(const char *cert_path, const char *private_key_path,
                            const char *client_ca_cert_file_path,
                            const char *client_ca_cert_dir_path) {
  ctx_ = SSL_CTX_new(SSLv23_server_method());

  if (ctx_) {
    SSL_CTX_set_options(ctx_,
                        SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                            SSL_OP_NO_COMPRESSION |
                            SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

    // auto ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    // SSL_CTX_set_tmp_ecdh(ctx_, ecdh);
    // EC_KEY_free(ecdh);

    if (SSL_CTX_use_certificate_chain_file(ctx_, cert_path) != 1 ||
        SSL_CTX_use_PrivateKey_file(ctx_, private_key_path, SSL_FILETYPE_PEM) !=
            1) {
      SSL_CTX_free(ctx_);
      ctx_ = nullptr;
    } else if (client_ca_cert_file_path || client_ca_cert_dir_path) {
      // if (client_ca_cert_file_path) {
      //   auto list = SSL_load_client_CA_file(client_ca_cert_file_path);
      //   SSL_CTX_set_client_CA_list(ctx_, list);
      // }

      SSL_CTX_load_verify_locations(ctx_, client_ca_cert_file_path,
                                    client_ca_cert_dir_path);

      SSL_CTX_set_verify(
          ctx_,
          SSL_VERIFY_PEER |
              SSL_VERIFY_FAIL_IF_NO_PEER_CERT, // SSL_VERIFY_CLIENT_ONCE,
          nullptr);
    }
  }
}

inline SSLServer::~SSLServer() {
  if (ctx_) { SSL_CTX_free(ctx_); }
}

inline bool SSLServer::is_valid() const { return ctx_; }

inline bool SSLServer::read_and_close_socket(socket_t sock) {
  return detail::read_and_close_socket_ssl(
      sock, keep_alive_max_count_, ctx_, ctx_mutex_, SSL_accept,
      [](SSL * /*ssl*/) { return true; },
      [this](SSL *ssl, Stream &strm, bool last_connection,
             bool &connection_close) {
        return process_request(strm, last_connection, connection_close,
                               [&](Request &req) { req.ssl = ssl; });
      });
}

// SSL HTTP client implementation
inline SSLClient::SSLClient(const char *host, int port, time_t timeout_sec,
                            const char *client_cert_path,
                            const char *client_key_path)
    : Client(host, port, timeout_sec) {
  ctx_ = SSL_CTX_new(SSLv23_client_method());

  detail::split(&host_[0], &host_[host_.size()], '.',
                [&](const char *b, const char *e) {
                  host_components_.emplace_back(std::string(b, e));
                });
  if (client_cert_path && client_key_path) {
    if (SSL_CTX_use_certificate_file(ctx_, client_cert_path,
                                     SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_use_PrivateKey_file(ctx_, client_key_path, SSL_FILETYPE_PEM) !=
            1) {
      SSL_CTX_free(ctx_);
      ctx_ = nullptr;
    }
  }
}

inline SSLClient::~SSLClient() {
  if (ctx_) { SSL_CTX_free(ctx_); }
}

inline bool SSLClient::is_valid() const { return ctx_; }

inline void SSLClient::set_ca_cert_path(const char *ca_cert_file_path,
                                        const char *ca_cert_dir_path) {
  if (ca_cert_file_path) { ca_cert_file_path_ = ca_cert_file_path; }
  if (ca_cert_dir_path) { ca_cert_dir_path_ = ca_cert_dir_path; }
}

inline void SSLClient::enable_server_certificate_verification(bool enabled) {
  server_certificate_verification_ = enabled;
}

inline long SSLClient::get_openssl_verify_result() const {
  return verify_result_;
}

inline bool SSLClient::read_and_close_socket(socket_t sock, Request &req,
                                             Response &res) {

  return is_valid() &&
         detail::read_and_close_socket_ssl(
             sock, 0, ctx_, ctx_mutex_,
             [&](SSL *ssl) {
               if (ca_cert_file_path_.empty()) {
                 SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
               } else {
                 if (!SSL_CTX_load_verify_locations(
                         ctx_, ca_cert_file_path_.c_str(), nullptr)) {
                   return false;
                 }
                 SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
               }

               if (SSL_connect(ssl) != 1) { return false; }

               if (server_certificate_verification_) {
                 verify_result_ = SSL_get_verify_result(ssl);

                 if (verify_result_ != X509_V_OK) { return false; }

                 auto server_cert = SSL_get_peer_certificate(ssl);

                 if (server_cert == nullptr) { return false; }

                 if (!verify_host(server_cert)) {
                   X509_free(server_cert);
                   return false;
                 }
                 X509_free(server_cert);
               }

               return true;
             },
             [&](SSL *ssl) {
               SSL_set_tlsext_host_name(ssl, host_.c_str());
               return true;
             },
             [&](SSL * /*ssl*/, Stream &strm, bool /*last_connection*/,
                 bool &connection_close) {
               return process_request(strm, req, res, connection_close);
             });
}

inline bool SSLClient::is_ssl() const { return true; }

inline bool SSLClient::verify_host(X509 *server_cert) const {
  /* Quote from RFC2818 section 3.1 "Server Identity"

     If a subjectAltName extension of type dNSName is present, that MUST
     be used as the identity. Otherwise, the (most specific) Common Name
     field in the Subject field of the certificate MUST be used. Although
     the use of the Common Name is existing practice, it is deprecated and
     Certification Authorities are encouraged to use the dNSName instead.

     Matching is performed using the matching rules specified by
     [RFC2459].  If more than one identity of a given type is present in
     the certificate (e.g., more than one dNSName name, a match in any one
     of the set is considered acceptable.) Names may contain the wildcard
     character * which is considered to match any single domain name
     component or component fragment. E.g., *.a.com matches foo.a.com but
     not bar.foo.a.com. f*.com matches foo.com but not bar.com.

     In some cases, the URI is specified as an IP address rather than a
     hostname. In this case, the iPAddress subjectAltName must be present
     in the certificate and must exactly match the IP in the URI.

  */
  return verify_host_with_subject_alt_name(server_cert) ||
         verify_host_with_common_name(server_cert);
}

inline bool
SSLClient::verify_host_with_subject_alt_name(X509 *server_cert) const {
  auto ret = false;

  auto type = GEN_DNS;

  struct in6_addr addr6;
  struct in_addr addr;
  size_t addr_len = 0;

  if (inet_pton(AF_INET6, host_.c_str(), &addr6)) {
    type = GEN_IPADD;
    addr_len = sizeof(struct in6_addr);
  } else if (inet_pton(AF_INET, host_.c_str(), &addr)) {
    type = GEN_IPADD;
    addr_len = sizeof(struct in_addr);
  }

  auto alt_names = static_cast<const struct stack_st_GENERAL_NAME *>(
      X509_get_ext_d2i(server_cert, NID_subject_alt_name, nullptr, nullptr));

  if (alt_names) {
    auto dsn_matched = false;
    auto ip_mached = false;

    auto count = sk_GENERAL_NAME_num(alt_names);

    for (auto i = 0; i < count && !dsn_matched; i++) {
      auto val = sk_GENERAL_NAME_value(alt_names, i);
      if (val->type == type) {
        auto name = (const char *)ASN1_STRING_get0_data(val->d.ia5);
        auto name_len = (size_t)ASN1_STRING_length(val->d.ia5);

        if (strlen(name) == name_len) {
          switch (type) {
          case GEN_DNS: dsn_matched = check_host_name(name, name_len); break;

          case GEN_IPADD:
            if (!memcmp(&addr6, name, addr_len) ||
                !memcmp(&addr, name, addr_len)) {
              ip_mached = true;
            }
            break;
          }
        }
      }
    }

    if (dsn_matched || ip_mached) { ret = true; }
  }

  GENERAL_NAMES_free((STACK_OF(GENERAL_NAME) *)alt_names);

  return ret;
}

inline bool SSLClient::verify_host_with_common_name(X509 *server_cert) const {
  const auto subject_name = X509_get_subject_name(server_cert);

  if (subject_name != nullptr) {
    char name[BUFSIZ];
    auto name_len = X509_NAME_get_text_by_NID(subject_name, NID_commonName,
                                              name, sizeof(name));

    if (name_len != -1) { return check_host_name(name, name_len); }
  }

  return false;
}

inline bool SSLClient::check_host_name(const char *pattern,
                                       size_t pattern_len) const {
  if (host_.size() == pattern_len && host_ == pattern) { return true; }

  // Wildcard match
  // https://bugs.launchpad.net/ubuntu/+source/firefox-3.0/+bug/376484
  std::vector<std::string> pattern_components;
  detail::split(&pattern[0], &pattern[pattern_len], '.',
                [&](const char *b, const char *e) {
                  pattern_components.emplace_back(std::string(b, e));
                });

  if (host_components_.size() != pattern_components.size()) { return false; }

  auto itr = pattern_components.begin();
  for (const auto &h : host_components_) {
    auto &p = *itr;
    if (p != h && p != "*") {
      auto partial_match = (p.size() > 0 && p[p.size() - 1] == '*' &&
                            !p.compare(0, p.size() - 1, h));
      if (!partial_match) { return false; }
    }
    ++itr;
  }

  return true;
}
#endif

} // namespace httplib

#endif // CPPHTTPLIB_HTTPLIB_H
