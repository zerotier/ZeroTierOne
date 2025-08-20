/* ++

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    TraceLoggingDynamic.h

Abstract:

    Dynamic TraceLogging Provider API for C++.

Environment:

    User mode.

--*/

#pragma once
#include <windows.h>
#include <evntprov.h>
#include <stdlib.h> // byteswap

namespace tld
{
#pragma region Public interface

    /*
    GENERAL:

    - This implementation of manifest-free ETW supports more functionality
      than the implementation in TraceLoggingProvider.h, but it also has
      higher runtime costs. This implementation is intended for use only when
      the set of events is not known at compile-time. For example,
      TraceLoggingDynamic.h might be used to implement a library providing
      manifest-free ETW to a scripting language like JavaScript or Perl.
    - This header is not optimized for direct use by developers adding events
      to their code. There is no way to make an optimal solution that
      works for all of the intended target users. Instead, this header
      provides various pieces that you can build upon to create a user-friendly
      implementation of manifest-free ETW tailored for a specific scenario.

    HIGH-LEVEL API:

    The high-level API provides an easy way to get up and running with
    TraceLogging ETW events.

    Contents:
    - Class:    tld::Provider
    - Class:    tld::Event
    - Class:    tld::EventBuilder
    - Enum:     tld::Type

    Basic usage:
    - Create a Provider object.
    - Check provider.IsEnabled(...) so that you don't do the remaining steps
      if nobody is listening for your event.
    - Create an Event<std::vector<BYTE>> object.
    - Add fields definitions (metadata) and values (data) using methods on
      Event. (You are responsible for making sure that the metadata you add
      matches the data you add -- the Event object does not validate this.)
    - Some methods on the Event object return EventBuilder objects, which are
      used to build the fields of nested structures. Note that Event inherits
      from EventBuilder, so if you write a function that accepts an
      EventBuilder&, it will also accept an Event&.
    - Once you've added all data and metadata, call event.Write() to send the
      event to ETW.

    LOW-LEVEL API:

    The low-level API provides components that you can mix and match to build
    your own solution when the high-level components don't precisely meet your
    needs. For example, you might use the high-level Provider class to manage
    your REGHANDLE and the ETW callbacks, but you might use
    EventMetadataBuilder and EventDataBuilder directly instead of using Event
    so that you can cache event definitions.

    Contents:
    - Function: tld::RegisterProvider
    - Function: tld::UnregisterProvider
    - Function: tld::GetGuidForName
    - Function: tld::SetInformation
    - Class:    tld::ProviderMetadataBuilder
    - Class:    tld::EventMetadataBuilder
    - Class:    tld::EventDataBuilder
    - Struct:   tld::EventDescriptor
    - Class:    tld::ByteArrayWrapper
    - Enum:     tld::InType
    - Enum:     tld::OutType
    - Enum:     tld::ProviderTraitType
    - Function: tld::WriteEvent
    - Function: tld::WriteEventEx
    - Function: tld::PushBackAsUtf8
    - Function: tld::MakeType
    - Macro:    TLD_HAVE_EVENT_SET_INFORMATION
    - Macro:    TLD_HAVE_EVENT_WRITE_EX

    Notes:
    - EventDataDescCreate is a native ETW API from <evntprov.h>. It is not
      part of this API, but you may want to use it to initialize
      EVENT_DATA_DESCRIPTOR structures instead of doing it directly.
    - If you directly initialize your EVENT_DATA_DESCRIPTOR structures instead
      of using EventDataDescCreate, be sure to properly initialize the
      EVENT_DATA_DESCRIPTOR.Reserved field (e.g. by setting it to 0).
      Initializing the Reserved field is NOT optional. (EventDataDescCreate
      does correctly initialize the Reserved field.)
    - When the API asks for a byte-buffer, you can use std::vector<BYTE> or
      another similar type. If you don't want to use a vector, the provided
      ByteArrayWrapper type allows you to use your own allocation strategy for
      the buffer.
    - By default, TraceLogging events have Id=0 and Version=0, indicating
      that the event does not have an assigned Id. However, events can have
      Id and Version assigned (typically assigned manually). If you don't want
      to manage event IDs, set both Id and Version to 0. If you do assign
      IDs to your events, the Id must be non-zero and there should be a
      one-to-one mapping between {provider.Id + event.Id + event.Version} and
      {event metadata}. In other words, any event with a given non-zero
      Id+Version combination must always have exactly the same event metadata.
      (Note to decoders: this can be used as an optimization, but do not rely
      on providers to follow this rule.)
    - TraceLogging events default to channel 11 (WINEVENT_CHANNEL_TRACELOGGING).
      This channel has no effect other than to mark the event as TraceLogging-
      compatible. Other channels can be used, but channels other than 11 will
      only work if the provider is running on a version of Windows that
      understands TraceLogging (Windows 7sp1 with latest updates, Windows 8.1
      with latest updates, Windows 10, or later). If your provider is running
      on a version of Windows that does not understand TraceLogging and you use
      a channel other than 11, the resulting events will not decode correctly.

    Low-level provider management:
    - Use tld::ProviderMetadataBuilder to build a provider metadata blob.
    - If you don't have a specific provider GUID already selected, use
      tld::GetGuidForName to compute your provider GUID.
    - Use tld::RegisterProvider to open the REGHANDLE.
    - Use the REGHANDLE in calls to tld::WriteEvent.
    - Use tld::UnregisterProvider to close the REGHANDLE.

    Low-level event management:
    - Use tld::EventMetadataBuilder to build an event metadata blob.
      - Use tld::Type values to define field types, or create non-standard
        types using tld::MakeType, tld::InType, and tld::OutType.
    - Use tld::EventDataBuilder to build an event data blob.
    - Create an EVENT_DESCRIPTOR for your event.
      - Optionally use the tld::EventDescriptor wrapper class to simplify
        initialization of EVENT_DESCRIPTOR structures.
    - Allocate an array of EVENT_DATA_DESCRIPTORs. Size should be two more
      than you need for your event payload (the first two are reserved for the
      provider and event metadata).
    - Use EventDataDescCreate to fill in the data descriptor array (skipping
      the first two) to reference your event payload data.
    - Use tld::WriteEvent to write your event.
    */

    /*
    class Provider (high-level API):

    Manages an ETW REGHANDLE and the provider metadata blob.
    - Constructor creates the provider metadata blob and registers the provider.
      Note that this includes configuring ETW callbacks.
    - Destructor unregisters the provider and frees the metadata blob.
    - IsEnabled APIs provide very efficient checks for whether an event needs
      to be written.
    - Write APIs call WriteEvent, automatically providing the correct REGHANDLE
      and provider metadata blobs.

    Typical usage:

    tld::Provider provider("ProviderName", ...);
    if (provider.IsEnabled(eventLevel, eventKeywords))
    {
        (prepare event);
        provider.Write(...);
    }

    It is not a problem to call provider.Write without checking
    provider.IsEnabled(...), but it is more efficient to check
    provider.IsEnabled(...) so that you can skip the process of preparing the
    event if the event is not enabled.
    */
    class Provider;

    /*
    class Event (high-level API):

    Manages the data and metadata for an event. You create an Event object,
    set properties, add field definitions, add field values, and then Write
    the event.

    ByteVectorTy will usually be std::vector<BYTE>, though other similar
    types could be used instead.

    Typical usage:

    if (provider.IsEnabled(eventLevel, eventKeywords))
    {
        Event event(provider, szEventName, eventLevel, eventKeywords);
        (add fields definitions and values);
        event.Write();
    }

    You can reuse an Event object by calling event.Reset(...).
    */
    template<class ByteVectorTy>
    class Event;

    /*
    class EventBuilder (high-level API):

    This class exposes an interface for adding field metadata/data to an event.
    The Event class inherits from EventBuilder so that it can expose this
    interface. In addition, each call to eventBuilder.AddStruct(...) returns a
    new EventBuilder that can be used to add fields to a nested structure.
    You won't directly create instances of this type -- you'll use instances
    returned by Event.AddStruct or by EventBuilder.AddStruct.

    You can use EventBuilder& as the type of a parameter to a function, and
    then pass either an Event or an EventBuilder to that function (allowing
    for recursive processing of event fields).

    Note that order is important but not checked when using multiple Event or
    EventBuilder objects. Fields are always added to the end of the overall
    event, regardless of which Event or EventBuilder you use to add the field.
    The separate Event and EventBuilder objects are used for the sole purpose
    of tracking the number of fields in each struct.

    Assume that you have obtained EventBuilder object b from object a, i.e.
    EventBuilder b = a.AddStruct(...). You must complete all operations using
    b before resuming any operations using a. If this rule is not followed,
    fields may end up nesting in unexpected ways.
    */
    template<class ByteVectorTy>
    class EventBuilder;

    /*
    enum Type (high-level API and low-level API):

    Types for event fields, corresponding to a combination of TDH_INTYPE and
    TDH_OUTTYPE values. The Type enumeration contains predefined combinations
    of InType and OutType that are known to work well and are recognized by
    decoders such as TDH and xperf.

    Advanced use: It is possible to create other Type values by combining
    values from the InType and OutType enumerations using the tld::MakeType
    function. (Valid Type values consist of one InType combined with one
    OutType.) However, custom combinations not already present in the Type
    enumeration are unlikely to be recognized by xperf or other decoders,
    and will usually end up being decoded as if they had used OutTypeDefault.

    When providing payload for a field, the payload data must match the InType.
    Payload is always packed tightly with no alignment or padding.

    ****

    How to pack single values:
    
    Primitive values (int, GUID) are packed directly, with no padding or
    alignment. Just reserve sizeof(value) bytes in the buffer, then memcpy the
    value into the buffer. Note that there should be no alignment or padding
    between items -- the payload is always tightly-packed.

    NUL-terminated strings are also simply memcpy'ed into the buffer. Be sure
    to include the NUL termination when reserving space and copying the value.
    There is no special encoding reserved for a NULL string. The encoding
    helpers in this header simply treat a NULL string the same as an empty
    string, i.e. AddString((char*)NULL) is the same as AppendString("").
    
    Binary, CountedString, and CountedAnsiString scalars are all encoded as a
    UINT16 byte-count followed by the string data. In this case, no NUL
    termination should be included. Remember that the size is a byte count,
    not a character count.

    ****

    How to pack arrays:

    Assume you have a function Pack(payloadVec, item) that correctly appends an
    item to the end of payloadVec.

    Packing a variable-length array of N items is done by appending N (encoded
    as UINT16) to payloadVec followed by calling Pack(...) N times.

    Packing a constant-length array of N items is done by calling Pack(...) N
    times. The value of N is encoded in the metadata (it was provided in the
    field's declaration) so it does not need to be provided in the payload.

    Note: while this header allows you to create arrays of anything, you
    should avoid creating arrays of the following, as they might not decode
    correctly:
    - TypeNone (or anything based on InTypeNull).
    - TypeBinary (or anything based on InTypeBinary).

    It is ok to create an array of nested structures where fields in the
    structure have TypeNone or TypeBinary.
    */
    enum Type : UINT16;

    /*
    Used for composing Type values.
    Normally you'll use a precomposed Type... value, but you can compose a
    custom Type... value by combining a value from InType with a value from
    OutType using MakeType.

    The InType tells the pipeline how to encode the field's payload (primarily
    how to determine payload size). In addition, each InType has an implicit
    default formatting behavior that will be used if not overridden by an
    OutType.

    The comments for each InType value indicate the payload encoding rules and
    the OutTypes that are most likely to be usable with this InType.
    */
    enum InType : UINT8;

    /*
    Used for composing Type values.
    Normally you'll use a precomposed Type... value, but you can compose a
    custom Type... value by combining a value from InType with a value from
    OutType using MakeType.

    The OutType gives the pipeline a formatting hint that the trace consumer
    may use to override the InType's default formatting behavior. If no
    OutType is specified (i.e. if OutTypeDefault is used) or if the trace
    consumer does not recognize the specified InType+OutType combination, the
    trace consumer will perform default formatting based solely on InType.

    The comments for each OutType indicate the InTypes for which the OutType
    might have valid semantics. However, most trace consumer only recognize a
    small subset of the "valid" InType+OutType combinations, so even if the
    semantics are valid, the OutType might still be ignored by the trace
    consumer. The most commonly-supported combinations are the combinations
    with corresponding precomposed Type... values.
    */
    enum OutType : UINT8;

    /*
    enum ProviderTraitType (low-level API):

    The type of a provider trait. Used when building up provider metadata.
    */
    enum ProviderTraitType : UINT8;

    /*
    macro TLD_HAVE_EVENT_SET_INFORMATION (low-level API):

    Configuration macro for controlling the behavior of SetInformation.

    Not all versions of Windows support the EventSetInformation API. The
    tld::SetInformation wrapper provides default behavior that works well in
    most cases, but may not meet the needs of all users. The configuration
    macros can be used to control how the tld::SetInformation function invokes
    the EventSetInformation API. (Note that tld::SetInformation is called
    automatically by the Provider class's constructor to register the
    provider's traits with ETW.)

    When TLD_HAVE_EVENT_SET_INFORMATION is not defined and WINVER < 0x0602:
    SetInformation uses GetModuleHandleExW+GetProcAddress to find the
    EventSetInformation function. If found, SetInformation calls it and
    returns whatever EventSetInformation returns. Otherwise, SetInformation
    returns an error.

    When TLD_HAVE_EVENT_SET_INFORMATION is not defined and WINVER >= 0x0602:
    SetInformation directly calls the EventSetInformation(...) function and
    returns whatever EventSetInformation returns.

    If you set TLD_HAVE_EVENT_SET_INFORMATION:
    - If TLD_HAVE_EVENT_SET_INFORMATION == 0, SetInformation always returns an
      error.
    - If TLD_HAVE_EVENT_SET_INFORMATION == 1, SetInformation calls
      EventSetInformation.
    - If TLD_HAVE_EVENT_SET_INFORMATION == 2, SetInformation locates
      EventSetInformation via GetProcAddress.
    */
#ifndef TLD_HAVE_EVENT_SET_INFORMATION
  #if WINVER < 0x0602 // If targeting Windows before Windows 8
    #define TLD_HAVE_EVENT_SET_INFORMATION 2 // Find "EventSetInformation" via GetModuleHandleExW/GetProcAddress
  #else
    #define TLD_HAVE_EVENT_SET_INFORMATION 1 // Directly invoke EventSetInformation(...)
  #endif
#endif // TLD_HAVE_EVENT_SET_INFORMATION

