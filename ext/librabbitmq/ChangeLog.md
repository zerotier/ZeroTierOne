# Change Log
## v0.9.0 - 2018-05-08
### Added:
- amqp-publish: added support for specifying headers via the -H flag
- Add support for specifying timeout for amqp_login calls via
  amqp_set_handshake_timeout
- Add support for specifying timeouts in RPC-style AMQP methods via
  amqp_set_rpc_timeout
- Add define for `AMQP_DEFAULT_VHOST`
- Support for SSL SNI
- Support for OpenSSL v1.1.0

### Changed:
- rabbitmq-c now requires Windows Vista or better
- rabbitmq-c enables TCP keep-alive by default on platforms that support it
- dropped support for compiling rabbitmq-c without threading support
- OpenSSL is no longer un-intialized automatically by default. OpenSSL can be
  explicitly initialized by calling amqp_initialize_ssl_library and
  uninitialized by calling amqp_uninitialize_ssl_library.

### Fixed:
- Correct bugs in processing of --url flag in tools (#364).
- Improve documentation on AMQP_SASL_METHOD_EXTERNAL (#349)
- Improve support for compiling under mingw-w64
- Better support for handing SIGPIPE on Linux over SSL (#401)
- Improve publish performance on Linux by not specifying MSG_MORE on last part
  of message.
- Fix connection logic where multiple hostnames won't be tried if connection to
  doesn't fail immediately (#430)

### Removed:
- autotools build system has been removed
- many duplicate amqps_* examples, they did not add a lot of value


## v0.8.0 - 2016-04-09
### Added:
- SSL: peer certificate and hostname validation can now be controlled separately
  using `amqp_ssl_socket_set_verify_peer` and
  `amqp_ssl_socket_set_verify_hostname`.
- SSL: the desire SSL version range can now be specified using the
  `amqp_ssl_socket_set_ssl_versions` function.
- Add flags to SSL examples on controlling hostname verification.

### Changed:
- SSL: SSLv2, and SSLv3 have been disabled by default.
- SSL: OpenSSL hostname validation has been improved.
- Win32 debug information is built with /Z7 on MSVC to embed debug info instead
  of using a .pdb

### Fixed:
- Connection failure results in hang on Win32 (#297, #346)
- Rabbitmq-c may block when attempting to close an SSL socket (#313)
- amqp_parse_url does not correctly initialize default parameters (#319)
- x509 objects are leaked in verify_hostname (#323)
- TCP_NOPUSH doesn't work under cygwin (#335)

### Deprecated
- SSL: `amqp_ssl_socket_set_verify` is being replaced by
  `amqp_ssl_socket_set_verify_peer` and `amqp_ssl_socket_set_verify_hostname`.

### Removed:
- OpenVMS build system and related files.
- Unmaintained PolarSSL, CyaSSL, and gnuTLS SSL backends

## Changes since v0.7.0 (a.k.a., v0.7.1)
- `41fa9df` Autoconf: add missing files in build system
- `ef73c06` Win32: Use WSAEWOULDBLOCK instead of EWOULDBLOCK on Win32
- `ceca348` CI: use travis-ci container based builds
- `393e2df` Lib: if channel_max is 0 use server's channel_max
- `ff47574` Lib: fix build on OpenBSD
- `8429496...0ac6430` CI: enable CI on Mac OS X in travis-ci

## Changes since v0.6.0 (a.k.a., v0.7.0)
- `3379812` Tools: Add support for heartbeats
- `d7029db` CI: Add continuous integration on Win32 using Appveyor
- `a5f7ffb` Tests: only link against static libraries
- `a16ad45...9cf7a3b`  Lib: add support for EXTERNAL SASL method
- `038a9ed` Lib: fix incorrect parameters to WSAPoll on Win32
- `a240c69...14ae307` Lib: use non-blocking sockets internally
- `8d1d5cc`, `5498dc6` Lib: simplify timer/timeout logic
- `61fc4e1` Lib: add support for heartbeat checks in blocking send calls
- `f462c0f...3546a70` Lib: Fix warnings on Win32
- `ba9d8ba...112a54d` Lib: Add support for RabbitMQ auth failure extension
- `fb8e318` Lib: allow calling functions to override client-properties
- `3ef3f5f` examples: replace usleep() with nanosleep()
- `9027a94` Lib: add AMQP_VERSION code
- `9ee1718` Lib: fix res maybe returned uninitialized in amqp_merge_capbilities
- `22a36db` Lib: Fix SSL_connection status check
- `abbefd4` Lib: Fix issues with c89 compatiblity
- `2bc1f9b...816cbfc` Lib: perf improvements when sending small messages by
  hinting to the OS message boundaries.
- `be2e6dd...784a0e9` Lib: add select()-based timeout implementation
- `91db548...8d77b4c` CI: add ubsan, asan, and tsan CI builds

## Changes since v0.5.2 (a.k.a., v0.6.0)
- `e1746f9` Tools: Enable support for SSL in tools.
- `9626dd5` Lib: ABI CHANGE: enable support for auto_delete, internal flags to
     amqp_exchange_declare
- `ee54e27`, `656f833` Lib: check for double-close in SSL/TCP socket impl
- `cf2760d` Lib: allocate struct when method has no field.
- `513ad4a` Lib: add support for SANs in OpenSSL socket impl.
- `5348c69` Lib: add functions to get negotiated frame_max and heartbeat parms.

## Changes since v0.5.1 (a.k.a., v0.5.2)
- `fcdf0f8` Autoconf: check for htonll as declaration in a header file
- `5790ec7` SSL: correctly report hostname verification errors.
- `d60c28c` Build: disable OpenSSL deprecation warnings on OSX
- `072191a` Lib: include platform, version and copyright in AMQP handshake
- `8b448c6` Examples: print message body in amqp[s]_listen[q] examples
- `7188e5d` Tools: Add flag to set prefetch for amqp-consume tool

## Changes since v0.5.0 (a.k.a., v0.5.1)
### Enhancements:
- `a566929` SSL: Add support for wildcards in hostname verification (Mike
  Steinert)
- `a78aa8a` Lib: Use poll(2) instead of select(2) for timeouts on sockets.
- `357bdb3` Lib: support for specifying frame and decoding pool sizes. (Mike
  Stitt)
- `8956003` Lib: improve invalid frame detection code.

### Bug fixes:
- `b852f84` Lib: Add missing amqp_get_server_properties() function.
- `7001e82` Lib: Add missing ssize_t on Win32 (emazv72)
- `c2ce2cb` Lib: Correctly specify WINVER on Win32 when unspecified.
- `fe844e4` CMake: specify -DHAVE_CONFIG_H in examples.
- `932de5f` Lib: correct time computation on Win32 (jestor)
- `3e83192` HPUX: use gethrtime on HP-UX for timers.
- `cb1b44e` HPUX: correct include location of sys/uio.h
- `8ce585d` Lib: incorrect OOM condition when 0-lenth exchange name is received.
- `c7716b8` CMake: correct htonll detection code on platforms defined with a
  macro.
- `4dc4eda` Lib: remove unused assignment.
- `45302cf` Lib: remove range-check of channel-ids.


## Changes since v0.4.1 (a.k.a., v0.5.0):
### Major changes:
- Add amqp_get_broker_properties() function 5c7c40adc1
- Remove distro-specific packaging a5749657ee
- Add -x flag to amqp-consume utilty 1d9c5291ff
- Add amqp_basic_nack() public API 9b168776fb
- Add delivery mode constants to amqp.h 5f291ea772
- Add support for connection.blocked/connection.unblocked methods ccbc24d270

### Bug fixes:
- `f8c6cee749` Examples: Destroy amqp_envelope_t in consumer example
- `ac88db56d3` CMake: fix generation of librabbitmq.pc
- `d5b35afa40` CMake: fix missing POPT_INCLUDE_DIRS variable in tools/
- `5ea6a0945a` build: provide independent locations for x64 libs
- `fac34656c0` Doc: documentation fixes
- `715901d675` Lib: Correct OpenSSL initialization under threaded conditions
- `ce64e57df8` Examples: Handle unexpected frames in amqp_consumer.c
- `bcda3e933d` CMake: Use GnuInstallDirs to generate install dirs
- `27245a4e15` Lib: correctly handle amqp_get_monotonic_timestamp on win32
- `693662ef5b` Tools: honor --persistent flag in publish utility
- `01d9c3ca60` Doc: improve documentation in amqp_ssl_socket functions
- `02d5c58ae4` autoconf: correct librabbitmq.pc generation
- `1f4e0cc48b` Doc: improve documentation in amqp_tcp_socket functions

## Changes since v0.4.0:
### Major changes:
- Removed distro-specific packaging d285d01

### Bug fixes:
- `a642602` FIX: destroy amqp_envelop_t object in consumer example
- `860dd71` FIX: correct generation of librabbitmq.pc under CMake
- `bdda7ab` FIX: amqp_socket_close() should not be exported from shlib
- `24f4131` FIX: Use correct buf/len vars when re-starting send()

## Changes since v0.3.0:
### New Features/Enhancements:
- `amqp_login_with_properties()` function to connect to a broker sending a
   properties table to the broker 21b124e #101
- SSL support (Mike Steinert) 473c865 #17
- `amqp_simple_wait_frame_noblock()` function variant to wait for a frame
   with a timeout f8cfc72 #119
- Allow memory to be released on a per-channel basis with
   `amqp_maybe_release_buffers_on_channel()` 4a2d899 #5
- Support for AMQP heartbeats while blocking in `amqp_simple_wait_frame*()`
   and `amqp_basic_publish()` daa0e66 aca5dc1
- `amqp_socket_open_noblock()` for a non-blocking socket connection
   (Bogdan Padalko) 6ad770d
- `amqp_table_clone()` to do a deep-copy of an amqp_table_t 08af83a
- Add option to listen to multiple keys in `amqp_consume` tool (Brian Hammond) e6c256d
- Add contributed OpenVMS build system 448ab68
- Higher level APIs for consuming messages 33ebeed #8
- Doxygen-based API documentation.
- Many improvements to error-handling and reporting

### Bug Fixes:
- `24ffaf8` FIX: autotools was broken when dependency-tracking was disabled
- `38e741b` FIX: CMake XmlTo not found warning
- `906f04f` FIX: htonll redeclared on Win32 v8
- `8e41603` FIX: SIGPIPE not disabled on OS X/BSD #102
- `872ea49` FIX: Header issues with amqp.h on Mingw on Win32 (yoniyoni)
- `0f1f75b` FIX: potential memory leak in amqp_new_connection
- `c9f6312` FIX: missing va_end in `amqp_login()`/`amqp_login_with_properties()`
- `7bb64e4` FIX: include amqp_tcp_socket.h in dpkg (Tim Stewart)
- `ba9d1f5` FIX: Report out of buffer space in `amqp_table_encode()`
- `9496e10` FIX: Remove `abort()` on invalid parameter in `amqp_send_frame()`
- `f209420` FIX: Remote `abort()` in `amqp_simple_wait_method()`
- `f027518` FIX: Return error on socket lib init error
- `0ae534a` FIX: Correctly handle 0 return val from `SSL_read()`/`SSL_write()`
- `22e41b8` FIX: Improve error handling in socket functions
- `33c2353` FIX: Set state->socket to NULL after `amqp_socket_close()`
- `c83e728` FIX: Incorrect error code returned
- `1a19879` FIX: redecl of int i in `amqp_tcp_socket_writev()`
- `7477449` FIX: incorrect bit-shift in `amqp_error_string2()`
- `2e37bb3` FIX: correctly handle `amqp_get_sockfd()` in `amqp_simple_wait_frame()`
- `52a459b` FIX: Don't delete state in `amqp_tune_connection()` on error
- `01e38dd` FIX: Correctly handle `mach_timebase_info()` failure
- `34bffb7` FIX: Correctly disable `SIGPIPE` on platforms with `SO_NOSIGPIPE`
- `3866433` FIX: Use correct number of bits in timer precision on MacOSX
- `b6a1dfe` FIX: Squash OpenSSL deprecated warnings on MacOSX (Bogdan Padalko)
- `7a217d5` FIX: Incorrect `assert()` in `wait_frame_inner()`
- `7942af3` FIX: Correctly handle 0-length table in `amqp_table_clone()`
- `157788e` FIX: Correctly handle 0-length strings in `amqp_basic_properties_clone()`
- `4eaf771` FIX: Correctly handle 0-length message body in `amqp_read_message()`
- `59f943b` FIX: Double-free SSL on connection failure
- `7a451a4` FIX: `amqp_open_socket()` not defined
