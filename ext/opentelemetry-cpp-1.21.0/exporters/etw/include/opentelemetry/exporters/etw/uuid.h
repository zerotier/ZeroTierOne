// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

#include <cstddef>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#ifdef _WIN32
#  include "Windows.h"
#endif

OPENTELEMETRY_BEGIN_NAMESPACE

namespace utils
{

/// <summary>
/// The UUID structure represents the portable cross-platform implementation of a GUID (Globally
/// Unique ID).
/// </summary>
/// <remarks>
/// UUIDs identify objects such as interfaces, manager entry-point vectors (EPVs), and class
/// objects. A UUID is a 128-bit value consisting of one group of eight hexadecimal digits, followed
/// by three groups of four hexadecimal digits, each followed by one group of 12 hexadecimal digits.
/// </remarks>
#pragma pack(push) /* push current alignment to stack */
#pragma pack(1)    /* set alignment to 1 byte boundary */
struct UUID
{
  /// <summary>
  /// Specifies the first eight hexadecimal digits of the GUID.
  /// </summary>
  uint32_t Data1;

  /// <summary>
  /// Specifies the first group of four hexadecimal digits.
  ///</summary>
  uint16_t Data2;

  /// <summary>
  /// Specifies the second group of four hexadecimal digits.
  /// </summary>
  uint16_t Data3;

  /// <summary>
  /// An array of eight bytes.
  /// The first two bytes contain the third group of four hexadecimal digits.
  /// The remaining six bytes contain the final 12 hexadecimal digits.
  /// </summary>
  uint8_t Data4[8];

  /// <summary>
  /// The default UUID constructor.
  /// Creates a null instance of the UUID object (initialized to all zeros).
  /// {00000000-0000-0000-0000-000000000000}.
  /// </summary>
  UUID() : Data1(0), Data2(0), Data3(0)
  {
    for (size_t i = 0; i < 8; i++)
    {
      Data4[i] = 0;
    }
  }

  /// <summary>
  /// A constructor that creates a UUID object from a hyphenated string as defined by
  /// https://tools.ietf.org/html/rfc4122#page-4
  /// </summary>
  /// <param name="uuid_string">A hyphenated string that contains the UUID (curly braces
  /// optional).</param>
  UUID(const char *uuid_string)
  {
    const char *str = uuid_string;
    // Skip curly brace
    if (str[0] == '{')
    {
      str++;
    }
    // Convert to set of integer values
    unsigned long p0;
    unsigned int p1, p2, p3, p4, p5, p6, p7, p8, p9, p10;
    if (
        // Parse input with dashes
        (11 == sscanf_s(str, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", &p0, &p1, &p2,
                        &p3, &p4, &p5, &p6, &p7, &p8, &p9, &p10)) ||
        // Parse input without dashes
        (11 == sscanf_s(str, "%08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X", &p0, &p1, &p2, &p3,
                        &p4, &p5, &p6, &p7, &p8, &p9, &p10)))
    {
      Data1    = static_cast<uint32_t>(p0);
      Data2    = static_cast<uint16_t>(p1);
      Data3    = static_cast<uint16_t>(p2);
      Data4[0] = static_cast<uint8_t>(p3);
      Data4[1] = static_cast<uint8_t>(p4);
      Data4[2] = static_cast<uint8_t>(p5);
      Data4[3] = static_cast<uint8_t>(p6);
      Data4[4] = static_cast<uint8_t>(p7);
      Data4[5] = static_cast<uint8_t>(p8);
      Data4[6] = static_cast<uint8_t>(p9);
      Data4[7] = static_cast<uint8_t>(p10);
    }
    else  // Invalid input--use a safe default value
    {
      Data1    = 0;
      Data2    = 0;
      Data3    = 0;
      Data4[0] = 0;
      Data4[1] = 0;
      Data4[2] = 0;
      Data4[3] = 0;
      Data4[4] = 0;
      Data4[5] = 0;
      Data4[6] = 0;
      Data4[7] = 0;
    }
  }