    /*
    macro TLD_HAVE_EVENT_WRITE_EX (low-level API):

    Configuration macro for enabling/disabling WriteEventEx, WriteEx.

    If TLD_HAVE_EVENT_WRITE_EX is not defined and WINVER < 0x0601, or
    if TLD_HAVE_EVENT_WRITE_EX is defined as 0,
    then the WriteEventEx, WriteEx, and related APIs will not be available.

    If TLD_HAVE_EVENT_WRITE_EX is not defined and WINVER >= 0x0601, or
    if TLD_HAVE_EVENT_WRITE_EX is defined as 1,
    then the WriteEventEx, WriteEx, and related APIs will be available.
    */
#ifndef TLD_HAVE_EVENT_WRITE_EX
  #if WINVER < 0x0601 // If targeting Windows before Windows 7
    #define TLD_HAVE_EVENT_WRITE_EX 0
  #else
    #define TLD_HAVE_EVENT_WRITE_EX 1
  #endif
#endif // TLD_HAVE_EVENT_WRITE_EX


    /*
    class ProviderMetadataBuilder (low-level API):

    Helper for building the provider metadata blob.
    The type provided for ByteBufferTy must behave like a std::vector<BYTE>.

    Example usage:

        std::vector<BYTE> byteVector;
        tld::ProviderMetadataBuilder<std::vector<BYTE>> builder(byteVector);
        builder.Begin(szProviderName); // Note: calls byteVector.clear().
        if (builder.End()) // Returns false if the metadata is too large.
        {
            // byteVector now contains a valid provider metadata blob.
        }
    */
    template<class ByteBufferTy>
    class ProviderMetadataBuilder;

    /*
    class EventMetadataBuilder (low-level API):

    Helper for building the event metadata blob, including field definitions.
    The type provided for ByteBufferTy must behave like a std::vector<BYTE>.

    Example usage:

        std::vector<BYTE> byteVector;
        tld::EventMetadataBuilder<std::vector<BYTE>> builder(byteVector);
        builder.Begin(eventName); // Note: calls byteVector.clear().
        builder.AddField("Field1", TypeBOOL32); // top-level field
        auto  struct1 = builder.AddStruct("Struct1");
        struct1.AddField("Nested1", TypeINT32) // nested field
        struct1.AddField("Nested2", TypeANSISTRING); // nested field
        if (builder.End()) // Returns false if the metadata is too large.
        {
            // byteVector now contains a valid event metadata blob.
        }
    */
    template<class ByteBufferTy>
    class EventMetadataBuilder;

    /*
    class EventDataBuilder (low-level API):

    Helpers for adding event data to an event data blob.
    The type provided for ByteBufferTy must behave like a std::vector<BYTE>.
    */
    template<class ByteBufferTy>
    class EventDataBuilder;

    /*
    class EventDescriptor (low-level API):

    Derives from EVENT_DESCRIPTOR. Adds convenient constructor and Reset
    members. Contains an event's Id/Version/Level/Opcode/Task/Keyword
    settings. Use of this class is optional - you can use this class if you
    want to use the constructor or Reset methods, or you can use
    EVENT_DESCRIPTOR directly if you don't need the convenience methods.
    */
    struct EventDescriptor;

    /*
    class ByteArrayWrapper (low-level API):

    Adapter that allows a regular BYTE array (stack-allocated or
    manually-allocated) to be used as the buffer in MetadataBuilder
    operations. If the underlying BYTE array is too small, or if no array is
    provided at all, this adapter will stop writing data beyond the end of the
    array, but will continue to "accept" data (and will increment it size
    member accordingly). This allows you to determine the size of the required
    array so that the operation can be retried with a correctly-sized array.

    Example usage:

        // Create a wrapper with a default (0-length) buffer.
        tld::ByteArrayWrapper wrapper;

        // Create a metadata builder attached to the wrapper.
        tld::ProviderMetadataBuilder<tld::ByteArrayWrapper> builder(wrapper);

        // Determine the size needed for provider metadata.
        builder.Begin(providerName);
        builder.End();

        // Allocate a real buffer and reset the wrapper to use it.
        BYTE* pBuffer = new BYTE[wrapper.size()];
        wrapper.reset(pBuffer, wrapper.size());

        // Redo the operation with the real buffer.
        builder.Begin(providerName);
        if (builder.End())
        {
            // pBuffer now contains a valid provider metadata blob.
        }
    */
    class ByteArrayWrapper;

    /*
    function RegisterProvider (low-level API):

    Calls EventRegister. If EventRegister succeeds, calls EventSetInformation
    to register your provider traits with ETW. Use this instead of calling
    EventRegister directly to ensure that your provider traits are properly
    registered.
    */
    inline HRESULT RegisterProvider(
        _Out_ REGHANDLE* phProvider,
        _In_ GUID const* pProviderId,
        _In_count_x_((UINT16*)pProviderMetadata) UINT8 const* pProviderMetadata,
        _In_opt_ PENABLECALLBACK pEnableCallback = 0,
        _In_opt_ void* pEnableCallbackContext = 0);

    /*
    function UnregisterProvider (low-level API):

    Calls EventUnregister.
    */
    inline HRESULT UnregisterProvider(
        REGHANDLE hProvider);

    /*
    function WriteEvent (low-level API):

    Calls EventWriteTransfer. You must provide 2 more data descriptors than
    are required for your data. The first two elements in pDataDescriptors
    will be used for provider and event metadata. The actual payload (if any)
    is expected to be in the remaining elements of pDataDescriptors.

    Example usage:

        tld::EventDescriptor eventDescriptor(
            level,
            opcode,
            task,
            keywords);
        EVENT_DATA_DESCRIPTOR pDataDescriptors[cFields + 2];
        for (int i = 0; i < cFields; i++)
        {
            EventDataDescCreate(&pDataDescriptors[i + 2], ...);
        }
        tld::WriteEvent(
            hProvider,
            eventDescriptor,
            pProviderMetadata,
            pEventMetadata,
            cFields + 2,
            pDataDescriptors);
    */
    inline HRESULT WriteEvent(
        _In_ REGHANDLE hProvider,
        _In_ EVENT_DESCRIPTOR const& eventDescriptor,
        _In_count_x_((UINT16*)pProviderMetadata) UINT8 const* pProviderMetadata,
        _In_count_x_((UINT16*)pEventMetadata) UINT8 const* pEventMetadata,
        _In_range_(2, 128) ULONG cDataDescriptors,
        _Inout_count_(cDataDescriptors) EVENT_DATA_DESCRIPTOR* pDataDescriptors,
        _In_opt_ GUID const* pActivityId = 0,
        _In_opt_ GUID const* pRelatedActivityId = 0);

#if TLD_HAVE_EVENT_WRITE_EX == 1

    /*
    function WriteEventEx (low-level API):
    Calls EventWriteEx. Otherwise works like the WriteEvent function.
    */
    inline HRESULT WriteEventEx(
        _In_ REGHANDLE hProvider,
        _In_ EVENT_DESCRIPTOR const& eventDescriptor,
        _In_count_x_((UINT16*)pProviderMetadata) UINT8 const* pProviderMetadata,
        _In_count_x_((UINT16*)pEventMetadata) UINT8 const* pEventMetadata,
        _In_range_(2, 128) ULONG cDataDescriptors,
        _Inout_count_(cDataDescriptors) EVENT_DATA_DESCRIPTOR* pDataDescriptors,
        ULONG64 filter = 0,
        ULONG flags = 0,
        _In_opt_ GUID const* pActivityId = 0,
        _In_opt_ GUID const* pRelatedActivityId = 0);

#endif // TLD_HAVE_EVENT_WRITE_EX

    /*
    function SetInformation (low-level API):

    Wrapper for ETW API EventSetInformation.
    If TraceLoggingDynamic.cpp was compiled to require Win8 or later (as
    determined by WINVER), this directly calls EventSetInformation. Otherwise,
    this attempts to dynamically load the EventSetInformation API via
    GetModuleHandleExW.

    The behavior of this function (e.g. to override the WINVER check) can be
    adjusted by setting the TLD_HAVE_EVENT_SET_INFORMATION macro as described
    below.
    */
    inline HRESULT SetInformation(
        _In_ REGHANDLE hProvider,
        EVENT_INFO_CLASS informationClass,
        _In_reads_bytes_opt_(cbInformation) void* pbInformation,
        ULONG cbInformation);

    /*
    function GetGuidForName (low-level API):

    Hashes a provider name to generate a GUID. Uses the same GUID generation
    algorithm as System.Diagnostics.Tracing.EventSource (from .NET) and
    Windows.Foundation.Diagnostics.LoggingChannel (from Windows Runtime).
    */
    inline GUID GetGuidForName(
        _In_z_ char const* szUtf8Name);

    /*
    function GetGuidForName (low-level API):

    Hashes a provider name to generate a GUID. Uses the same GUID generation
    algorithm as .NET System.Diagnostics.Tracing.EventSource and Windows
    Runtime Windows.Foundation.Diagnostics.LoggingChannel.
    */
    inline GUID GetGuidForName(
        _In_z_ wchar_t const* szUtf16Name);

    /*
    function PushBackAsUtf8 (low-level API):

    Transcodes a NUL-terminated UTF-16LE string to a NUL-terminated UTF-8
    string. Intended for use when appending provider/event/field names to
    metadata buffers.
    */
    template<class ByteBufferTy>
    void PushBackAsUtf8(
        ByteBufferTy& buffer,
        _In_z_ wchar_t const* szUtf16);

#pragma endregion

    namespace detail
    {
#pragma region Internal macros

#define _tld_MAKE_TYPE(inType, outType) \
    static_cast<::tld::Type>((inType) | (static_cast<int>(outType) << 8))

#ifndef TLD_DEBUG
  #if (DBG || defined(DEBUG) || defined(_DEBUG)) && !defined(NDEBUG)
    #define TLD_DEBUG 1
  #else // DBG
    #define TLD_DEBUG 0
  #endif // DBG
#endif // TLD_DEBUG

#ifndef _tld_ASSERT
  #if TLD_DEBUG
    #define _tld_ASSERT(exp, str) ((void)(!(exp) ? (__annotation(L"Debug", L"AssertFail", L"TraceLogging: " L#exp L" : " L##str), DbgRaiseAssertionFailure(), 0) : 0))
  #else // TLD_DEBUG
    #define _tld_ASSERT(exp, str) ((void)0)
  #endif // TLD_DEBUG
#endif // _tld_ASSERT

#pragma endregion

#pragma region MetadataBuilderBase

        template<class ByteBufferTy>
        class MetadataBuilderBase
        {
            ByteBufferTy& m_buffer;

            MetadataBuilderBase(MetadataBuilderBase const&); // = delete
            void operator=(MetadataBuilderBase const&); // = delete

        protected:

            explicit MetadataBuilderBase(ByteBufferTy& buffer)
                : m_buffer(buffer)
            {
                return;
            }

            void BaseBegin()
            {
                m_buffer.clear();
                AddU16(0); // Metadata size filled in by BaseEnd
            }

            bool BaseEnd()
            {
                auto size = m_buffer.size();
                _tld_ASSERT(2 <= size, "Begin was not called");
                SetU16(0, static_cast<UINT16>(size));
                return size < 32768;
            }

            void AddBytes(
                _In_reads_bytes_(cb) void const* p,
                unsigned cb)
            {
                auto pb = static_cast<UINT8 const*>(p);
                for (unsigned i = 0; i != cb; i++)
                {
                    m_buffer.push_back(pb[i]);
                }
            }

            void AddString(
                _In_z_ char const* szUtf8)
            {
                for (unsigned i = 0;; i++)
                {
                    m_buffer.push_back(szUtf8[i]);
                    if (szUtf8[i] == 0)
                    {
                        break;
                    }
                }
            }

            void AddString(
                _In_z_ wchar_t const* szUtf16)
            {
                PushBackAsUtf8(m_buffer, szUtf16);
            }

            void AddU8(
                UINT8 val)
            {
                m_buffer.push_back(val);;
            }

            void AddU16(
                UINT16 val)
            {
                m_buffer.push_back(static_cast<UINT8>(val));
                m_buffer.push_back(static_cast<UINT8>(val >> 8));
            }

            UINT32 GetBookmark()
            {
                return static_cast<UINT32>(m_buffer.size());
            }

            void SetU8(
                UINT32 bookmark,
                UINT8 val)
            {
                _tld_ASSERT(bookmark < m_buffer.size(), "bookmark out of range");
                m_buffer[bookmark] = val;
            }

            void SetU16(
                UINT32 bookmark,
                UINT16 val)
            {
                _tld_ASSERT(1 <= bookmark + 1, "bookmark out of range");
                SetU8(bookmark + 0, static_cast<UINT8>(val));
                SetU8(bookmark + 1, static_cast<UINT8>(val >> 8));
            }

            void IncrementU7(
                UINT32 bookmark)
            {
                _tld_ASSERT(bookmark < m_buffer.size(), "bookmark out of range");
                UINT8 result;
                result = ++m_buffer[bookmark];
                _tld_ASSERT((result & 0x7f) != 0, "too many fields in struct");
            }

