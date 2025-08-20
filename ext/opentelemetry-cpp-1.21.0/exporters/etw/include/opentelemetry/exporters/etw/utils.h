// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

#include "opentelemetry/common/macros.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/etw/uuid.h"
#include "opentelemetry/version.h"

#ifdef _WIN32
#  include <Windows.h>
#  include <evntprov.h>
#  include <wincrypt.h>
#  pragma comment(lib, "Advapi32.lib")
#  pragma comment(lib, "Rpcrt4.lib")
#  include <Objbase.h>
#  pragma comment(lib, "Ole32.Lib")
#else
#  include <codecvt>
#endif

#if defined(ENABLE_ENV_PROPERTIES) || defined(HAVE_MSGPACK)
#  include <nlohmann/json.hpp>
#endif

#if defined(ENABLE_ENV_PROPERTIES)
#  include "etw_properties.h"
#endif

OPENTELEMETRY_BEGIN_NAMESPACE

namespace utils
{

/// <summary>
/// Compile-time constexpr djb2 hash function for strings
/// </summary>
static constexpr uint32_t hashCode(const char *str, uint32_t h = 0)
{
  return (uint32_t)(!str[h] ? 5381 : ((uint32_t)hashCode(str, h + 1) * (uint32_t)33) ^ str[h]);
}

#define CONST_UINT32_T(x) std::integral_constant<uint32_t, (uint32_t)x>::value

#define CONST_HASHCODE(name) CONST_UINT32_T(OPENTELEMETRY_NAMESPACE::utils::hashCode(#name))

#ifdef _WIN32

/// <summary>
/// Compute SHA-1 hash of input buffer and save to output
/// </summary>
/// <param name="pData">Input buffer</param>
/// <param name="nData">Input buffer size</param>
/// <param name="pHashedData">Output buffer</param>
/// <param name="nHashedData">Output buffer size</param>
/// <returns></returns>
static inline bool sha1(const BYTE *pData, DWORD nData, BYTE *pHashedData, DWORD &nHashedData)
{
  bool bRet        = false;
  HCRYPTPROV hProv = NULL;
  HCRYPTHASH hHash = NULL;

  if (!CryptAcquireContext(&hProv,                // handle of the CSP
                           NULL,                  // key container name
                           NULL,                  // CSP name
                           PROV_RSA_FULL,         // provider type
                           CRYPT_VERIFYCONTEXT))  // no key access is requested
  {
    bRet = false;
    goto CleanUp;
  }

  if (!CryptCreateHash(hProv,      // handle of the CSP
                       CALG_SHA1,  // hash algorithm to use
                       0,          // hash key
                       0,          // reserved
                       &hHash))    //
  {
    bRet = false;
    goto CleanUp;
  }

  if (!CryptHashData(hHash,  // handle of the HMAC hash object
                     pData,  // message to hash
                     nData,  // number of bytes of data to add
                     0))     // flags
  {
    bRet = false;
    goto CleanUp;
  }

  if (!CryptGetHashParam(hHash,         // handle of the HMAC hash object
                         HP_HASHVAL,    // query on the hash value
                         pHashedData,   // filled on second call
                         &nHashedData,  // length, in bytes,of the hash
                         0))
  {
    bRet = false;
    goto CleanUp;
  }

  bRet = true;

CleanUp:

  if (hHash)
  {
    CryptDestroyHash(hHash);
  }

  if (hProv)
  {
    CryptReleaseContext(hProv, 0);
  }
  return bRet;
}

/// <summary>
/// Convert UTF-8 string to UTF-16 wide string.
///
/// </summary>
/// <param name="in"></param>
/// <returns></returns>
static inline std::wstring to_utf16_string(const std::string &in)
{
#  ifdef _WIN32
  int in_length  = static_cast<int>(in.size());
  int out_length = MultiByteToWideChar(CP_UTF8, 0, &in[0], in_length, NULL, 0);
  std::wstring result(out_length, '\0');
  MultiByteToWideChar(CP_UTF8, 0, &in[0], in_length, &result[0], out_length);
  return result;
#  else
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
  return converter.from_bytes(in);
#  endif
}

/// <summary>
/// Transform ETW provider name to provider GUID as described here:
/// https://blogs.msdn.microsoft.com/dcook/2015/09/08/etw-provider-names-and-guids/
/// </summary>
/// <param name="providerName"></param>
/// <returns></returns>
static inline GUID GetProviderGuid(const char *providerName)
{
  std::string name(providerName);
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return (char)::toupper(c); });

  size_t len      = name.length() * 2 + 0x10;
  uint8_t *buffer = new uint8_t[len];
  uint32_t num    = 0x482c2db2;
  uint32_t num2   = 0xc39047c8;
  uint32_t num3   = 0x87f81a15;
  uint32_t num4   = 0xbfc130fb;

  for (int i = 3; i >= 0; i--)
  {
    buffer[i]      = (uint8_t)num;
    num            = num >> 8;
    buffer[i + 4]  = (uint8_t)num2;
    num2           = num2 >> 8;
    buffer[i + 8]  = (uint8_t)num3;
    num3           = num3 >> 8;
    buffer[i + 12] = (uint8_t)num4;
    num4           = num4 >> 8;
  }

  for (size_t j = 0; j < name.length(); j++)
  {
    buffer[((2 * j) + 0x10) + 1] = (uint8_t)name[j];
    buffer[(2 * j) + 0x10]       = (uint8_t)(name[j] >> 8);
  }

  const size_t sha1_hash_size = 21;
  uint8_t *buffer2            = new uint8_t[sha1_hash_size];
  DWORD len2                  = sha1_hash_size;
  sha1((const BYTE *)buffer, (DWORD)len, (BYTE *)buffer2, len2);

  unsigned long a     = (((((buffer2[3] << 8) + buffer2[2]) << 8) + buffer2[1]) << 8) + buffer2[0];
  unsigned short b    = (unsigned short)((buffer2[5] << 8) + buffer2[4]);
  unsigned short num9 = (unsigned short)((buffer2[7] << 8) + buffer2[6]);

  GUID guid;
  guid.Data1    = a;
  guid.Data2    = b;
  guid.Data3    = (unsigned short)((num9 & 0xfff) | 0x5000);
  guid.Data4[0] = buffer2[8];
  guid.Data4[1] = buffer2[9];
  guid.Data4[2] = buffer2[10];
  guid.Data4[3] = buffer2[11];
  guid.Data4[4] = buffer2[12];
  guid.Data4[5] = buffer2[13];
  guid.Data4[6] = buffer2[14];
  guid.Data4[7] = buffer2[15];

  delete[] buffer;
  delete[] buffer2;

  return guid;
}
#endif