  /// <summary>
  /// A constructor that creates a UUID object from a byte array.
  /// </summary>
  /// <param name="guid_bytes">A byte array.</param>
  /// <param name="bigEndian">
  /// A boolean value that specifies the byte order.<br>
  /// A value of <i>true</i> specifies the more natural human-readable order.<br>
  /// A value of <i>false</i> (the default) specifies the same order as the .NET GUID constructor.
  /// </param>
  UUID(const uint8_t guid_bytes[16], bool bigEndian = false)
  {
    if (bigEndian)
    {
      /* Use big endian - human-readable */
      // Part 1
      Data1 = guid_bytes[3];
      Data1 |= ((uint32_t)(guid_bytes[2])) << 8;
      Data1 |= ((uint32_t)(guid_bytes[1])) << 16;
      Data1 |= ((uint32_t)(guid_bytes[0])) << 24;
      // Part 2
      Data2 = guid_bytes[5];
      Data2 |= ((uint16_t)(guid_bytes[4])) << 8;
      // Part 3
      Data3 = guid_bytes[7];
      Data3 |= ((uint16_t)(guid_bytes[6])) << 8;
    }
    else
    {
      /* Use little endian - the same order as .NET C# Guid() class uses */
      // Part 1
      Data1 = guid_bytes[0];
      Data1 |= ((uint32_t)(guid_bytes[1])) << 8;
      Data1 |= ((uint32_t)(guid_bytes[2])) << 16;
      Data1 |= ((uint32_t)(guid_bytes[3])) << 24;
      // Part 2
      Data2 = guid_bytes[4];
      Data2 |= ((uint16_t)(guid_bytes[5])) << 8;
      // Part 3
      Data3 = guid_bytes[6];
      Data3 |= ((uint16_t)(guid_bytes[7])) << 8;
    }
    // Part 4
    for (size_t i = 0; i < 8; i++)
    {
      Data4[i] = guid_bytes[8 + i];
    }
  }

  /// <summary>
  /// A constructor that creates a UUID object from three integers and a byte array.
  /// </summary>
  /// <param name="d1">An integer that specifies the first eight hexadecimal digits of the
  /// UUID.</param> <param name="d2">An integer that specifies the first group of four hexadecimal
  /// digits.</param> <param name="d3">An integer that specifies the second group of four
  /// hexadecimal digits.</param> <param name="v">A reference to an array of eight bytes. The first
  /// two bytes contain the third group of four hexadecimal digits. The remaining six bytes contain
  /// the final 12 hexadecimal digits.</param>
  UUID(int d1, int d2, int d3, const std::initializer_list<uint8_t> &v)
      : Data1((uint32_t)d1), Data2((uint16_t)d2), Data3((uint16_t)d3)
  {
    size_t i = 0;
    for (auto val : v)
    {
      Data4[i] = val;
      i++;
    }
  }

  /// <summary>
  /// The UUID copy constructor.
  /// </summary>
  /// <param name="uuid">A UUID object.</param>
  UUID(const UUID &uuid)
  {
    this->Data1 = uuid.Data1;
    this->Data2 = uuid.Data2;
    this->Data3 = uuid.Data3;
    memcpy(&(this->Data4[0]), &(uuid.Data4[0]), sizeof(uuid.Data4));
  }

#ifdef _WIN32

  /// <summary>
  /// A constructor that creates a UUID object from a Windows GUID object.
  /// </summary>
  /// <param name="guid">A Windows GUID object.</param>
  UUID(GUID guid)
  {
    this->Data1 = guid.Data1;
    this->Data2 = guid.Data2;
    this->Data3 = guid.Data3;
    std::memcpy(&(this->Data4[0]), &(guid.Data4[0]), sizeof(guid.Data4));
  }