        public:

            ByteBufferTy& GetBuffer()
            {
                return m_buffer;
            }

            ByteBufferTy const& GetBuffer() const
            {
                return m_buffer;
            }
        };

#pragma endregion

#pragma region Sha1ForNonSecretPurposes

        /*
        Implements the SHA1 hashing algorithm. Note that this implementation is
        for hashing public information. Do not use this code to hash private data,
        as this implementation does not take any steps to avoid information
        disclosure (i.e. does not scrub its buffers).
        */
        class Sha1ForNonSecretPurposes
        {
            Sha1ForNonSecretPurposes(Sha1ForNonSecretPurposes const&); // = delete
            void operator=(Sha1ForNonSecretPurposes const&); // = delete

            UINT64 m_length; // Total message length in bits
            unsigned m_pos; // Length of current chunk in bytes
            UINT32 m_results[5];
            UINT32 m_w[80]; // Workspace

        public:

            Sha1ForNonSecretPurposes()
                : m_length()
                , m_pos()
            {
                m_results[0] = 0x67452301;
                m_results[1] = 0xEFCDAB89;
                m_results[2] = 0x98BADCFE;
                m_results[3] = 0x10325476;
                m_results[4] = 0xC3D2E1F0;
            }

            void Append(
                _In_reads_bytes_(cbInput) void const* pInput,
                unsigned cbInput)
            {
                for (unsigned i = 0; i != cbInput; i++)
                {
                    Append(static_cast<UINT8 const*>(pInput)[i]);
                }
            }

            void Append(UINT8 input)
            {
                reinterpret_cast<UINT8*>(m_w)[m_pos++] = input;
                if (64 == m_pos)
                {
                    Drain();
                }
            }

            _Ret_bytecount_(20) UINT8* Finish()
            {
                UINT64 totalBitCount = m_length + 8 * m_pos;
                Append(0x80);
                while (m_pos != 56)
                {
                    Append(0x00);
                }

                *reinterpret_cast<UINT64*>(m_w + 14) = _byteswap_uint64(totalBitCount);
                Drain();

                for (unsigned i = 0; i != 5; i++)
                {
                    m_results[i] = _byteswap_ulong(m_results[i]);
                }

                return reinterpret_cast<UINT8*>(m_results);
            }

        private:

            void Drain()
            {
                for (unsigned i = 0; i != 16; i++)
                {
                    m_w[i] = _byteswap_ulong(m_w[i]);
                }

                for (unsigned i = 16; i != 80; i++)
                {
                    m_w[i] = _rotl((m_w[i - 3] ^ m_w[i - 8] ^ m_w[i - 14] ^ m_w[i - 16]), 1);
                }

                UINT32 a = m_results[0];
                UINT32 b = m_results[1];
                UINT32 c = m_results[2];
                UINT32 d = m_results[3];
                UINT32 e = m_results[4];

                for (unsigned i = 0; i != 20; i++)
                {
                    UINT32 const k = 0x5A827999;
                    UINT32 f = (b & c) | ((~b) & d);
                    UINT32 temp = _rotl(a, 5) + f + e + k + m_w[i]; e = d; d = c; c = _rotl(b, 30); b = a; a = temp;
                }

                for (unsigned i = 20; i != 40; i++)
                {
                    const UINT32 k = 0x6ED9EBA1;
                    UINT32 f = b ^ c ^ d;
                    UINT32 temp = _rotl(a, 5) + f + e + k + m_w[i]; e = d; d = c; c = _rotl(b, 30); b = a; a = temp;
                }

                for (unsigned i = 40; i != 60; i++)
                {
                    const UINT32 k = 0x8F1BBCDC;
                    UINT32 f = (b & c) | (b & d) | (c & d);
                    UINT32 temp = _rotl(a, 5) + f + e + k + m_w[i]; e = d; d = c; c = _rotl(b, 30); b = a; a = temp;
                }

                for (unsigned i = 60; i != 80; i++)
                {
                    const UINT32 k = 0xCA62C1D6;
                    UINT32 f = b ^ c ^ d;
                    UINT32 temp = _rotl(a, 5) + f + e + k + m_w[i]; e = d; d = c; c = _rotl(b, 30); b = a; a = temp;
                }

                m_results[0] += a;
                m_results[1] += b;
                m_results[2] += c;
                m_results[3] += d;
                m_results[4] += e;

                m_length += 512; // 64 bytes == 512 bits
                m_pos = 0;
            }
        };

#pragma endregion

#pragma region CopyUtfCodePoint

        /*
        Transcodes one code point from UTF-8 to UTF-16LE.
    
        Note that this function requires the input buffer to be nul-terminated.
        This function may try to read several bytes from the input buffer, and the
        nul-termination is required to ensure that it doesn't read off the end of
        the valid buffer when decoding what appears to be a multi-byte code point.

        The UTF-8 validation is very permissive -- anything that is not valid
        UTF-8 is treated as if it were ISO-8859-1 (i.e. the BYTE value is cast
        to wchar_t and assumed to be a valid Unicode code point). This usually
        works well - it has a reasonable probability of doing what the developer
        expects even when the input is CP1252/Latin1 instead of UTF-8, and doesn't
        fail too badly when it doesn't do what the developer expects.

        pchUtf16Output:
        Buffer that receives a single code point, encoded in UTF-16-LE as one or
        two 16-bit wchar_t values.
    
        pszUtf8Input:
        Pointer into a nul-terminated UTF-8 byte sequence. On entry, this points
        to the next char to be consumed. This function will advance the pointer
        past the consumed data.

        returns:
        The number of 16-bit wchar_t values added to pchUtf16Output (one or two).
        */
        static unsigned CopyUtfCodePoint(
            _Out_writes_to_(2, return) wchar_t* pchUtf16Output,
            char const*& pszUtf8Input)
        {
            unsigned cchOutput = 1;
            unsigned ch = static_cast<unsigned char>(*pszUtf8Input);
            pszUtf8Input += 1;
            if (ch <= 0xbf)
            {
                // 0x01..0x7f: ASCII - pass through.
                // 0x80..0xbf: Invalid - pass through.
            }
            else if ((pszUtf8Input[0] & 0xc0) != 0x80)
            {
                // Invalid trail byte - pass through.
            }
            else if (ch <= 0xdf)
            {
                // 0xc0..0xdf: Two-byte encoding for 0x0080..0x07ff.
                unsigned ch2 = ((ch & 0x1f) << 6)
                    | (pszUtf8Input[0] & 0x3f);
                if (0x0080 <= ch2)
                {
                    ch = ch2;
                    pszUtf8Input += 1;
                }
            }
            else if ((pszUtf8Input[1] & 0xc0) != 0x80)
            {
                // Invalid trail byte - pass through.
            }
            else if (ch <= 0xef)
            {
                // 0xe0..0xef: Three-byte encoding for 0x0800..0xffff.
                unsigned ch2 = ((ch & 0x0f) << 12)
                    | ((pszUtf8Input[0] & 0x3f) << 6)
                    | (pszUtf8Input[1] & 0x3f);
                if (0x0800 <= ch2)
                {
                    ch = ch2;
                    pszUtf8Input += 2;
                }
            }
            else if ((pszUtf8Input[2] & 0xc0) != 0x80)
            {
                // Invalid trail byte - pass through.
            }
            else if (ch <= 0xf4)
            {
                // 0xf0..0xf4: Four-byte encoding for 0x010000..0x10ffff.
                unsigned ch2 = ((ch & 0x07) << 18)
                    | ((pszUtf8Input[0] & 0x3f) << 12)
                    | ((pszUtf8Input[1] & 0x3f) << 6)
                    | (pszUtf8Input[2] & 0x3f);
                ch2 -= 0x10000;
                if (ch2 <= 0xfffff)
                {
                    // Decode into surrogate pair (2 wchar_t units)
                    pchUtf16Output[1] = static_cast<wchar_t>(0xdc00 | (ch2 & 0x3ff));
                    ch = 0xd800 | (ch2 >> 10);
                    pszUtf8Input += 3;
                    cchOutput = 2;
                }
            }

            pchUtf16Output[0] = static_cast<wchar_t>(ch);
            return cchOutput;
        }

        /*
        Copies one code point of UTF-16LE data.

        Note that this function requires the input buffer to be nul-terminated.
        This function may try to read several words from the input buffer, and the
        nul-termination is required to ensure that it doesn't read off the end of
        the valid buffer when copying what appears to be a surrogate pair.

        pchUtf16Output:
        Buffer that receives a single code point, encoded in UTF-16-LE as one or
        two 16-bit wchar_t values.

        pszUtf16Input:
        Pointer into a nul-terminated UTF-16-LE wchar_t sequence. On entry, this
        points to the next wchar_t to be consumed. This function will advance the
        pointer past the consumed data.

        returns:
        The number of 16-bit wchar_t values added to pchUtf16Output (one or two).
        */
        static unsigned CopyUtfCodePoint(
            _Out_writes_to_(2, return) wchar_t* pchUtf16Output,
            wchar_t const*& pszUtf16Input)
        {
            unsigned cchOutput = 1;
            wchar_t ch = *pszUtf16Input;
            pszUtf16Input += 1;
            if (0xd800 <= ch && ch < 0xdc00 &&
                0xdc00 <= *pszUtf16Input && *pszUtf16Input < 0xe000)
            {
                pchUtf16Output[1] = *pszUtf16Input;
                pszUtf16Input += 1;
                cchOutput = 2;
            }
            pchUtf16Output[0] = ch;
            return cchOutput;
        }

#pragma endregion

#pragma region GetGuidForNameImpl

        __declspec(selectany) extern UINT32 const NamespaceValue[] = {
            0xB22D2C48, 0xC84790C3, 0x151AF887, 0xFB30C1Bf
        };

        template<class CharTy>
        GUID GetGuidForNameImpl(
            _In_z_ CharTy const* szUtfName)
        {
            /*
            Algorithm:
            szNameUpperBE = ByteSwap(ToUpper(szName));
            hash = SHA1(namespaceBytes + szNameUpperBE); // Does not include zero-termination in hash.
            hash[7] = (hash[7] & 0x0F) | 0x50;
            guid = First16Bytes(hash);
            */

            WCHAR buffer[16];
            unsigned const cchBuffer = ARRAYSIZE(buffer);
            Sha1ForNonSecretPurposes sha1;

            sha1.Append(NamespaceValue, sizeof(NamespaceValue));
            if (szUtfName[0] != 0)
            {
                CharTy const* pszName = szUtfName;
                unsigned iBuffer = 0;
                for (;;)
                {
                    // CopyUtfCodePoint may add up to 2 wchar_t units to buffer.
                    iBuffer += CopyUtfCodePoint(buffer + iBuffer, pszName);
                    _tld_ASSERT(iBuffer <= cchBuffer, "buffer overflow");

                    bool const bDone = *pszName == 0;
                    if (bDone || cchBuffer - 1 <= iBuffer)
                    {
    #pragma warning(suppress: 26035) // string is counted, not nul-terminated.
                        LCMapStringEx(
                            LOCALE_NAME_INVARIANT,
                            LCMAP_UPPERCASE | LCMAP_BYTEREV,
                            buffer,
                            iBuffer,
                            buffer,
                            iBuffer,
                            NULL,
                            NULL,
                            0);
                        sha1.Append(buffer, iBuffer * 2);

                        if (bDone)
                        {
                            break;
                        }

                        iBuffer = 0;
                    }
                }
            }

            UINT8* pSha1 = sha1.Finish();

            // Set high 4 bits of octet 7 to 5, as per RFC 4122
            pSha1[7] = (pSha1[7] & 0x0f) | 0x50;

            return *reinterpret_cast<GUID*>(pSha1);
        }

#pragma endregion
    }
    // namespace detail

#pragma region Enumerations

    enum InType : UINT8
    {
        /*
        A field with no value (0-length payload).
        Arrays of InTypeNull are illegal.
        */
        InTypeNull,

        /*
        A nul-terminated CHAR16 string.
        Useful OutTypes: Xml, Json.
        */
        InTypeUnicodeString,

        /*
        A nul-terminated CHAR8 string.
        Useful OutTypes: Xml, Json, Utf8.
        */
        InTypeAnsiString,

        /*
        INT8.
        */
        InTypeInt8,

        /*
        UINT8.
        Useful OutTypes: String, Boolean, Hex.
        */
        InTypeUInt8,

        /*
        INT16.
        */
        InTypeInt16,

        /*
        UINT16.
        Useful OutTypes: String, Hex, Port.
        */
        InTypeUInt16,

        /*
        INT32.
        Useful OutTypes: HResult.
        */
        InTypeInt32,

        /*
        UINT32.
        Useful OutTypes: Pid, Tid, IPv4, Win32Error, NTStatus.
        */
        InTypeUInt32,

        /*
        INT64.
        */
        InTypeInt64,

        /*
        UINT64.
        */
        InTypeUInt64,

        /*
        FLOAT.
        */
        InTypeFloat,

        /*
        DOUBLE.
        */
        InTypeDouble,

        /*
        BOOL.
        */
        InTypeBool32,

        /*
        UINT16 byte-count followed by binary data.
        Useful OutTypes: IPv6, SocketAddress.
        Arrays of InTypeBinary are not supported. (No error will be reported,
        but decoding tools will probably not be able to decode them).
        */
        InTypeBinary,

        /*
        GUID.
        */
        InTypeGuid,

        /*
        Not supported. Use InTypePointer. (No error will be reported,
        but decoding tools will probably not be able to decode them).
        */
        InTypePointer_PlatformSpecific,

        /*
        FILETIME.
        */
        InTypeFileTime,

        /*
        SYSTEMTIME.
        */
        InTypeSystemTime,

        /*
        SID. Size is determined from the number of subauthorities.
        */
        InTypeSid,