static inline int64_t getUtcSystemTimeMs()
{
#ifdef _WIN32
  ULARGE_INTEGER now;
  ::GetSystemTimeAsFileTime(reinterpret_cast<FILETIME *>(&now));
  return (now.QuadPart - 116444736000000000ull) / 10000;
#else
  return std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
#endif
}

static inline int64_t getUtcSystemTimeinTicks()
{
#ifdef _WIN32
  FILETIME tocks;
  ::GetSystemTimeAsFileTime(&tocks);
  ULONGLONG ticks = (ULONGLONG(tocks.dwHighDateTime) << 32) | tocks.dwLowDateTime;
  // number of days from beginning to 1601 multiplied by ticks per day
  return ticks + 0x701ce1722770000ULL;
#else
  // On Un*x systems system_clock de-facto contains UTC time. Ref:
  // https://en.cppreference.com/w/cpp/chrono/system_clock
  // This UTC epoch contract has been signed in blood since C++20
  std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
  auto duration                                          = now.time_since_epoch();
  auto millis    = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  uint64_t ticks = millis;
  ticks *= 10000;                 // convert millis to ticks (1 tick = 100ns)
  ticks += 0x89F7FF5F7B58000ULL;  // UTC time 0 in .NET ticks
  return ticks;
#endif
}

constexpr unsigned int NANOSECS_PRECISION = 1000000000;
/**
 * @brief Convert local system nanoseconds time to ISO8601 string UTC time
 *
 * @param timestampNs   Milliseconds since epoch in system time
 *
 * @return ISO8601 UTC string with nanoseconds
 */
