// // Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4459)
#  pragma warning(disable : 4018)
#  pragma warning(disable : 5054)
#endif

#include "opentelemetry/exporters/etw/etw_properties.h"
#include "opentelemetry/exporters/etw/uuid.h"
#include "opentelemetry/version.h"

#include "opentelemetry/exporters/etw/etw_fields.h"
#include "opentelemetry/exporters/etw/utils.h"

#include "opentelemetry/exporters/etw/etw_traceloggingdynamic.h"

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef HAVE_KRABS_TESTS
// krabs.hpp requires this definition of min macro from Windows.h
#  ifndef min
#    define min(a, b) (((a) < (b)) ? (a) : (b))
#  endif
#endif

#define MICROSOFT_EVENTTAG_NORMAL_PERSISTENCE 0x01000000

namespace exporter_etw = OPENTELEMETRY_NAMESPACE::exporter::etw;

OPENTELEMETRY_BEGIN_NAMESPACE

class ETWProvider
{

public:
  const unsigned long STATUS_OK    = 0;
  const unsigned long STATUS_ERROR = ULONG_MAX;
  const unsigned long STATUS_EFBIG = ULONG_MAX - 1;

  enum EventFormat
  {
    ETW_MANIFEST = 0,
    ETW_MSGPACK  = 1,
    ETW_XML      = 2
  };

  /// <summary>
  /// Entry that contains Provider Handle, Provider MetaData and Provider GUID
  /// </summary>
  struct Handle
  {
    uint64_t refCount;
    REGHANDLE providerHandle;
    std::vector<BYTE> providerMetaVector;
    GUID providerGuid;
  };

  /// <summary>
  /// Check if given provider is registered.
  /// </summary>
  /// <param name="providerId"></param>
  /// <returns></returns>
  bool is_registered(const std::string &providerId)
  {
    std::lock_guard<std::mutex> lock(m_providerMapLock);
    auto it = providers().find(providerId);
    if (it != providers().end())
    {
      if (it->second.providerHandle != INVALID_HANDLE)
      {
        return true;
      }
    }
    return false;
  }

  /// <summary>
  /// Get Provider by Name or string representation of GUID
  /// </summary>
  /// <param name="providerId"></param>
  /// <returns></returns>
  Handle &open(const std::string &providerId, EventFormat format = EventFormat::ETW_MSGPACK)
  {
    std::lock_guard<std::mutex> lock(m_providerMapLock);

    // Check and return if provider is already registered
    auto it = providers().find(providerId);
    if (it != providers().end())
    {
      if (it->second.providerHandle != INVALID_HANDLE)
      {
        it->second.refCount++;
        return it->second;
      }
    }

    // Register provider if necessary
    auto &data = providers()[providerId];
    data.providerMetaVector.clear();

    utils::UUID guid = (providerId.rfind("{", 0) == 0) ? utils::UUID(providerId.c_str())
                                                       :                // It's a ProviderGUID
                           utils::GetProviderGuid(providerId.c_str());  // It's a ProviderName

    data.providerGuid = guid.to_GUID();

    // TODO: currently we do not allow to specify a custom group GUID
    GUID providerGroupGuid = NULL_GUID;

    switch (format)
    {
      // Register with TraceLoggingDynamic facility - dynamic manifest ETW events.
      case EventFormat::ETW_MANIFEST: {
        tld::ProviderMetadataBuilder<std::vector<BYTE>> providerMetaBuilder(
            data.providerMetaVector);

        // Use Tenant ID as provider Name
        providerMetaBuilder.Begin(providerId.c_str());
        providerMetaBuilder.AddTrait(tld::ProviderTraitType::ProviderTraitGroupGuid,
                                     (void *)&providerGroupGuid, sizeof(GUID));
        providerMetaBuilder.End();

        REGHANDLE hProvider = 0;
        if (0 !=
            tld::RegisterProvider(&hProvider, &data.providerGuid, data.providerMetaVector.data()))
        {
          // There was an error registering the ETW provider
          data.refCount       = 0;
          data.providerHandle = INVALID_HANDLE;
        }
        else
        {
          data.refCount       = 1;
          data.providerHandle = hProvider;
        }
      }
      break;

#ifdef HAVE_MSGPACK
      // Register for MsgPack payload ETW events.
      case EventFormat::ETW_MSGPACK: {
        REGHANDLE hProvider = 0;
        if (EventRegister(&data.providerGuid, NULL, NULL, &hProvider) != ERROR_SUCCESS)
        {
          // There was an error registering the ETW provider
          data.refCount       = 0;
          data.providerHandle = INVALID_HANDLE;
        }
        else
        {
          data.refCount       = 1;
          data.providerHandle = hProvider;
        }
      }
      break;
#endif

      default:
        // TODO: other protocols, e.g. XML events - not supported yet
        break;
    }

    // We always return an entry even if we failed to register.
    // Caller should check whether the hProvider handle is valid.
    return data;
  }