        /*
        INT32.
        */
        InTypeHexInt32,

        /*
        INT64.
        */
        InTypeHexInt64,

        /*
        UINT16 byte-count followed by UTF-16 data.
        */
        InTypeCountedString,

        /*
        UINT16 byte-count followed by MBCS data.
        */
        InTypeCountedAnsiString,

        /*
        This field has no value by itself, but it causes the decoder to treat
        the next N fields as part of this field. The value of N is taken from
        the field's OutType.

        Note that it is valid to have an array of structs. The array length
        will apply to the entire group of fields contained in the array.

        The Struct type is special. Do not use it directly. Use helper APIs to
        create nested structures.
        */
        InTypeStruct,

        /*
        INT_PTR.
        InTypeIntPtr is an alias for either InTypeInt32 or InTypeInt64.
        */
        InTypeIntPtr = sizeof(void*) == 8 ? InTypeInt64 : InTypeInt32,

        /*
        UINT_PTR.
        InTypeUIntPtr is an alias for either InTypeUInt32 or InTypeUInt64.
        */
        InTypeUIntPtr = sizeof(void*) == 8 ? InTypeUInt64 : InTypeUInt32,

        /*
        LPVOID.
        InTypePointer is an alias for either InTypeHexInt32 or InTypeHexInt64.
        */
        InTypePointer = sizeof(void*) == 8 ? InTypeHexInt64 : InTypeHexInt32,

        /*
        InType must fit in 5 bits.
        */
        InTypeMask = 31
    };

    enum OutType : UINT8
    {
        OutTypeDefault = 0x00,
        OutTypeNoPrint = 0x01,
        OutTypeString = 0x02,    // affects 8-bit and 16-bit integral types.
        OutTypeBoolean = 0x03,   // affects 8-bit and 32-bit integral types.
        OutTypeHex = 0x04,       // affects integral types.
        OutTypePid = 0x05,       // affects 32-bit integral types.
        OutTypeTid = 0x06,       // affects 32-bit integral types.
        OutTypePort = 0x07,      // affects 16-bit integral types.
        OutTypeIPv4 = 0x08,      // affects 32-bit integral types.
        OutTypeIPv6 = 0x09,      // affects arrays of 8-bit integral types, e.g. UInt8[].
        OutTypeSocketAddress = 0x0a, // affects arrays of 8-bit integral types, e.g. UInt8[].
        OutTypeXml = 0x0b,       // affects strings; affects arrays of 8-bit and 16-bit integral types.
        OutTypeJson = 0x0c,      // affects strings; affects arrays of 8-bit and 16-bit integral types.
        OutTypeWin32Error = 0x0d,// affects 32-bit integral types.
        OutTypeNTStatus = 0x0e,  // affects 32-bit integral types.
        OutTypeHResult = 0x0f,   // affects 32-bit integral types.
        OutTypeFileTime = 0x10,  // affects 64-bit integral types.
        OutTypeSigned = 0x11,    // affects integral types.
        OutTypeUnsigned = 0x12,  // affects integral types.
        OutTypeDateTimeCultureInsensitive = 0x21, // affects FileTime, SystemTime.
        OutTypeUtf8 = 0x23,      // affects AnsiString types.
        OutTypePkcs7WithTypeInfo = 0x24, // affects binary types.
        OutTypeCodePointer = 0x25, // affects UInt32, UInt64, HexInt32, HexInt64.
        OutTypeDateTimeUtc = 0x26, // affects FileTime, SystemTime.
        OutTypeMask = 0x7f // OutType must fit into 7 bits.
    };

    enum Type : UINT16
    {
        /*
        Field with no data (empty). 0-length payload.
        Can only be used on scalar fields. Illegal for arrays.
        NOTE: Not well-supported by decoders.
        */
        TypeNone = InTypeNull,

        /*
        Encoding assumes 0-terminated CHAR16 string.
        Formatting treats as UTF-16LE string.
        */
        TypeUtf16String = InTypeUnicodeString,

        /*
        Encoding assumes 0-terminated CHAR8 string.
        Formatting treats as MBCS string.
        */
        TypeMbcsString = InTypeAnsiString,

        /*
        Encoding assumes nul-terminated CHAR8 string.
        Formatting treats as UTF-8 string.
        */
        TypeUtf8String = _tld_MAKE_TYPE(InTypeAnsiString, OutTypeUtf8),

        /*
        Encoding assumes 1-byte value.
        Formatting treats as signed integer.
        */
        TypeInt8 = InTypeInt8,

        /*
        Encoding assumes 1-byte value.
        Formatting treats as unsigned integer.
        */
        TypeUInt8 = InTypeUInt8,

        /*
        Encoding assumes 2-byte value.
        Formatting treats as signed integer.
        */
        TypeInt16 = InTypeInt16,

        /*
        Encoding assumes 2-byte value.
        Formatting treats as unsigned integer.
        */
        TypeUInt16 = InTypeUInt16,

        /*
        Encoding assumes 4-byte value.
        Formatting treats as signed integer.
        */
        TypeInt32 = InTypeInt32,

        /*
        Encoding assumes 4-byte value.
        Formatting treats as unsigned integer.
        */
        TypeUInt32 = InTypeUInt32,

        /*
        Encoding assumes 8-byte value.
        Formatting treats as signed integer.
        */
        TypeInt64 = InTypeInt64,

        /*
        Encoding assumes 8-byte value.
        Formatting treats as unsigned integer.
        */
        TypeUInt64 = InTypeUInt64,

        /*
        Encoding assumes 4-byte value.
        Formatting treats as Float32.
        */
        TypeFloat = InTypeFloat,

        /*
        Encoding assumes 8-byte value.
        Formatting treats as Float64.
        */
        TypeDouble = InTypeDouble,

        /*
        Encoding assumes 4-byte value.
        Formatting treats as Boolean.
        */
        TypeBool32 = InTypeBool32,

        /*
        Encoding assumes 2-byte length followed by binary data.
        Formatting treats as binary blob.
        Arrays of TypeBinary are not supported. (No error will be reported,
        but decoding tools will probably not be able to decode them).
        */
        TypeBinary = InTypeBinary,

        /*
        Encoding assumes 16-byte value.
        Formatting treats as GUID.
        */
        TypeGuid = InTypeGuid,

        /*
        Encoding assumes 8-byte value.
        Formatting treats as FILETIME.
        */
        TypeFileTime = InTypeFileTime,

        /*
        Encoding assumes 16-byte value.
        Formatting treats as SYSTEMTIME.
        */
        TypeSystemTime = InTypeSystemTime,

        /*
        Encoding assumes 16-byte value.
        Formatting treats as UTC SYSTEMTIME.
        */
        TypeSystemTimeUtc = _tld_MAKE_TYPE(InTypeSystemTime, OutTypeDateTimeUtc),

        /*
        Encoding assumes SID. Length is determined from number of subauthorities.
        Formatting treats as a SID.
        */
        TypeSid = InTypeSid,

        /*
        Encoding assumes 4-byte value.
        Formatting treats as hexadecimal unsigned integer.
        */
        TypeHexInt32 = InTypeHexInt32,

        /*
        Encoding assumes 8-byte value.
        Formatting treats as hexadecimal unsigned integer.
        */
        TypeHexInt64 = InTypeHexInt64,

        /*
        Encoding assumes 2-byte bytecount followed by CHAR16 data.
        Formatting treats as UTF-16LE string.
        */
        TypeCountedUtf16String = InTypeCountedString,

        /*
        Encoding assumes 2-byte bytecount followed by CHAR8 data.
        Formatting treats as MBCS string.
        */
        TypeCountedMbcsString = InTypeCountedAnsiString,

        /*
        Encoding assumes 2-byte bytecount followed by CHAR8 data.
        Formatting treats as UTF-8 string.
        */
        TypeCountedUtf8String = _tld_MAKE_TYPE(InTypeCountedAnsiString, OutTypeUtf8),

        /*
        Encoding assumes pointer-sized value.
        Formatting treats as signed integer.
        Note: the value of this enum is different on 32-bit and 64-bit systems.
        This is an alias for TypeInt32 when compiled for a 32-bit target.
        This is an alias for TypeInt64 when compiled for a 64-bit target.
        */
        TypeIntPtr = InTypeIntPtr,

        /*
        Encoding assumes pointer-sized value.
        Formatting treats as unsigned integer.
        Note: the value of this enum is different on 32-bit and 64-bit systems.
        This is an alias for TypeUInt32 when compiled for a 32-bit target.
        This is an alias for TypeUInt64 when compiled for a 64-bit target.
        */
        TypeUIntPtr = InTypeUIntPtr,

        /*
        Encoding assumes pointer-sized value.
        Formatting treats as hexadecimal unsigned integer.
        Note: the value of this enum is different on 32-bit and 64-bit systems.
        This is an alias for TypeHexInt32 when compiled for a 32-bit target.
        This is an alias for TypeHexInt64 when compiled for a 64-bit target.
        */
        TypePointer = InTypePointer,

        /*
        Encoding assumes pointer-sized value.
        Formatting treats as code pointer.
        Note: the value of this enum is different on 32-bit and 64-bit systems.
        This is a subtype of TypeHexInt32 when compiled for a 32-bit target.
        This is a subtype of TypeHexInt64 when compiled for a 64-bit target.
        */
        TypeCodePointer = _tld_MAKE_TYPE(InTypePointer, OutTypeCodePointer),

        /*
        Encoding assumes 2-byte value.
        Formatting treats as UTF-16LE character.
        */
        TypeChar16 = _tld_MAKE_TYPE(InTypeUInt16, OutTypeString),

        /*
        Encoding assumes 1-byte value.
        Formatting treats as ANSI character.
        */
        TypeChar8 = _tld_MAKE_TYPE(InTypeUInt8, OutTypeString),

        /*
        Encoding assumes 1-byte value.
        Formatting treats as Boolean.
        */
        TypeBool8 = _tld_MAKE_TYPE(InTypeUInt8, OutTypeBoolean),

        /*
        Encoding assumes 1-byte value.
        Formatting treats as hexadecimal unsigned integer.
        */
        TypeHexInt8 = _tld_MAKE_TYPE(InTypeUInt8, OutTypeHex),

        /*
        Encoding assumes 2-byte value.
        Formatting treats as hexadecimal unsigned integer.
        */
        TypeHexInt16 = _tld_MAKE_TYPE(InTypeUInt16, OutTypeHex),

        /*
        Encoding assumes 4-byte value.
        Formatting treats as process identifier.
        */
        TypePid = _tld_MAKE_TYPE(InTypeUInt32, OutTypePid),

        /*
        Encoding assumes 4-byte value.
        Formatting treats as thread identifier.
        */
        TypeTid = _tld_MAKE_TYPE(InTypeUInt32, OutTypeTid),

        /*
        Encoding assumes 2-byte value.
        Formatting treats as big-endian IP port(s).
        */
        TypePort = _tld_MAKE_TYPE(InTypeUInt16, OutTypePort),

        /*
        Encoding assumes 4-byte value.
        Formatting treats as IPv4 address.
        */
        TypeIPv4 = _tld_MAKE_TYPE(InTypeUInt32, OutTypeIPv4),

        /*
        Encoding assumes 2-byte length followed by binary data.
        Formatting treats as IPv6 address.
        Arrays of TypeIPv6 are not supported. (No error will be reported,
        but decoding tools will probably not be able to decode them).
        */
        TypeIPv6 = _tld_MAKE_TYPE(InTypeBinary, OutTypeIPv6),

        /*
        Encoding assumes 2-byte length followed by binary data.
        Formatting treats as SOCKADDR.
        Arrays of TypeSocketAddress are not supported. (No error will be
        reported, but decoding tools will probably not be able to decode them).
        */
        TypeSocketAddress = _tld_MAKE_TYPE(InTypeBinary, OutTypeSocketAddress),

        /*
        Encoding assumes nul-terminated CHAR16 string.
        Formatting treats as UTF-16LE XML.
        */
        TypeUtf16Xml = _tld_MAKE_TYPE(InTypeUnicodeString, OutTypeXml),

        /*
        Encoding assumes nul-terminated CHAR8 string.
        Formatting treats as UTF-8 XML.
        */
        TypeMbcsXml = _tld_MAKE_TYPE(InTypeAnsiString, OutTypeXml),

        /*
        Encoding assumes nul-terminated CHAR16 string.
        Formatting treats as UTF-16LE JSON.
        */
        TypeUtf16Json = _tld_MAKE_TYPE(InTypeUnicodeString, OutTypeJson),

        /*
        Encoding assumes nul-terminated CHAR8 string.
        Formatting treats as UTF-8 JSON.
        */
        TypeMbcsJson = _tld_MAKE_TYPE(InTypeAnsiString, OutTypeJson),

        /*
        Encoding assumes 2-byte bytecount followed by CHAR16 data.
        Formatting treats as UTF-16LE XML.
        */
        TypeCountedUtf16Xml = _tld_MAKE_TYPE(InTypeCountedString, OutTypeXml),

        /*
        Encoding assumes 2-byte bytecount followed by CHAR8 data.
        Formatting treats as UTF-8 XML.
        */
        TypeCountedMbcsXml = _tld_MAKE_TYPE(InTypeCountedAnsiString, OutTypeXml),

        /*
        Encoding assumes 2-byte bytecount followed by CHAR16 data.
        Formatting treats as UTF-16LE JSON.
        */
        TypeCountedUtf16Json = _tld_MAKE_TYPE(InTypeCountedString, OutTypeJson),

        /*
        Encoding assumes 2-byte bytecount followed by CHAR8 data.
        Formatting treats as UTF-8 JSON.
        */
        TypeCountedMbcsJson = _tld_MAKE_TYPE(InTypeCountedAnsiString, OutTypeJson),