static inline std::string formatUtcTimestampNsAsISO8601(int64_t timestampNs)
{
  char buf[sizeof("YYYY-MM-DDTHH:MM:SS.sssssssssZ") + 1] = {0};
#ifdef _WIN32
  __time64_t seconds = static_cast<__time64_t>(timestampNs / NANOSECS_PRECISION);
  int nanoseconds    = static_cast<int>(timestampNs % NANOSECS_PRECISION);
  tm tm;
  if (::_gmtime64_s(&tm, &seconds) != 0)
  {
    memset(&tm, 0, sizeof(tm));
  }
  ::_snprintf_s(buf, _TRUNCATE, "%04d-%02d-%02dT%02d:%02d:%02d.%09dZ", 1900 + tm.tm_year,
                1 + tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, nanoseconds);
#else
  time_t seconds  = static_cast<time_t>(timestampNs / NANOSECS_PRECISION);
  int nanoseconds = static_cast<int>(timestampNs % NANOSECS_PRECISION);
  tm tm;
  bool valid = (gmtime_r(&seconds, &tm) != NULL);
  if (!valid)
  {
    memset(&tm, 0, sizeof(tm));
  }
  (void)snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%09dZ", 1900 + tm.tm_year,
                 1 + tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, nanoseconds);
#endif
  return buf;
}

#if defined(ENABLE_ENV_PROPERTIES)

static inline void PopulateAttribute(nlohmann::json &attribute,
                                     nostd::string_view key,
                                     const exporter::etw::PropertyValue &value)
{
  if (nostd::holds_alternative<bool>(value))
  {
    attribute[key.data()] = nostd::get<bool>(value);
  }
  else if (nostd::holds_alternative<int>(value))
  {
    attribute[key.data()] = nostd::get<int>(value);
  }
  else if (nostd::holds_alternative<int64_t>(value))
  {
    attribute[key.data()] = nostd::get<int64_t>(value);
  }
  else if (nostd::holds_alternative<unsigned int>(value))
  {
    attribute[key.data()] = nostd::get<unsigned int>(value);
  }
  else if (nostd::holds_alternative<uint64_t>(value))
  {
    attribute[key.data()] = nostd::get<uint64_t>(value);
  }
  else if (nostd::holds_alternative<double>(value))
  {
    attribute[key.data()] = nostd::get<double>(value);
  }
  else if (nostd::holds_alternative<const char *>(value))
  {
    attribute[key.data()] = std::string(nostd::get<const char *>(value));
  }
  else if (nostd::holds_alternative<std::string>(value))
  {
    attribute[key.data()] = nostd::get<std::string>(value);
  }
  else if (nostd::holds_alternative<std::vector<uint8_t>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<std::vector<uint8_t>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<std::vector<bool>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<std::vector<bool>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<std::vector<int>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<std::vector<int>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<std::vector<int64_t>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<std::vector<int64_t>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<std::vector<unsigned int>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<std::vector<unsigned int>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<std::vector<uint64_t>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<std::vector<uint64_t>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<std::vector<double>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<std::vector<double>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<std::vector<std::string>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<std::vector<std::string>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
}

#endif  // defined(ENABLE_ENV_PROPERTIES)

#if defined(HAVE_MSGPACK)

static inline nlohmann::byte_container_with_subtype<std::vector<std::uint8_t>>
get_msgpack_eventtimeext(int32_t seconds = 0, int32_t nanoseconds = 0)
{
  if ((seconds == 0) && (nanoseconds == 0))
  {
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    auto duration                            = tp.time_since_epoch();
    seconds =
        static_cast<int32_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
    nanoseconds = static_cast<int32_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000);
  }

  uint64_t timestamp =
      ((seconds / 100) & ((1ull << 34) - 1)) |
      (((seconds % 100 * 10000000 + nanoseconds / 100) & ((1ull << 30) - 1)) << 34);

  nlohmann::byte_container_with_subtype<std::vector<std::uint8_t>> ts{std::vector<uint8_t>(8)};

  *reinterpret_cast<uint64_t *>(ts.data()) = timestamp;
  ts.set_subtype(0x00);

  return ts;
}

static inline nlohmann::byte_container_with_subtype<std::vector<std::uint8_t>>
GetMsgPackEventTimeFromSystemTimestamp(opentelemetry::common::SystemTimestamp timestamp) noexcept
{
  return get_msgpack_eventtimeext(
      // Add all whole seconds to the event time
      static_cast<int32_t>(
          std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count()),
      // Add any remaining nanoseconds past the last whole second
      std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp.time_since_epoch()).count() %
          1000000000);
}

#endif  // defined(HAVE_MSGPACK)

};  // namespace utils

OPENTELEMETRY_END_NAMESPACE