  /// <summary>
  /// Converts a standard vector of bytes into a Windows GUID object.
  /// </summary>
  /// <param name="bytes">A standard vector of bytes.</param>
  /// <returns>A GUID.</returns>
  static GUID to_GUID(std::vector<uint8_t> const &bytes)
  {
    UUID temp_t = UUID(bytes.data());
    GUID temp;
    temp.Data1 = temp_t.Data1;
    temp.Data2 = temp_t.Data2;
    temp.Data3 = temp_t.Data3;
    for (size_t i = 0; i < 8; i++)
    {
      temp.Data4[i] = temp_t.Data4[i];
    }
    return temp;
  }

  GUID to_GUID()
  {
    GUID temp;
    temp.Data1 = Data1;
    temp.Data2 = Data2;
    temp.Data3 = Data3;
    for (size_t i = 0; i < 8; i++)
    {
      temp.Data4[i] = Data4[i];
    }
    return temp;
  }

#endif

  /// <summary>
  /// Converts this UUID to an array of bytes.
  /// </summary>
  /// <param name="guid_bytes">A uint8_t array of 16 bytes.</param>
  void to_bytes(uint8_t (&guid_bytes)[16]) const
  {
    // Part 1
    guid_bytes[0] = (uint8_t)((Data1) & 0xFF);
    guid_bytes[1] = (uint8_t)((Data1 >> 8) & 0xFF);
    guid_bytes[2] = (uint8_t)((Data1 >> 16) & 0xFF);
    guid_bytes[3] = (uint8_t)((Data1 >> 24) & 0xFF);
    // Part 2
    guid_bytes[4] = (uint8_t)((Data2) & 0xFF);
    guid_bytes[5] = (uint8_t)((Data2 >> 8) & 0xFF);
    // Part 3
    guid_bytes[6] = (uint8_t)((Data3) & 0xFF);
    guid_bytes[7] = (uint8_t)((Data3 >> 8) & 0xFF);
    // Part 4
    for (size_t i = 0; i < 8; i++)
    {
      guid_bytes[8 + i] = Data4[i];
    }
  }