  /**
   * @brief Unregister provider
   * @param data Provider Handle
   * @return status code
   */
  unsigned long close(Handle handle)
  {
    std::lock_guard<std::mutex> lock(m_providerMapLock);

    // use reference to provider list, NOT it' copy.
    auto &m = providers();
    auto it = m.begin();
    while (it != m.end())
    {
      if (it->second.providerHandle == handle.providerHandle)
      {
        // reference to item in the map of open provider handles
        auto &data           = it->second;
        unsigned long result = STATUS_OK;

        data.refCount--;
        if (data.refCount == 0)
        {
          if (data.providerMetaVector.size())
          {
            // ETW/TraceLoggingDynamic provider
            result = tld::UnregisterProvider(data.providerHandle);
          }
          else
          {
            // Other provider types, e.g. ETW/MsgPack
            result = EventUnregister(data.providerHandle);
          }

          it->second.providerHandle = INVALID_HANDLE;
          if (result == STATUS_OK)
          {
            m.erase(it);
          }
        }
        return result;
      }
    }
    return STATUS_ERROR;
  }

  unsigned long writeMsgPack(Handle &providerData,
                             exporter_etw::Properties &eventData,
                             LPCGUID ActivityId        = nullptr,
                             LPCGUID RelatedActivityId = nullptr,
                             uint8_t Opcode            = 0)
  {
#ifdef HAVE_MSGPACK
    // Events can only be sent if provider is registered
    if (providerData.providerHandle == INVALID_HANDLE)
    {
      // Provider not registered!
      return STATUS_ERROR;
    }

    std::string eventName = "NoName";
    auto nameField        = eventData[ETW_FIELD_NAME];

#  ifdef HAVE_FIELD_TIME
    // Event time is appended by ETW layer itself by default. This code allows
    // to override the timestamp by millisecond timestamp, in case if it has
    // not been already provided by the upper layer.
    if (!eventData.count(ETW_FIELD_TIME))
    {
      // TODO: if nanoseconds resolution is required, then we can populate it in 96-bit MsgPack
      // spec. auto nanos =
      // std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
      eventData[ETW_FIELD_TIME] = opentelemetry::utils::getUtcSystemTimeMs();
    }
#  endif

    switch (nameField.index())
    {
      case exporter_etw::PropertyType::kTypeString:
        eventName = (char *)(nostd::get<std::string>(nameField).data());  // must be 0-terminated!
        break;
      case exporter_etw::PropertyType::kTypeCString:
        eventName = (char *)(nostd::get<const char *>(nameField));
        break;
      default:
        // If invalid event name is supplied, then we replace it with 'NoName'
        break;
    }

    /* clang-format off */
    nlohmann::json jObj =
    {
      { ETW_FIELD_OPCODE, Opcode }
    };
    /* clang-format on */

    std::string eventFieldName(ETW_FIELD_NAME);
    for (auto &kv : eventData)
    {
      const char *name = kv.first.data();

      // Don't include event name field in the Payload section
      if (eventFieldName == name)
        continue;

      auto &value = kv.second;
      switch (value.index())
      {
        case exporter_etw::PropertyType::kTypeBool: {
          UINT8 temp = static_cast<UINT8>(nostd::get<bool>(value));
          jObj[name] = temp;
          break;
        }
        case exporter_etw::PropertyType::kTypeInt: {
          auto temp  = nostd::get<int32_t>(value);
          jObj[name] = temp;
          break;
        }
        case exporter_etw::PropertyType::kTypeInt64: {
          auto temp  = nostd::get<int64_t>(value);
          jObj[name] = temp;
          break;
        }
        case exporter_etw::PropertyType::kTypeUInt: {
          auto temp  = nostd::get<uint32_t>(value);
          jObj[name] = temp;
          break;
        }
        case exporter_etw::PropertyType::kTypeUInt64: {
          auto temp  = nostd::get<uint64_t>(value);
          jObj[name] = temp;
          break;
        }
        case exporter_etw::PropertyType::kTypeDouble: {
          auto temp  = nostd::get<double>(value);
          jObj[name] = temp;
          break;
        }
        case exporter_etw::PropertyType::kTypeString: {
          jObj[name] = nostd::get<std::string>(value);
          break;
        }
        case exporter_etw::PropertyType::kTypeCString: {
          auto temp  = nostd::get<const char *>(value);
          jObj[name] = temp;
          break;
        }
#  if HAVE_TYPE_GUID
          // TODO: consider adding UUID/GUID to spec
        case opentelemetry::common::AttributeType::TYPE_GUID: {
          auto temp = nostd::get<GUID>(value);
          // TODO: add transform from GUID type to string?
          jObj[name] = temp;
          break;
        }
#  endif

        // TODO: arrays are not supported yet
        case exporter_etw::PropertyType::kTypeSpanByte:
        case exporter_etw::PropertyType::kTypeSpanBool:
        case exporter_etw::PropertyType::kTypeSpanInt:
        case exporter_etw::PropertyType::kTypeSpanInt64:
        case exporter_etw::PropertyType::kTypeSpanUInt:
        case exporter_etw::PropertyType::kTypeSpanUInt64:
        case exporter_etw::PropertyType::kTypeSpanDouble:
        case exporter_etw::PropertyType::kTypeSpanString:
        default:
          // TODO: unsupported type
          break;
      }
    }

    // forwardMessage.push_back(nameField);
    nlohmann::json payloadPair = nlohmann::json::array();

    payloadPair.push_back(
        utils::GetMsgPackEventTimeFromSystemTimestamp(std::chrono::system_clock::now()));
    payloadPair.push_back(jObj);

    nlohmann::json payloadArray = nlohmann::json::array({payloadPair});

    nlohmann::json forwardMessage = nlohmann::json::array({eventName, payloadArray});

    std::vector<uint8_t> v = nlohmann::json::to_msgpack(forwardMessage);

    EVENT_DESCRIPTOR evtDescriptor;
    // TODO: event descriptor may be populated with additional values as follows:
    // Id       - if 0, auto-incremented sequence number that uniquely identifies event in a trace
    // Version  - event version
    // Channel  - 11 for TraceLogging
    // Level    - verbosity level
    // Task     - TaskId
    // Opcode   - described in evntprov.h:259 : 0 - info, 1 - activity start, 2 - activity stop.
    EventDescCreate(&evtDescriptor, 100, 0x1, 0, 0, 0, Opcode, 0);
    EVENT_DATA_DESCRIPTOR evtData[1];
    EventDataDescCreate(&evtData[0], v.data(), static_cast<ULONG>(v.size()));
    ULONG writeResponse = 0;
    if ((ActivityId != nullptr) || (RelatedActivityId != nullptr))
    {
      writeResponse = EventWriteTransfer(providerData.providerHandle, &evtDescriptor, ActivityId,
                                         RelatedActivityId, 1, evtData);
    }
    else
    {
      writeResponse = EventWrite(providerData.providerHandle, &evtDescriptor, 1, evtData);
    }

    switch (writeResponse)
    {
      case ERROR_INVALID_PARAMETER:
        break;
      case ERROR_INVALID_HANDLE:
        break;
      case ERROR_ARITHMETIC_OVERFLOW:
        break;
      case ERROR_MORE_DATA:
        break;
      case ERROR_NOT_ENOUGH_MEMORY:
        break;
      default:
        break;
    }

    if (writeResponse == ERROR_ARITHMETIC_OVERFLOW)
    {
      return STATUS_EFBIG;
    }
    return (unsigned long)(writeResponse);
#else
    UNREFERENCED_PARAMETER(providerData);
    UNREFERENCED_PARAMETER(eventData);
    UNREFERENCED_PARAMETER(ActivityId);
    UNREFERENCED_PARAMETER(RelatedActivityId);
    UNREFERENCED_PARAMETER(Opcode);
    return STATUS_ERROR;
#endif
  }