        /*
        Encoding assumes 4-byte value.
        Formatting treats as Win32 error(s).
        */
        TypeWin32Error = _tld_MAKE_TYPE(InTypeUInt32, OutTypeWin32Error),

        /*
        Encoding assumes 4-byte value.
        Formatting treats as NTSTATUS(s).
        */
        TypeNTStatus = _tld_MAKE_TYPE(InTypeUInt32, OutTypeNTStatus),

        /*
        Encoding assumes 4-byte value.
        Formatting treats as HRESULT(s).
        */
        TypeHResult = _tld_MAKE_TYPE(InTypeInt32, OutTypeHResult)
    };

    enum ProviderTraitType : UINT8
    {
        ProviderTraitNone = 0, // Invalid value
        ProviderTraitGroupGuid = 1 // Data is the group GUID.
    };

#pragma endregion

#pragma region EventDescriptor

    /*
    Simple wrapper around the ETW EVENT_DESCRIPTOR structure.
    Provides a constructor to make initialization easier.
    Can be used anywhere an EVENT_DESCRIPTOR is required.

    Notes:
    - Channel defaults to 11 (WINEVENT_CHANNEL_TRACELOGGING). Other channels
      will only work if the provider is running on a TraceLogging-enabled OS
      (Windows 10 or later, or Windows 7 sp1 with updates, or Windows 8.1 with
      updates). If the provider is running on an earlier version of Windows and
      you use a channel other than 11, the events will not decode correctly.
    - If you are not assigning unique Ids for your events, set both Id and
      Version to 0.
    */
    struct EventDescriptor
        : EVENT_DESCRIPTOR
    {
        explicit EventDescriptor(
            UCHAR level = 5, // 5 = WINEVENT_LEVEL_VERBOSE
            ULONGLONG keyword = 0, // 0 = no keywords
            UCHAR opcode = 0, // 0 = WINEVENT_OPCODE_INFO
            USHORT task = 0) // 0 = WINEVENT_TASK_NONE
        {
            Reset(level, keyword, opcode, task);
        }

        void Reset(
            UCHAR level = 5, // 5 = WINEVENT_LEVEL_VERBOSE
            ULONGLONG keyword = 0, // 0 = no keywords
            UCHAR opcode = 0, // 0 = WINEVENT_OPCODE_INFO
            USHORT task = 0) // 0 = WINEVENT_TASK_NONE
        {
            Id = 0;
            Version = 0;
            Channel = 11; // WINEVENT_CHANNEL_TRACELOGGING
            Level = level;
            Opcode = opcode;
            Task = task;
            Keyword = keyword;
        }
    };

#pragma endregion

#pragma region Support functions

#pragma warning(push)
#pragma warning(disable: 26018) // PREfast: Potential read overflow. (Analysis is incorrect.)

    template<class ByteBufferTy>
    void PushBackAsUtf8(
        ByteBufferTy& buffer,
        _In_z_ wchar_t const* szUtf16)
    {
        for (unsigned i = 0;; i++)
        {
            unsigned ch = szUtf16[i];
            if (ch < 0x80)
            {
                buffer.push_back(static_cast<UINT8>(ch));
                if (ch == 0)
                {
                    break;
                }
            }
            else if (ch < 0x800)
            {
                buffer.push_back(static_cast<UINT8>(((ch >> 6)) | 0xc0));
                buffer.push_back(static_cast<UINT8>(((ch)& 0x3f) | 0x80));
            }
            else if (
                0xd800 <= ch && ch < 0xdc00 &&
                0xdc00 <= szUtf16[i + 1] && szUtf16[i + 1] < 0xe000)
            {
                // Decode surrogate pair.
                ++i;
                ch = 0x010000 + (((ch - 0xd800) << 10) | (szUtf16[i] - 0xdc00));
                buffer.push_back(static_cast<UINT8>(((ch >> 18)) | 0xf0));
                buffer.push_back(static_cast<UINT8>(((ch >> 12) & 0x3f) | 0x80));
                buffer.push_back(static_cast<UINT8>(((ch >> 6) & 0x3f) | 0x80));
                buffer.push_back(static_cast<UINT8>(((ch)& 0x3f) | 0x80));
            }
            else
            {
                buffer.push_back(static_cast<UINT8>(((ch >> 12)) | 0xe0));
                buffer.push_back(static_cast<UINT8>(((ch >> 6) & 0x3f) | 0x80));
                buffer.push_back(static_cast<UINT8>(((ch)& 0x3f) | 0x80));
            }
        }
    }

#pragma warning(pop)

    inline ::tld::Type MakeType(
        ::tld::InType inType,
        ::tld::OutType outType)
    {
        return _tld_MAKE_TYPE(inType & InTypeMask, outType & OutTypeMask);
    }

    _Use_decl_annotations_
    inline GUID GetGuidForName(
        wchar_t const* szUtf16Name)
    {
        return ::tld::detail::GetGuidForNameImpl(szUtf16Name);
    }

    _Use_decl_annotations_
    inline GUID GetGuidForName(
        char const* szUtf8Name)
    {
        return ::tld::detail::GetGuidForNameImpl(szUtf8Name);
    }

    inline HRESULT RegisterProvider(
        _Out_ REGHANDLE* phProvider,
        _In_ GUID const* pProviderId,
        _In_count_x_((UINT16*)pProviderMetadata) UINT8 const* pProviderMetadata,
        _In_opt_ PENABLECALLBACK pEnableCallback,
        _In_opt_ void* pEnableCallbackContext)
    {
        HRESULT hr;
        REGHANDLE hProvider = 0;
        ULONG status = ::EventRegister(
            pProviderId,
            pEnableCallback,
            pEnableCallbackContext,
            &hProvider);
        if (status == 0)
        {
            auto cbMetadata = *reinterpret_cast<UINT16 const*>(pProviderMetadata);
            status = tld::SetInformation(
                hProvider,
                static_cast<EVENT_INFO_CLASS>(2), // EventProviderSetTraits
                const_cast<UINT8*>(pProviderMetadata),
                cbMetadata);
            hr = S_OK; // Ignore any failures from SetInformation.
        }
        else
        {
            hr = HRESULT_FROM_WIN32(status);
        }

        *phProvider = hProvider;
        return hr;
    }

    inline HRESULT UnregisterProvider(
        REGHANDLE hProvider)
    {
        ULONG status = ::EventUnregister(hProvider);
        return HRESULT_FROM_WIN32(status);
    }

    _Use_decl_annotations_
    inline HRESULT SetInformation(
        REGHANDLE hProvider,
        EVENT_INFO_CLASS informationClass,
        void* pvInformation,
        ULONG cbInformation)
    {
        ULONG status;

#if TLD_HAVE_EVENT_SET_INFORMATION == 1

#pragma warning(suppress: 6387) // It's ok for pvInformation to be null if cbInformation is 0.
        status = ::EventSetInformation(
            hProvider,
            informationClass,
            pvInformation,
            cbInformation);

#elif TLD_HAVE_EVENT_SET_INFORMATION == 2

        HMODULE hEventing;
        status = ERROR_NOT_SUPPORTED;
        if (GetModuleHandleExW(0, L"api-ms-win-eventing-provider-l1-1-0", &hEventing) ||
            GetModuleHandleExW(0, L"advapi32.dll", &hEventing))
        {
            typedef ULONG(WINAPI* PFEventSetInformation)(
                _In_ REGHANDLE RegHandle,
                _In_ EVENT_INFO_CLASS InformationClass,
                _In_reads_bytes_opt_(InformationLength) PVOID EventInformation,
                _In_ ULONG InformationLength);
            PFEventSetInformation pfEventSetInformation =
                (PFEventSetInformation)GetProcAddress(hEventing, "EventSetInformation");
            if (pfEventSetInformation)
            {
                status = pfEventSetInformation(
                    hProvider,
                    informationClass,
                    pvInformation,
                    cbInformation);
            }

            FreeLibrary(hEventing);
        }

#else // TLD_HAVE_EVENT_SET_INFORMATION == 0

        (void)hProvider;
        (void)informationClass;
        (void)pvInformation;
        (void)cbInformation;

        status = ERROR_NOT_SUPPORTED;

#endif // TLD_HAVE_EVENT_SET_INFORMATION

        return HRESULT_FROM_WIN32(status);
    }

    _Use_decl_annotations_
    inline HRESULT WriteEvent(
        REGHANDLE hProvider,
        EVENT_DESCRIPTOR const& eventDescriptor,
        UINT8 const* pProviderMetadata,
        UINT8 const* pEventMetadata,
        ULONG cDataDescriptors,
        EVENT_DATA_DESCRIPTOR* pDataDescriptors,
        LPCGUID pActivityId,
        LPCGUID pRelatedActivityId)
    {
        _tld_ASSERT(3 <= *(UINT16*)pProviderMetadata, "Invalid provider metadata.");
        _tld_ASSERT(4 <= *(UINT16*)pEventMetadata, "Invalid event metadata.");
        pDataDescriptors[0].Ptr = (ULONG_PTR)pProviderMetadata;
        pDataDescriptors[0].Size = *(UINT16*)pProviderMetadata;
        pDataDescriptors[0].Reserved = 2; // Descriptor contains provider metadata.
        pDataDescriptors[1].Ptr = (ULONG_PTR)pEventMetadata;
        pDataDescriptors[1].Size = *(UINT16*)pEventMetadata;
        pDataDescriptors[1].Reserved = 1; // Descriptor contains event metadata.
        ULONG status = ::EventWriteTransfer(
            hProvider,
            &eventDescriptor,
            pActivityId,
            pRelatedActivityId,
            cDataDescriptors,
            pDataDescriptors);
        return HRESULT_FROM_WIN32(status);
    }

#if TLD_HAVE_EVENT_WRITE_EX == 1

    _Use_decl_annotations_
    inline HRESULT WriteEventEx(
        REGHANDLE hProvider,
        EVENT_DESCRIPTOR const& eventDescriptor,
        UINT8 const* pProviderMetadata,
        UINT8 const* pEventMetadata,
        ULONG cDataDescriptors,
        EVENT_DATA_DESCRIPTOR* pDataDescriptors,
        ULONG64 filter,
        ULONG flags,
        GUID const* pActivityId,
        GUID const* pRelatedActivityId)
    {
        _tld_ASSERT(3 <= *(UINT16*)pProviderMetadata, "Invalid provider metadata.");
        _tld_ASSERT(4 <= *(UINT16*)pEventMetadata, "Invalid event metadata.");
        pDataDescriptors[0].Ptr = (ULONG_PTR)pProviderMetadata;
        pDataDescriptors[0].Size = *(UINT16*)pProviderMetadata;
        pDataDescriptors[0].Reserved = 2; // Descriptor contains provider metadata.
        pDataDescriptors[1].Ptr = (ULONG_PTR)pEventMetadata;
        pDataDescriptors[1].Size = *(UINT16*)pEventMetadata;
        pDataDescriptors[1].Reserved = 1; // Descriptor contains event metadata.
        ULONG status = ::EventWriteEx(
            hProvider,
            &eventDescriptor,
            filter,
            flags,
            pActivityId,
            pRelatedActivityId,
            cDataDescriptors,
            pDataDescriptors);
        return HRESULT_FROM_WIN32(status);
    }

#endif // TLD_HAVE_EVENT_WRITE_EX

#pragma endregion

#pragma region ByteArrayWrapper

    class ByteArrayWrapper
    {
        UINT8* m_pb;
        UINT32 m_cbMax;
        UINT32 m_cbCur;

    public:

        ByteArrayWrapper()
        {
            reset();
        }

        ByteArrayWrapper(
            _Out_writes_(cbMax) UINT8* pb,
            UINT32 cbMax)
        {
            reset(pb, cbMax);
        }

        void reset()
        {
            m_pb = 0;
            m_cbMax = 0;
            m_cbCur = 0;
        }

        void reset(
            _Inout_updates_bytes_(cbMax) UINT8* pb,
            UINT32 cbMax)
        {
            m_pb = pb;
            m_cbMax = cbMax;
            m_cbCur = 0;
        }

        void clear()
        {
            m_cbCur = 0;
        }

        UINT32 size() const
        {
            return m_cbCur;
        }

        BYTE const& front() const
        {
            return *m_pb;
        }

        BYTE const* data() const
        {
            return m_pb;
        }

        UINT32 capacity() const
        {
            return m_cbMax;
        }

        void push_back(UINT8 val)
        {
            if (m_cbCur < m_cbMax)
            {
                m_pb[m_cbCur] = val;
            }

            m_cbCur += 1;
        }

        UINT8& operator[](UINT32 index)
        {
            static UINT8 dummy;
            return index < m_cbMax
                ? m_pb[index]
                : dummy;
        }
    };

#pragma endregion

#pragma region ProviderMetadataBuilder

    template<class ByteBufferTy>
    class ProviderMetadataBuilder
        : public detail::MetadataBuilderBase<ByteBufferTy>
    {
        /*
        ProviderMetadata =
        Size    : UINT16; // Size counts the whole data structure, including the Size field.
        Name    : Nul-terminated UTF-8.
        Traits[]: ProviderTrait (parse until you have consumed Size bytes)

        ProviderTrait =
        TraitSize: UINT16;
        TraitType: UINT8;
        TraitData: UINT8[TraitSize - 3];
        */

    public:

        explicit ProviderMetadataBuilder(ByteBufferTy& buffer)
            : detail::MetadataBuilderBase<ByteBufferTy>(buffer)
        {
            return;
        }

        template<class CharTy>
        void Begin(
            _In_z_ CharTy const* szUtfProviderName)
        {
            this->BaseBegin();
            this->AddString(szUtfProviderName);
        }

        void AddTrait(
            ProviderTraitType type,
            _In_reads_bytes_(cbData) void const* pData,
            unsigned cbData)
        {
            this->AddU16(static_cast<UINT16>(cbData + 3));
            this->AddU8(static_cast<UINT8>(type));
            this->AddBytes(pData, cbData);
        }

        bool End()
        {
            return this->BaseEnd();
        }
    };

#pragma endregion

#pragma region EventMetadataBuilder

