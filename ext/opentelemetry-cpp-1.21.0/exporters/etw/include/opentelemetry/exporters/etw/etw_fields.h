/* // Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <unordered_map>

#include "opentelemetry/exporters/etw/utils.h"

/* clang-format off */
#ifdef CUSTOM_ETW_FIELDS_H
/* Customers may redefine the default field names by including CUSTOM_ETW_FIELDS_H header */
#  include CUSTOM_ETW_FIELDS_H
#else

/**

  List of configurable Field Name constants:

   Version                      - Schema version (optional for ETW exporter).
   _name                        - Built-in ETW name at envelope level (dedicated ETW field).
   _time                        - Built-in ETW time at envelope level (dedicated ETW field).
   SpanId                       - OT SpanId
   TraceId                      - OT TraceId
   StartTime                    - OT Span start time
   Kind                         - OT Span kind
   Name                         - OT Span name in ETW 'Payload["Name"]'
   ParentId                     - OT Span parentId
   Links                        - OT Span links array

  Other standard fields (reserved names) that may be appended by ETW channel:

   Level                        - a 1-byte integer that enables filtering based on the severity or verbosity of events
   ProviderGuid                 - ETW Provider Guid
   ProviderName                 - ETW Provider Name
   OpcodeName                   - Name of Opcode (e.g. Start, Stop)
   KeywordName                  - Name of Keyword
   TaskName                     - TaskName, could be handled as an alias to Payload['name']
   ChannelName                  - ETW Channel Name
   EventMessage                 - ETW Event Message string for unstructured events
   ActivityId                   - ActivityId for EventSource parenting (current event)
   RelatedActivityId            - RelatedActivityId for EventSource parenting (parent event)
   Pid                          - Process Id
   Tid                          - Thread Id

  Example "Span" as shown in Visual Studio "Diagnostic Events" view. EventName="Span":

    {
      "Timestamp": "2021-04-01T00:33:25.5876605-07:00",
      "ProviderName": "OpenTelemetry-ETW-TLD",
      "Id": 20,
      "Message": null,
      "ProcessId": 10424,
      "Level": "Always",
      "Keywords": "0x0000000000000000",
      "EventName": "Span",
      "ActivityID": "56f2366b-5475-496f-0000-000000000000",
      "RelatedActivityID": null,
      "Payload": {
        "Duration": 0,
        "Name": "B.max",
        "ParentId": "8ad900d0587fad4a",
        "SpanId": "6b36f25675546f49",
        "StartTime": "2021-04-01T07:33:25.587Z",
        "TraceId": "8f8ac710c37c5a419f0fe574f335e986"
      }
    }

  Example named Event on Span. Note that EventName="MyEvent2" in this case:

    {
      "Timestamp": "2021-04-01T00:33:22.5848789-07:00",
      "ProviderName": "OpenTelemetry-ETW-TLD",
      "Id": 15,
      "Message": null,
      "ProcessId": 10424,
      "Level": "Always",
      "Keywords": "0x0000000000000000",
      "EventName": "MyEvent2",
      "ActivityID": null,
      "RelatedActivityID": null,
      "Payload": {
        "SpanId": "0da9f6bf7524a449",
        "TraceId": "7715c9d490f54f44a5d0c6b62570f1b2",
        "strKey": "anotherValue",
        "uint32Key": 9876,
        "uint64Key": 987654321
      }
    }

 */
#  define ETW_FIELD_VERSION         "Version"               /* Event version           */
#  define ETW_FIELD_TYPE            "Type"              	/* Event type              */
#  define ETW_FIELD_NAME            "_name"              	/* Event name              */
#  define ETW_FIELD_TIME            "_time"              	/* Event time              */
#  define ETW_FIELD_OPCODE          "OpCode"            	/* OpCode for TraceLogging */

#  define ETW_FIELD_TRACE_ID        "TraceId"        		/* Trace Id                */
#  define ETW_FIELD_SPAN_ID         "SpanId"         		/* Span Id                 */
#  define ETW_FIELD_SPAN_PARENTID   "ParentId"              /* Span ParentId           */
#  define ETW_FIELD_SPAN_KIND       "Kind"                  /* Span Kind               */
#  define ETW_FIELD_SPAN_LINKS      "Links"                 /* Span Links array        */

#  define ETW_FIELD_SPAN_LINKS_TO_SPAN_ID  "toSpanId"         /* Span Links toSpanId     */
#  define ETW_FIELD_SPAN_LINKS_TO_TRACE_ID "toTraceId"        /* Span Links toTraceId    */

#  define ETW_FIELD_PAYLOAD_NAME    "Name"                  /* ETW Payload["Name"]     */

/* Span option constants */
#  define ETW_FIELD_STARTTIME       "StartTime"             /* Operation start time    */
#  define ETW_FIELD_DURATION        "Duration"              /* Operation duration      */
#  define ETW_FIELD_STATUSCODE      "StatusCode"            /* Span status code        */
#  define ETW_FIELD_STATUSMESSAGE   "StatusMessage"         /* Span status message     */
#  define ETW_FIELD_SUCCESS         "Success"               /* Span success            */
#  define ETW_FIELD_TIMESTAMP       "Timestamp"             /* Log timestamp   */

/* Value constants */
#  define ETW_VALUE_SPAN            "Span"                  /* ETW event name for Span */
#  define ETW_VALUE_LOG             "Log"                   /* ETW event name for Log */

#  define ETW_VALUE_SPAN_START      "SpanStart"             /* ETW for Span Start      */
#  define ETW_VALUE_SPAN_END        "SpanEnd"               /* ETW for Span Start      */


# define ETW_FIELD_ENV_PROPERTIES   "env_properties"        /* ETW event_properties with JSON string */

/* Log specific */
# define ETW_FIELD_LOG_BODY               "body"                 /* Log body   */
# define ETW_FIELD_LOG_SEVERITY_TEXT      "severityText"         /* Sev text  */
# define ETW_FIELD_LOG_SEVERITY_NUM       "severityNumber"       /* Sev num   */

#endif

/* clang-format on */