  /// <summary>
  /// Convert this UUID object to a string.
  /// </summary>
  /// <returns>This UUID object in a string.</returns>
  std::string to_string() const
  {
    static char inttoHex[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    const unsigned buffSize = 36 + 1;  // 36 + null-terminator
    char buf[buffSize]      = {0};

    int test = (Data1 >> 28 & 0x0000000F);
    buf[0]   = inttoHex[test];
    test     = (int)(Data1 >> 24 & 0x0000000F);
    buf[1]   = inttoHex[test];
    test     = (int)(Data1 >> 20 & 0x0000000F);
    buf[2]   = inttoHex[test];
    test     = (int)(Data1 >> 16 & 0x0000000F);
    buf[3]   = inttoHex[test];
    test     = (int)(Data1 >> 12 & 0x0000000F);
    buf[4]   = inttoHex[test];
    test     = (int)(Data1 >> 8 & 0x0000000F);
    buf[5]   = inttoHex[test];
    test     = (int)(Data1 >> 4 & 0x0000000F);
    buf[6]   = inttoHex[test];
    test     = (int)(Data1 & 0x0000000F);
    buf[7]   = inttoHex[test];
    buf[8]   = '-';
    test     = (int)(Data2 >> 12 & 0x000F);
    buf[9]   = inttoHex[test];
    test     = (int)(Data2 >> 8 & 0x000F);
    buf[10]  = inttoHex[test];
    test     = (int)(Data2 >> 4 & 0x000F);
    buf[11]  = inttoHex[test];
    test     = (int)(Data2 & 0x000F);
    buf[12]  = inttoHex[test];
    buf[13]  = '-';
    test     = (int)(Data3 >> 12 & 0x000F);
    buf[14]  = inttoHex[test];
    test     = (int)(Data3 >> 8 & 0x000F);
    buf[15]  = inttoHex[test];
    test     = (int)(Data3 >> 4 & 0x000F);
    buf[16]  = inttoHex[test];
    test     = (int)(Data3 & 0x000F);
    buf[17]  = inttoHex[test];
    buf[18]  = '-';
    test     = (int)(Data4[0] >> 4 & 0x0F);
    buf[19]  = inttoHex[test];
    test     = (int)(Data4[0] & 0x0F);
    buf[20]  = inttoHex[test];
    test     = (int)(Data4[1] >> 4 & 0x0F);
    buf[21]  = inttoHex[test];
    test     = (int)(Data4[1] & 0x0F);
    buf[22]  = inttoHex[test];
    buf[23]  = '-';
    test     = (int)(Data4[2] >> 4 & 0x0F);
    buf[24]  = inttoHex[test];
    test     = (int)(Data4[2] & 0x0F);
    buf[25]  = inttoHex[test];
    test     = (int)(Data4[3] >> 4 & 0x0F);
    buf[26]  = inttoHex[test];
    test     = (int)(Data4[3] & 0x0F);
    buf[27]  = inttoHex[test];
    test     = (int)(Data4[4] >> 4 & 0x0F);
    buf[28]  = inttoHex[test];
    test     = (int)(Data4[4] & 0x0F);
    buf[29]  = inttoHex[test];
    test     = (int)(Data4[5] >> 4 & 0x0F);
    buf[30]  = inttoHex[test];
    test     = (int)(Data4[5] & 0x0F);
    buf[31]  = inttoHex[test];
    test     = (int)(Data4[6] >> 4 & 0x0F);
    buf[32]  = inttoHex[test];
    test     = (int)(Data4[6] & 0x0F);
    buf[33]  = inttoHex[test];
    test     = (int)(Data4[7] >> 4 & 0x0F);
    buf[34]  = inttoHex[test];
    test     = (int)(Data4[7] & 0x0F);
    buf[35]  = inttoHex[test];
    buf[36]  = 0;

    return std::string(buf);
  }

  /// <summary>
  /// Calculates the size of this UUID object.
  /// The output from this method is compatible with std::unordered_map.
  /// </summary>
  /// <returns>The size of the UUID object in bytes.</returns>
  size_t Hash() const
  {
    // Compute individual hash values for Data1, Data2, Data3, and parts of Data4
    size_t res = 17;
    res        = res * 31 + Data1;
    res        = res * 31 + Data2;
    res        = res * 31 + Data3;
    res        = res * 31 + (Data4[0] << 24 | Data4[1] << 16 | Data4[6] << 8 | Data4[7]);
    return res;
  }

  /// <summary>
  /// Tests to determine whether two UUID objects are equivalent (needed for maps).
  /// </summary>
  /// <returns>A boolean value that indicates success or failure.</returns>
  bool operator==(UUID const &other) const
  {
    return Data1 == other.Data1 && Data2 == other.Data2 && Data3 == other.Data3 &&
           (0 == memcmp(Data4, other.Data4, sizeof(Data4)));
  }

  /// <summary>
  /// Tests to determine how to sort 2 UUID objects
  /// </summary>
  /// <returns>A boolean value that indicates success or failure.</returns>
  bool operator<(UUID const &other) const
  {
    return Data1 < other.Data1 || Data2 < other.Data2 || Data3 == other.Data3 ||
           (memcmp(Data4, other.Data4, sizeof(Data4)) < 0);
  }
};
#pragma pack(pop) /* restore original alignment from stack */

/// <summary>
/// Declare UUIDComparer as the Comparer when using UUID as a key in a map or set
/// </summary>
struct UUIDComparer : std::less<UUID>
{
  inline size_t operator()(UUID const &key) const { return key.Hash(); }

  inline bool operator()(UUID const &lhs, UUID const &rhs) const { return lhs.Hash() < rhs.Hash(); }
};

}  // namespace utils

OPENTELEMETRY_END_NAMESPACE