    template<class ByteBufferTy>
    class EventMetadataBuilder
        : public detail::MetadataBuilderBase<ByteBufferTy>
    {
        /*
        EventMetadata =
        Size    : UINT16; // Size counts the whole data structure, including the Size field.
        Tags[]  : UINT8 (read bytes until you encounter a byte with high bit unset);
        Name    : Nul-terminated UTF-8;
        Fields[]: FieldMetadata (parse until you get to end of EventMetadata);

        FieldMetadata =
        Name    : Nul-terminated UTF-8.
        InMeta  : UINT8.
        OutMeta : UINT8 (only present if InMeta & InMetaChain);
        Tags[]  : UINT8 (only present if OutMeta & OutMetaChain, read bytes until you encounter a byte with high bit unset);
        Ccount  : UINT16 (only present if InMeta & InMetaCountMask == InMetaCcount);
        CbSchema: UINT16 (only present if InMeta & InMetaCountMask == InMetaCustom);
        Schema  : UINT8[CbSchema]
        */

        static const UINT8 InMetaScalar = 0;
        static const UINT8 InMetaCcount = 32;
        static const UINT8 InMetaVcount = 64;
        static const UINT8 InMetaCustom = 96;
        static const UINT8 InMetaCountMask = 96;
        static const UINT8 InMetaChain = 128;
        static const UINT8 OutMetaChain = 128;

        UINT32 const m_bookmark;

        EventMetadataBuilder(
            ByteBufferTy& buffer,
            UINT32 bookmark)
            : detail::MetadataBuilderBase<ByteBufferTy>(buffer)
            , m_bookmark(bookmark)
        {
            return;
        }

        void AddTags(UINT32 tags)
        {
            _tld_ASSERT(0 == (tags & 0xf0000000), "Tags only supports 28-bit values.");
            for (;;)
            {
                auto val = static_cast<UINT8>(tags >> 21);
                if ((tags & 0x1fffff) == 0)
                {
                    this->AddU8(val & 0x7f);
                    break;
                }

                this->AddU8(val | 0x80);
                tags = tags << 7;
            }
        }

        void AddFieldInfo(UINT8 arity, Type type, UINT32 tags)
        {
            _tld_ASSERT((type & (Type)InTypeMask) == (type & 0xff), "InType out of range");
            _tld_ASSERT((type & _tld_MAKE_TYPE(0, OutTypeMask)) == (Type)(type & 0xffffff00), "OutType out of range");

            UINT8 inMeta = arity | static_cast<UINT8>(type);
            UINT8 outMeta = static_cast<UINT8>(type >> 8);
            if (tags != 0)
            {
                this->AddU8(static_cast<UINT8>(inMeta | InMetaChain));
                this->AddU8(static_cast<UINT8>(outMeta | OutMetaChain));
                AddTags(tags);
            }
            else if (outMeta != 0)
            {
                this->AddU8(static_cast<UINT8>(inMeta | InMetaChain));
                this->AddU8(static_cast<UINT8>(outMeta));
            }
            else
            {
                this->AddU8(inMeta);
            }

            if (m_bookmark != 0)
            {
                this->IncrementU7(m_bookmark);
            }
        }

        UINT32 AddStructInfo(UINT8 arity, UINT32 tags)
        {
            UINT8 inMeta = arity | InTypeStruct;
            this->AddU8(static_cast<UINT8>(inMeta | InMetaChain));
            UINT32 bookmark = this->GetBookmark();
            if (tags == 0)
            {
                this->AddU8(0);
            }
            else
            {
                this->AddU8(OutMetaChain);
                AddTags(tags);
            }

            if (m_bookmark != 0)
            {
                this->IncrementU7(m_bookmark);
            }

            return bookmark;
        }

    public:

        explicit EventMetadataBuilder(
            ByteBufferTy& buffer)
            : detail::MetadataBuilderBase<ByteBufferTy>(buffer)
            , m_bookmark(0)
        {
            return;
        }

        EventMetadataBuilder(EventMetadataBuilder& other)
            : detail::MetadataBuilderBase<ByteBufferTy>(other.GetBuffer())
            , m_bookmark(other.m_bookmark)
        {
            return;
        }

        EventMetadataBuilder(EventMetadataBuilder&& other)
            : detail::MetadataBuilderBase<ByteBufferTy>(other.GetBuffer())
            , m_bookmark(other.m_bookmark)
        {
            return;
        }

        template<class CharTy>
        void Begin(
            _In_z_ CharTy const* szUtfEventName,
            UINT32 eventTags = 0)
        {
            _tld_ASSERT(m_bookmark == 0, "Must not call Begin on a struct builder");
            this->BaseBegin();
            AddTags(eventTags);
            this->AddString(szUtfEventName);
        }

        bool End()
        {
            _tld_ASSERT(m_bookmark == 0, "Must not call End on a struct builder");
            return this->BaseEnd();
        }

        // Note: Do not create structs with 0 fields.
        template<class CharTy>
        EventMetadataBuilder AddStruct(
            _In_z_ CharTy const* szUtfStructName,
            UINT32 fieldTags = 0)
        {
            this->AddString(szUtfStructName);
            auto bookmark = AddStructInfo(InMetaScalar, fieldTags);
            return EventMetadataBuilder(this->GetBuffer(), bookmark);
        }

        // Note: Do not create structs with 0 fields.
        template<class CharTy>
        UINT32 AddStructRaw(
            _In_z_ CharTy const* szUtfStructName,
            UINT32 fieldTags = 0)
        {
            this->AddString(szUtfStructName);
            auto bookmark = AddStructInfo(InMetaScalar, fieldTags);
            return bookmark;
        }

        // Note: Do not create structs with 0 fields.
        template<class CharTy>
        EventMetadataBuilder AddStructArray(
            _In_z_ CharTy const* szUtfStructName,
            UINT32 fieldTags = 0)
        {
            this->AddString(szUtfStructName);
            auto bookmark = AddStructInfo(InMetaVcount, fieldTags);
            return EventMetadataBuilder(this->GetBuffer(), bookmark);
        }

        // Note: Do not use 0 for itemCount.
        // Note: Do not create structs with 0 fields.
        template<class CharTy>
        EventMetadataBuilder AddStructFixedArray(
            _In_z_ CharTy const* szUtfStructName,
            UINT16 itemCount,
            UINT32 fieldTags = 0)
        {
            this->AddString(szUtfStructName);
            auto bookmark = AddStructInfo(InMetaCcount, fieldTags);
            this->AddU16(itemCount);
            return EventMetadataBuilder(this->GetBuffer(), bookmark);
        }

        template<class CharTy>
        void AddField(
            _In_z_ CharTy const* szUtfFieldName,
            Type type,
            UINT32 fieldTags = 0)
        {
            this->AddString(szUtfFieldName);
            AddFieldInfo(InMetaScalar, type, fieldTags);
        }

        template<class CharTy>
        void AddFieldArray(
            _In_z_ CharTy const* szUtfFieldName,
            Type type,
            UINT32 fieldTags = 0)
        {
            this->AddString(szUtfFieldName);
            AddFieldInfo(InMetaVcount, type, fieldTags);
        }

        // Note: Do not use 0 for itemCount.
        template<class CharTy>
        void AddFieldFixedArray(
            _In_z_ CharTy const* szUtfFieldName,
            UINT16 itemCount,
            Type type,
            UINT32 fieldTags = 0)
        {
            this->AddString(szUtfFieldName);
            AddFieldInfo(InMetaCcount, type, fieldTags);
            this->AddU16(itemCount);
        }

        template<class CharTy>
        void AddFieldCustom(
            _In_z_ CharTy const* szUtfFieldName,
            _In_reads_bytes_(cbSchema) void const* pSchema,
            UINT16 cbSchema,
            Type type,
            UINT32 fieldTags = 0)
        {
            this->AddString(szUtfFieldName);
            AddFieldInfo(InMetaCustom, type, fieldTags);
            this->AddU16(cbSchema);
            auto pbSchema = static_cast<UINT8 const*>(pSchema);
            for (UINT32 i = 0; i != cbSchema; i++)
            {
                this->AddU8(pbSchema[i]);
            }
        }
    };

#pragma endregion

#pragma region EventDataBuilder

    template<class ByteBufferTy>
    class EventDataBuilder
    {
        void operator=(EventDataBuilder const&); // = delete
        ByteBufferTy& m_buffer;

    public:

        explicit EventDataBuilder(ByteBufferTy& buffer)
            : m_buffer(buffer)
        {
            return;
        }

        EventDataBuilder(EventDataBuilder& other)
            : m_buffer(other.m_buffer)
        {
            return;
        }

        EventDataBuilder(EventDataBuilder&& other)
            : m_buffer(other.m_buffer)
        {
            return;
        }

        ByteBufferTy& Buffer()
        {
            return m_buffer;
        }

        ByteBufferTy const& Buffer() const
        {
            return m_buffer;
        }

        void Clear()
        {
            m_buffer.clear();
        }

        UINT32 Size() const
        {
            return static_cast<UINT32>(m_buffer.size());
        }

        UINT8& operator[](UINT32 index)
        {
            return m_buffer[index];
        }

        void AddArrayCount(UINT16 cItems)
        {
            AddBytes(&cItems, sizeof(cItems));
        }

        /*
        Adds a value to the payload.
        Note: should only be used for blittable POD types with no padding.
        */
        template<class T>
        void AddValue(T const& value)
        {
            AddBytes(&reinterpret_cast<const char&>(value), sizeof(T));
        }

        /*
        Adds an array of values to the payload.
        Note: should only be used for blittable POD types with no padding.
        */
        template<class T>
        void AddValues(_In_reads_(cValues) T const* pValues, unsigned cValues)
        {
            AddBytes(pValues, sizeof(T) * cValues);
        }

        /*
        Appends an InTypeBinary field to the payload.
        Compatible with: TypeBinary, TypeIPv6, TypeSocketAddress.
        */
        void AddBinary(_In_reads_bytes_(cbData) void const* pbData, UINT16 cbData)
        {
            AddBytes(&cbData, sizeof(cbData));
            AddBytes(pbData, cbData);
        }

        /*
        Appends an InTypeCountedAnsiString field to the payload.
        Compatible with: TypeCountedMbcsString, TypeCountedMbcsXml, TypeCountedMbcsJson.
        */
        void AddCountedString(_In_reads_(cch) char const* pch, UINT16 cch)
        {
            AddBinary(pch, cch * sizeof(*pch));
        }

        /*
        Appends an InTypeCountedString field to the payload.
        Compatible with: TypeCountedUtf16String, TypeCountedUtf16Xml, TypeCountedUtf16Json.
        */
        void AddCountedString(_In_reads_(cch) wchar_t const* pch, UINT16 cch)
        {
            AddBinary(pch, cch * sizeof(*pch));
        }

        /*
        Appends an InTypeAnsiString field to the payload.
        Compatible with: TypeMbcsString, TypeMbcsXml, TypeMbcsJson.
        */
        void AddString(_In_z_ char const* psz)
        {
            AddBytes(psz, static_cast<unsigned>(strlen(psz) + 1));
        }

        /*
        Appends a InTypeUnicodeString field to the payload.
        Compatible with: TypeUtf16String, TypeUtf16Xml, TypeUtf16Json
        */
        void AddString(_In_z_ wchar_t const* psz)
        {
            AddBytes(psz, static_cast<unsigned>(2 * (wcslen(psz) + 1)));
        }

        /*
        Appends a raw byte to the payload. (Calls buffer.push_back)
        */
        void AddByte(UINT8 val)
        {
            m_buffer.push_back(val);
        }

        /*
        Appends raw bytes to the payload. (Calls buffer.insert)
        */
        void AddBytes(_In_reads_bytes_(cbData) void const* pbData, unsigned cbData)
        {
            auto pb = static_cast<UINT8 const*>(pbData);
            m_buffer.insert(m_buffer.end(), pb + 0, pb + cbData);
        }
    };

#pragma endregion

#pragma region Provider

    class Provider
    {
        Provider(Provider const&); // = delete
        void operator=(Provider const&); // = delete

        UINT32 m_levelPlus1;
        HRESULT m_hrInitialization;
        ULONGLONG m_keywordsAny;
        ULONGLONG m_keywordsAll;
        REGHANDLE m_hProvider;
        UINT8 const* m_pbMetadata;
        PENABLECALLBACK m_pfCallback;
        LPVOID m_pCallbackContext;
        GUID m_id;

        /*
        Shortest possible valid provider metadata blob:
        Size = 0x0003, Name = "", no options.
        */
        static _Ret_bytecount_(3) UINT8 const* NullMetadata()
        {
            return reinterpret_cast<UINT8 const*>("\03\0");
        }

        static VOID NTAPI EnableCallback(
            _In_ LPCGUID pSourceId,
            _In_ ULONG callbackType,
            _In_ UCHAR level,
            _In_ ULONGLONG keywordsAny,
            _In_ ULONGLONG keywordsAll,
            _In_opt_ PEVENT_FILTER_DESCRIPTOR pFilterData,
            _Inout_opt_ PVOID pCallbackContext)
        {
            if (pCallbackContext)
            {
                Provider* pProvider = static_cast<Provider*>(pCallbackContext);
                switch (callbackType)
                {
                case 0: // EVENT_CONTROL_CODE_DISABLE_PROVIDER
                    pProvider->m_levelPlus1 = 0;
                    break;
                case 1: // EVENT_CONTROL_CODE_ENABLE_PROVIDER
                    pProvider->m_levelPlus1 = level != 0 ? (UINT32)level + 1u : 256u;
                    pProvider->m_keywordsAny = keywordsAny;
                    pProvider->m_keywordsAll = keywordsAll;
                    break;
                }

                if (pProvider->m_pfCallback)
                {
                    pProvider->m_pfCallback(
                        pSourceId,
                        callbackType,
                        level,
                        keywordsAny,
                        keywordsAll,
                        pFilterData,
                        pProvider->m_pCallbackContext);
                }
            }
        }