  /// <summary>
  /// Send event to Provider Id
  /// </summary>
  /// <param name="providerId"></param>
  /// <param name="eventData"></param>
  /// <returns></returns>
  unsigned long writeTld(Handle &providerData,
                         exporter_etw::Properties &eventData,
                         LPCGUID ActivityId        = nullptr,
                         LPCGUID RelatedActivityId = nullptr,
                         uint8_t Opcode            = 0 /* Information */)
  {
    // Make sure you stop sending event before register unregistering providerData
    if (providerData.providerHandle == INVALID_HANDLE)
    {
      // Provider not registered!
      return STATUS_ERROR;
    }

    UINT32 eventTags = MICROSOFT_EVENTTAG_NORMAL_PERSISTENCE;

    std::vector<BYTE> byteVector;
    std::vector<BYTE> byteDataVector;
    tld::EventMetadataBuilder<std::vector<BYTE>> builder(byteVector);
    tld::EventDataBuilder<std::vector<BYTE>> dbuilder(byteDataVector);

    const std::string EVENT_NAME = ETW_FIELD_NAME;
    std::string eventName        = "NoName";
    auto nameField               = eventData[EVENT_NAME];
    switch (nameField.index())
    {
      case exporter_etw::PropertyType::kTypeString:
        eventName = (char *)(nostd::get<std::string>(nameField).data());
        break;
      case exporter_etw::PropertyType::kTypeCString:
        eventName = (char *)(nostd::get<const char *>(nameField));
        break;
      default:
        // This is user error. Invalid event name!
        // We supply default 'NoName' event name in this case.
        break;
    }

    builder.Begin(eventName.c_str(), eventTags);

    for (auto &kv : eventData)
    {
      const char *name = kv.first.data();
      auto &value      = kv.second;
      switch (value.index())
      {
        case exporter_etw::PropertyType::kTypeBool: {
          builder.AddField(name, tld::TypeBool8);
          UINT8 temp = static_cast<UINT8>(nostd::get<bool>(value));
          dbuilder.AddByte(temp);
          break;
        }
        case exporter_etw::PropertyType::kTypeInt: {
          builder.AddField(name, tld::TypeInt32);
          auto temp = nostd::get<int32_t>(value);
          dbuilder.AddValue(temp);
          break;
        }
        case exporter_etw::PropertyType::kTypeInt64: {
          builder.AddField(name, tld::TypeInt64);
          auto temp = nostd::get<int64_t>(value);
          dbuilder.AddValue(temp);
          break;
        }
        case exporter_etw::PropertyType::kTypeUInt: {
          builder.AddField(name, tld::TypeUInt32);
          auto temp = nostd::get<uint32_t>(value);
          dbuilder.AddValue(temp);
          break;
        }
        case exporter_etw::PropertyType::kTypeUInt64: {
          builder.AddField(name, tld::TypeUInt64);
          auto temp = nostd::get<uint64_t>(value);
          dbuilder.AddValue(temp);
          break;
        }
        case exporter_etw::PropertyType::kTypeDouble: {
          builder.AddField(name, tld::TypeDouble);
          auto temp = nostd::get<double>(value);
          dbuilder.AddValue(temp);
          break;
        }
        case exporter_etw::PropertyType::kTypeString: {
          builder.AddField(name, tld::TypeUtf8String);
          dbuilder.AddString(nostd::get<std::string>(value).data());
          break;
        }
        case exporter_etw::PropertyType::kTypeCString: {
          builder.AddField(name, tld::TypeUtf8String);
          auto temp = nostd::get<const char *>(value);
          dbuilder.AddString(temp);
          break;
        }
#if HAVE_TYPE_GUID
          // TODO: consider adding UUID/GUID to spec
        case exporter_etw::PropertyType::kGUID: {
          builder.AddField(name.c_str(), TypeGuid);
          auto temp = nostd::get<GUID>(value);
          dbuilder.AddBytes(&temp, sizeof(GUID));
          break;
        }
#endif

        // TODO: arrays are not supported
        case exporter_etw::PropertyType::kTypeSpanByte:
        case exporter_etw::PropertyType::kTypeSpanBool:
        case exporter_etw::PropertyType::kTypeSpanInt:
        case exporter_etw::PropertyType::kTypeSpanInt64:
        case exporter_etw::PropertyType::kTypeSpanUInt:
        case exporter_etw::PropertyType::kTypeSpanUInt64:
        case exporter_etw::PropertyType::kTypeSpanDouble:
        case exporter_etw::PropertyType::kTypeSpanString:
        default:
          // TODO: unsupported type
          break;
      }
    }

    if (!builder.End())  // Returns false if the metadata is too large.
    {
      return STATUS_EFBIG;  // if event is too big for UTC to handle
    }

    tld::EventDescriptor eventDescriptor;
    eventDescriptor.Opcode = Opcode;
    eventDescriptor.Level  = 0; /* FIXME: Always on for now */

    EVENT_DATA_DESCRIPTOR pDataDescriptors[3];
    EventDataDescCreate(&pDataDescriptors[2], byteDataVector.data(),
                        static_cast<ULONG>(byteDataVector.size()));

    HRESULT writeResponse = 0;
    if ((ActivityId != nullptr) || (RelatedActivityId != nullptr))
    {
      writeResponse = tld::WriteEvent(providerData.providerHandle, eventDescriptor,
                                      providerData.providerMetaVector.data(), byteVector.data(), 3,
                                      pDataDescriptors, ActivityId, RelatedActivityId);
    }
    else
    {
      writeResponse = tld::WriteEvent(providerData.providerHandle, eventDescriptor,
                                      providerData.providerMetaVector.data(), byteVector.data(), 3,
                                      pDataDescriptors);
    }

    // Event is larger than ETW max sized of 64KB
    if (writeResponse == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW))
    {
      return STATUS_EFBIG;
    }