        _Ret_opt_bytecount_(cbMetadata)
        static UINT8* MetadataAlloc(
            UINT32 cbMetadata)
        {
            return static_cast<UINT8*>(HeapAlloc(GetProcessHeap(), 0, cbMetadata));
        }

        static void MetadataFree(
            _Post_invalid_ UINT8* pbMetadata)
        {
            if (pbMetadata)
            {
                HeapFree(GetProcessHeap(), 0, pbMetadata);
            }
        }

        void InitFail(
            HRESULT hr)
        {
            _tld_ASSERT(m_pbMetadata != NullMetadata(), "InitFail called with m_pbMetadata == NullMetadata()");
            MetadataFree(const_cast<UINT8*>(m_pbMetadata));
            m_hrInitialization = hr;
            m_pbMetadata = NullMetadata();
        }

        template<class CharTy>
        void Init(
            _In_z_ CharTy const* szName,
            _In_opt_ GUID const* pGroupId)
        {
            // Already initialized: m_pfCallback, m_pCallbackContext, m_id.
            m_levelPlus1 = 0;
            m_hrInitialization = 0;
            m_keywordsAny = 0;
            m_keywordsAll = 0;
            m_hProvider = 0;
            m_pbMetadata = 0;

            ByteArrayWrapper wrapper(0, 0);
            ProviderMetadataBuilder<ByteArrayWrapper> builder(wrapper);
            builder.Begin(szName);
            if (pGroupId)
            {
                builder.AddTrait(ProviderTraitGroupGuid, pGroupId, sizeof(GUID));
            }
            if (!builder.End())
            {
                InitFail(E_INVALIDARG); // Metadata > 64KB.
                goto Done;
            }

            {
                UINT32 const cbMetadata = wrapper.size();
                UINT8* pbMetadata = MetadataAlloc(cbMetadata);
                if (!pbMetadata)
                {
                    InitFail(E_OUTOFMEMORY);
                    goto Done;
                }

                m_pbMetadata = pbMetadata;
                wrapper.reset(pbMetadata, cbMetadata);
                builder.Begin(szName);
                if (pGroupId)
                {
                    builder.AddTrait(ProviderTraitGroupGuid, pGroupId, sizeof(GUID));
                }
                if (!builder.End() || cbMetadata < wrapper.size())
                {
                    InitFail(0x8000000CL); // E_CHANGED_STATE
                    goto Done;
                }

                {
                    HRESULT hr = ::tld::RegisterProvider(&m_hProvider, &m_id, m_pbMetadata, &EnableCallback, this);
                    if (FAILED(hr))
                    {
                        InitFail(hr);
                    }
                }
            }

        Done:

            return;
        }

        bool IsEnabledForKeywords(ULONGLONG keywords) const
        {
            return keywords == 0 || (
                (keywords & m_keywordsAny) != 0 &&
                (keywords & m_keywordsAll) == m_keywordsAll);
        }

    public:

        ~Provider()
        {
            EventUnregister(m_hProvider);
            if (m_pbMetadata != NullMetadata())
            {
                MetadataFree(const_cast<UINT8*>(m_pbMetadata));
            }
        }

        Provider(
            _In_z_ wchar_t const* szUtf16Name,
            GUID const& providerId,
            _In_opt_ GUID const* pGroupId = 0,
            PENABLECALLBACK pfCallback = 0,
            LPVOID pCallbackContext = 0)
            : m_pfCallback(pfCallback)
            , m_pCallbackContext(pCallbackContext)
            , m_id(providerId)
        {
            Init(szUtf16Name, pGroupId);
        }

        Provider(
            _In_z_ char const* szUtf8Name,
            GUID const& providerId,
            _In_opt_ GUID const* pGroupId = 0,
            PENABLECALLBACK pfCallback = 0,
            LPVOID pCallbackContext = 0)
            : m_pfCallback(pfCallback)
            , m_pCallbackContext(pCallbackContext)
            , m_id(providerId)
        {
            Init(szUtf8Name, pGroupId);
        }

        explicit Provider(
            _In_z_ wchar_t const* szUtf16Name,
            _In_opt_ GUID const* pGroupId = 0,
            PENABLECALLBACK pfCallback = 0,
            LPVOID pCallbackContext = 0)
            : m_pfCallback(pfCallback)
            , m_pCallbackContext(pCallbackContext)
            , m_id(GetGuidForName(szUtf16Name))
        {
            Init(szUtf16Name, pGroupId);
        }

        explicit Provider(
            _In_z_ char const* szUtf8Name,
            _In_opt_ GUID const* pGroupId = 0,
            PENABLECALLBACK pfCallback = 0,
            LPVOID pCallbackContext = 0)
            : m_pfCallback(pfCallback)
            , m_pCallbackContext(pCallbackContext)
            , m_id(GetGuidForName(szUtf8Name))
        {
            Init(szUtf8Name, pGroupId);
        }

        HRESULT InitializationResult() const
        {
            return m_hrInitialization;
        }

        _Ret_z_ char const* Name() const
        {
            return reinterpret_cast<char const*>(m_pbMetadata + 2);
        }

        GUID const& Id() const
        {
            return m_id;
        }

        _Ret_notnull_ UINT8 const* GetMetadata() const
        {
            return m_pbMetadata;
        }

        UINT16 GetMetadataSize() const
        {
            return *reinterpret_cast<UINT16 const*>(m_pbMetadata);
        }

        bool IsEnabled() const
        {
            return m_levelPlus1 != 0;
        }

        bool IsEnabled(UCHAR level) const
        {
            return level < m_levelPlus1;
        }

        bool IsEnabled(UCHAR level, ULONGLONG keywords) const
        {
            return level < m_levelPlus1 && IsEnabledForKeywords(keywords);
        }

        HRESULT SetInformation(
            EVENT_INFO_CLASS informationClass,
            _In_reads_bytes_opt_(cbInformation) void* pbInformation,
            ULONG cbInformation
            ) const
        {
            return ::tld::SetInformation(m_hProvider, informationClass, pbInformation, cbInformation);
        }

        HRESULT WriteGather(
            EVENT_DESCRIPTOR const& eventDescriptor,
            _In_count_x_((UINT16*)pEventMetadata) UINT8 const* pEventMetadata,
            _In_range_(2, 128) ULONG cDataDescriptors,
            _Inout_count_(cDataDescriptors) EVENT_DATA_DESCRIPTOR* pDataDescriptors,
            _In_opt_ LPCGUID pActivityId = 0,
            _In_opt_ LPCGUID pRelatedActivityId = 0
            ) const
        {
            return ::tld::WriteEvent(
                m_hProvider,
                eventDescriptor,
                m_pbMetadata,
                pEventMetadata,
                cDataDescriptors,
                pDataDescriptors,
                pActivityId,
                pRelatedActivityId);
        }

        HRESULT Write(
            EVENT_DESCRIPTOR const& eventDescriptor,
            _In_count_x_((UINT16*)pEventMetadata) UINT8 const* pEventMetadata,
            _In_reads_bytes_(cbData) LPCVOID pbData,
            UINT32 cbData,
            _In_opt_ LPCGUID pActivityId = 0,
            _In_opt_ LPCGUID pRelatedActivityId = 0
            ) const
        {
            EVENT_DATA_DESCRIPTOR dataDesc[3];
            EventDataDescCreate(dataDesc + 2, pbData, cbData);
            return ::tld::WriteEvent(
                m_hProvider,
                eventDescriptor,
                m_pbMetadata,
                pEventMetadata,
                cbData ? 3 : 2,
                dataDesc,
                pActivityId,
                pRelatedActivityId);
        }

#if TLD_HAVE_EVENT_WRITE_EX == 1

        HRESULT WriteGatherEx(
            EVENT_DESCRIPTOR const& eventDescriptor,
            _In_count_x_((UINT16*)pEventMetadata) UINT8 const* pEventMetadata,
            _In_range_(2, 128) ULONG cDataDescriptors,
            _Inout_count_(cDataDescriptors) EVENT_DATA_DESCRIPTOR* pDataDescriptors,
            ULONG64 filter,
            ULONG flags,
            _In_opt_ LPCGUID pActivityId = 0,
            _In_opt_ LPCGUID pRelatedActivityId = 0
            ) const
        {
            return ::tld::WriteEventEx(
                m_hProvider,
                eventDescriptor,
                m_pbMetadata,
                pEventMetadata,
                cDataDescriptors,
                pDataDescriptors,
                filter,
                flags,
                pActivityId,
                pRelatedActivityId);
        }

        HRESULT WriteEx(
            EVENT_DESCRIPTOR const& eventDescriptor,
            _In_count_x_((UINT16*)pEventMetadata) UINT8 const* pEventMetadata,
            _In_reads_bytes_(cbData) LPCVOID pbData,
            UINT32 cbData,
            ULONG64 filter,
            ULONG flags,
            _In_opt_ LPCGUID pActivityId = 0,
            _In_opt_ LPCGUID pRelatedActivityId = 0
            ) const
        {
            EVENT_DATA_DESCRIPTOR dataDesc[3];
            EventDataDescCreate(dataDesc + 2, pbData, cbData);
            return ::tld::WriteEventEx(
                m_hProvider,
                eventDescriptor,
                m_pbMetadata,
                pEventMetadata,
                cbData ? 3 : 2,
                dataDesc,
                filter,
                flags,
                pActivityId,
                pRelatedActivityId);
        }

#endif // TLD_HAVE_EVENT_WRITE_EX
    };

#pragma endregion

#pragma region EventBuilder

    enum EventState
    {
        EventStateOpen,
        EventStateClosed,
        EventStateError
    };

    template<class ByteBufferTy>
    class EventBuilder
    {
        void operator=(EventBuilder const&); // = delete

        EventMetadataBuilder<ByteBufferTy> m_metaBuilder;
        EventDataBuilder<ByteBufferTy> m_dataBuilder;
        EventState& m_eventState;

        EventBuilder(EventBuilder& other, EventMetadataBuilder<ByteBufferTy>&& metaBuilder)
            : m_metaBuilder(metaBuilder)
            , m_dataBuilder(other.m_dataBuilder)
            , m_eventState(other.m_eventState)
        {
            return;
        }

    protected:

        EventBuilder(
            ByteBufferTy& metaBuffer,
            ByteBufferTy& dataBuffer,
            EventState& eventState)
            : m_metaBuilder(metaBuffer)
            , m_dataBuilder(dataBuffer)
            , m_eventState(eventState)
        {
            return;
        }

    public:

        EventBuilder(EventBuilder& other)
            : m_metaBuilder(other.m_metaBuilder)
            , m_dataBuilder(other.m_dataBuilder)
            , m_eventState(other.m_eventState)
        {
            return;
        }

        EventBuilder(EventBuilder&& other)
            : m_metaBuilder(other.m_metaBuilder)
            , m_dataBuilder(other.m_dataBuilder)
            , m_eventState(other.m_eventState)
        {
            return;
        }

        /*
        Returns a const reference to the event metadata's underlying buffer.
        */
        ByteBufferTy const& MetaBuffer() const
        {
            return m_metaBuilder.GetBuffer();
        }

        /*
        Returns a const reference to the event payload data's underlying buffer.
        */
        ByteBufferTy const& DataBuffer() const
        {
            return m_dataBuilder.Buffer();
        }

        /*
        Adds a nested struct to the event's metadata.
        Use the returned EventBuilder to add data and metadata for the fields
        of the nested struct.
        Note: do not call any Add methods on this builder object until you are
        done calling Add methods on the nested builder object.
        Note: Do not create structs with 0 fields.
        */
        template<class CharTy>
        EventBuilder AddStruct(
            _In_z_ CharTy const* szUtfStructName,
            UINT32 fieldTags = 0)
        {
            _tld_ASSERT(m_eventState == EventStateOpen, "Metadata already prepared.");
            return EventBuilder(*this, m_metaBuilder.AddStruct(szUtfStructName, fieldTags));
        }

        /*
        Adds a nested array of struct to the event's metadata.
        Use the returned EventBuilder to add data and metadata for the fields
        of the nested struct.
        Note: do not call any Add methods on this builder object until you are
        done calling Add methods on the nested builder object.
        Note: Do not create structs with 0 fields.
        */
        template<class CharTy>
        EventBuilder AddStructArray(
            _In_z_ CharTy const* szUtfStructName,
            UINT32 fieldTags = 0)
        {
            _tld_ASSERT(m_eventState == EventStateOpen, "Metadata already prepared.");
            return EventBuilder(*this, m_metaBuilder.AddStructArray(szUtfStructName, fieldTags));
        }

        /*
        Adds a nested array of struct to the event's metadata.
        Use the returned EventBuilder to add data and metadata for the fields
        of the nested struct.
        Note: do not call any Add methods on this builder object until you are
        done calling Add methods on the nested builder object.
        Note: Do not use 0 for itemCount.
        Note: Do not create structs with 0 fields.
        */
        template<class CharTy>
        EventBuilder AddStructFixedArray(
            _In_z_ CharTy const* szUtfStructName,
            UINT16 itemCount,
            UINT32 fieldTags = 0)
        {
            _tld_ASSERT(m_eventState == EventStateOpen, "Metadata already prepared.");
            return EventBuilder(*this, m_metaBuilder.AddStructFixedArray(szUtfStructName, itemCount, fieldTags));
        }