    return (unsigned long)(writeResponse);
  }

  unsigned long write(Handle &providerData,
                      exporter_etw::Properties &eventData,
                      LPCGUID ActivityId,
                      LPCGUID RelatedActivityId,
                      uint8_t Opcode,
                      ETWProvider::EventFormat format)
  {
    if (format == ETWProvider::EventFormat::ETW_MANIFEST)
    {
      return writeTld(providerData, eventData, ActivityId, RelatedActivityId, Opcode);
    }
    if (format == ETWProvider::EventFormat::ETW_MSGPACK)
    {
      return writeMsgPack(providerData, eventData, ActivityId, RelatedActivityId, Opcode);
    }
    if (format == ETWProvider::EventFormat::ETW_XML)
    {
      // TODO: not implemented
      return STATUS_ERROR;
    }
    return STATUS_ERROR;
  }

  static const REGHANDLE INVALID_HANDLE = _UI64_MAX;

protected:
  const unsigned int LargeEventSizeKB = 62;

  const GUID NULL_GUID = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

  mutable std::mutex m_providerMapLock;

  using ProviderMap = std::map<std::string, Handle>;

  ProviderMap &providers()
  {
    static std::map<std::string, Handle> providers;
    return providers;
  }
};

OPENTELEMETRY_END_NAMESPACE

#ifdef _MSC_VER
#  pragma warning(pop)
#endif