        /*
        Adds a scalar field to the event's metadata.
        */
        template<class CharTy>
        void AddField(
            _In_z_ CharTy const* szUtfFieldName,
            Type type,
            UINT32 fieldTags = 0)
        {
            _tld_ASSERT(m_eventState == EventStateOpen, "Metadata already prepared.");
            m_metaBuilder.AddField(szUtfFieldName, type, fieldTags);
        }

        /*
        Adds a variable-length array field to the event's metadata.
        The length (item count) must be added to the event's payload
        immediately before the array item values.
        */
        template<class CharTy>
        void AddFieldArray(
            _In_z_ CharTy const* szUtfFieldName,
            Type type,
            UINT32 fieldTags = 0)
        {
            _tld_ASSERT(m_eventState == EventStateOpen, "Metadata already prepared.");
            m_metaBuilder.AddFieldArray(szUtfFieldName, type, fieldTags);
        }

        /*
        Adds a fixed-length array field to the event's metadata.
        The length (item count) is encoded in the metadata, so it does not
        need to be included in the event's payload.
        Note: Do not use 0 for itemCount.
        */
        template<class CharTy>
        void AddFieldFixedArray(
            _In_z_ CharTy const* szUtfFieldName,
            UINT16 itemCount,
            Type type,
            UINT32 fieldTags = 0)
        {
            _tld_ASSERT(m_eventState == EventStateOpen, "Metadata already prepared.");
            m_metaBuilder.AddFieldFixedArray(szUtfFieldName, itemCount, type, fieldTags);
        }

        /*
        Adds a custom-encoded field to the event's metadata.
        Custom-encoded means that you are using some other serialization
        system (e.g. Bond, Protocol Buffers) to produce the schema and data.
        The payload is encoded as a UINT16 byte count followed by the data.
        */
        template<class CharTy>
        void AddFieldCustom(
            _In_z_ CharTy const* szUtfFieldName,
            _In_reads_bytes_(cbSchema) void const* pSchema,
            UINT16 cbSchema,
            Type type,
            UINT32 fieldTags = 0)
        {
            _tld_ASSERT(m_eventState == EventStateOpen, "Metadata already prepared.");
            m_metaBuilder.AddFieldCustom(szUtfFieldName, pSchema, cbSchema, type, fieldTags);
        }

        /*
        Advanced use: adds raw metadata bytes.
        */
        void AddRawMetadata(
            _In_reads_bytes_(cbMetadata) void const* pMetadata,
            UINT32 cbMetadata)
        {
            auto& buf = m_metaBuilder.GetBuffer();
            auto pb = static_cast<BYTE const*>(pMetadata);
            buf.insert(buf.end(), pb + 0u, pb + cbMetadata);
        }

        /*
        Adds an array length (item count) to the payload. This is for
        variable-length arrays only (not fixed-length), and must occur
        immediately before the array item values are written to the payload.
        */
        void AddArrayCount(UINT16 cItems)
        {
            m_dataBuilder.AddArrayCount(cItems);
        }

        /*
        Adds a value to the payload.
        Note: should only be used for blittable POD types with no padding,
        e.g. INT32, FILETIME, GUID, not for strings or structs.
        */
        template<class T>
        void AddValue(T const& value)
        {
            m_dataBuilder.AddValue(value);
        }

        /*
        Adds an array of values to the payload.
        Note: should only be used for blittable POD types with no padding,
        e.g. INT32, FILETIME, GUID, not for strings or structs.
        */
        template<class T>
        void AddValues(_In_reads_(cValues) T const* pValues, unsigned cValues)
        {
            m_dataBuilder.AddValues(pValues, cValues);
        }

        /*
        Appends an InTypeBinary field to the payload.
        Compatible with: TypeBinary, TypeIPv6, TypeSocketAddress.
        */
        void AddBinary(_In_reads_bytes_(cbData) void const* pbData, UINT16 cbData)
        {
            m_dataBuilder.AddBinary(pbData, cbData);
        }

        /*
        Appends an InTypeCountedAnsiString field to the payload.
        Compatible with: TypeCountedMbcsString, TypeCountedMbcsXml, TypeCountedMbcsJson.
        */
        void AddCountedString(_In_reads_(cch) char const* pch, UINT16 cch)
        {
            m_dataBuilder.AddCountedString(pch, cch);
        }

        /*
        Appends an InTypeCountedString field to the payload.
        Compatible with: TypeCountedUtf16String, TypeCountedUtf16Xml, TypeCountedUtf16Json.
        */
        void AddCountedString(_In_reads_(cch) wchar_t const* pch, UINT16 cch)
        {
            m_dataBuilder.AddCountedString(pch, cch);
        }

        /*
        Appends an InTypeAnsiString field to the payload.
        Compatible with: TypeMbcsString, TypeMbcsXml, TypeMbcsJson.
        */
        void AddString(_In_z_ char const* psz)
        {
            m_dataBuilder.AddString(psz);
        }

        /*
        Appends a InTypeUnicodeString field to the payload.
        Compatible with: TypeUtf16String, TypeUtf16Xml, TypeUtf16Json
        */
        void AddString(_In_z_ wchar_t const* psz)
        {
            m_dataBuilder.AddString(psz);
        }

        /*
        Appends a raw byte to the payload.
        */
        void AddByte(UINT8 val)
        {
            m_dataBuilder.AddByte(val);
        }

        /*
        Appends raw bytes to the payload.
        */
        void AddBytes(_In_reads_bytes_(cbData) void const* pbData, unsigned cbData)
        {
            m_dataBuilder.AddBytes(pbData, cbData);
        }
    };

#pragma endregion

#pragma region Event

    template<class ByteBufferTy>
    class Event
        : public EventBuilder<ByteBufferTy>
    {
        Event(Event const&); // = delete
        void operator=(Event const&); // = delete

        ByteBufferTy m_metaBuffer;
        ByteBufferTy m_dataBuffer;
        EventDescriptor m_descriptor;
        GUID const* m_pActivityId;
        GUID const* m_pRelatedActivityId;
        EventState m_state;

    public:

        Event(
            _In_z_ char const* szUtf8Name,
            UCHAR level = 5,
            ULONGLONG keywords = 0,
            UINT32 tags = 0,
            UCHAR opcode = 0,
            USHORT task = 0,
            GUID const* pActivityId = 0,
            GUID const* pRelatedActivityId = 0)
            : EventBuilder<ByteBufferTy>(m_metaBuffer, m_dataBuffer, m_state)
            , m_descriptor(level, keywords, opcode, task)
            , m_pActivityId(pActivityId)
            , m_pRelatedActivityId(pRelatedActivityId)
            , m_state(EventStateOpen)
        {
            EventMetadataBuilder<ByteBufferTy>(m_metaBuffer).Begin(szUtf8Name, tags);
        }

        Event(
            _In_z_ wchar_t const* szUtf16Name,
            UCHAR level = 5,
            ULONGLONG keywords = 0,
            UINT32 tags = 0,
            UCHAR opcode = 0,
            USHORT task = 0,
            GUID const* pActivityId = 0,
            GUID const* pRelatedActivityId = 0)
            : EventBuilder<ByteBufferTy>(m_metaBuffer, m_dataBuffer, m_state)
            , m_descriptor(level, keywords, opcode, task)
            , m_pActivityId(pActivityId)
            , m_pRelatedActivityId(pRelatedActivityId)
            , m_state(EventStateOpen)
        {
            EventMetadataBuilder<ByteBufferTy>(m_metaBuffer).Begin(szUtf16Name, tags);
        }

        /*
        Clears the metadata and the payload.
        */
        template<class CharTy>
        void Reset(
            _In_z_ CharTy const* szUtfName,
            UCHAR level = 5,
            ULONGLONG keywords = 0,
            UINT32 tags = 0,
            UCHAR opcode = 0,
            USHORT task = 0,
            GUID* pActivityId = 0,
            GUID* pRelatedActivityId = 0)
        {
            m_metaBuffer.clear();
            m_dataBuffer.clear();
            m_descriptor.Reset(level, keywords, opcode, task);
            m_pActivityId = pActivityId;
            m_pRelatedActivityId = pRelatedActivityId;
            m_state = EventStateOpen;
            EventMetadataBuilder<ByteBufferTy>(m_metaBuffer).Begin(szUtfName, tags);
        }

        /*
        Clears the payload without clearing the metadata.
        */
        void ResetData()
        {
            m_dataBuffer.clear();
        }

        /*
        Sends the event to ETW using EventWriteTransfer.
        */
        HRESULT Write(Provider const& provider)
        {
            HRESULT hr = E_INVALIDARG;
            if (PrepareEvent())
            {
                hr = provider.Write(
                    m_descriptor,
                    &m_metaBuffer.front(),
                    m_dataBuffer.empty() ? NULL : &m_dataBuffer.front(),
                    static_cast<UINT32>(m_dataBuffer.size()),
                    m_pActivityId,
                    m_pRelatedActivityId);
            }
            return hr;
        }

        /*
        Sends the event to ETW using EventWriteTransfer.
        */
        HRESULT Write(
            REGHANDLE hProvider,
            _In_count_x_((UINT16*)pProviderMetadata) UINT8 const* pProviderMetadata)
        {
            HRESULT hr = E_INVALIDARG;

            if (PrepareEvent())
            {
                EVENT_DATA_DESCRIPTOR dataDesc[3];
                ULONG cDataDesc = 2;
                if (!m_dataBuffer.empty())
                {
                    EventDataDescCreate(
                        &dataDesc[cDataDesc++],
                        &m_dataBuffer.front(),
                        static_cast<ULONG>(m_dataBuffer.size()));
                }

                hr = ::tld::WriteEvent(
                    hProvider,
                    m_descriptor,
                    pProviderMetadata,
                    &m_metaBuffer.front(),
                    cDataDesc,
                    dataDesc,
                    m_pActivityId,
                    m_pRelatedActivityId);
            }

            return hr;
        }

#if TLD_HAVE_EVENT_WRITE_EX == 1

        /*
        Sends the event to ETW using EventWriteEx.
        */
        HRESULT WriteEx(
            Provider const& provider,
            ULONG64 filter,
            ULONG flags)
        {
            HRESULT hr = E_INVALIDARG;
            if (PrepareEvent())
            {
                hr = provider.WriteEx(
                    m_descriptor,
                    &m_metaBuffer.front(),
                    m_dataBuffer.empty() ? NULL : &m_dataBuffer.front(),
                    static_cast<UINT32>(m_dataBuffer.size()),
                    filter,
                    flags,
                    m_pActivityId,
                    m_pRelatedActivityId);
            }
            return hr;
        }

        /*
        Sends the event to ETW using EventWriteEx.
        */
        HRESULT WriteEx(
            REGHANDLE hProvider,
            _In_count_x_((UINT16*)pEventMetadata) UINT8 const* pProviderMetadata,
            ULONG64 filter,
            ULONG flags)
        {
            HRESULT hr = E_INVALIDARG;

            if (PrepareEvent())
            {
                EVENT_DATA_DESCRIPTOR dataDesc[3];
                ULONG cDataDesc = 2;
                if (!m_dataBuffer.empty())
                {
                    EventDataDescCreate(
                        &dataDesc[cDataDesc++],
                        &m_dataBuffer.front(),
                        static_cast<ULONG>(m_dataBuffer.size()));
                }

                hr = ::tld::WriteEventEx(
                    hProvider,
                    m_descriptor,
                    pProviderMetadata,
                    &m_metaBuffer.front(),
                    cDataDesc,
                    dataDesc,
                    filter,
                    flags,
                    m_pActivityId,
                    m_pRelatedActivityId);
            }

            return hr;
        }

#endif // TLD_HAVE_EVENT_WRITE_EX

#pragma region Basic properties

        UCHAR Channel() const
        {
            return m_descriptor.Channel;
        }

        /*
        Note: the default channel is 11 (WINEVENT_CHANNEL_TRACELOGGING). 
        Other channels are only supported if the provider is running on
        Windows 10 or later. If a provider is running on an earlier version
        of Windows and it uses a channel other than 11, TDH will not be able
        to decode the events.
        */
        void Channel(UCHAR channel)
        {
            m_descriptor.Channel = channel;
        }

        UCHAR Level() const
        {
            return m_descriptor.Level;
        }

        void Level(UCHAR value)
        {
            m_descriptor.Level = value;
        }

        ULONGLONG Keywords() const
        {
            return m_descriptor.Keyword;
        }

        void Keywords(ULONGLONG value)
        {
            m_descriptor.Keyword = value;
        }

        UCHAR Opcode() const
        {
            return m_descriptor.Opcode;
        }

        void Opcode(UCHAR value)
        {
            m_descriptor.Opcode = value;
        }

        USHORT Task() const
        {
            return m_descriptor.Task;
        }

        void Task(USHORT value)
        {
            m_descriptor.Task = value;
        }

        const GUID* ActivityId() const
        {
            return m_pActivityId;
        }

        void ActivityId(GUID* pValue)
        {
            m_pActivityId = pValue;
        }

        const GUID* RelatedActivityId() const
        {
            return m_pRelatedActivityId;
        }

        void RelatedActivityId(GUID* pValue)
        {
            m_pRelatedActivityId = pValue;
        }

        bool IsEnabledFor(Provider const& provider) const
        {
            return provider.IsEnabled(m_descriptor.Level, m_descriptor.Keyword);
        }

#pragma endregion

    private:

        bool PrepareEvent()
        {
            if (m_state == EventStateOpen)
            {
                PrepareEventImpl();
            }
            return m_state == EventStateClosed;
        }

        void PrepareEventImpl()
        {
            m_state =
                EventMetadataBuilder<ByteBufferTy>(m_metaBuffer).End()
                ? EventStateClosed
                : EventStateError;
        }
    };

#pragma endregion
}
// namespace tld
