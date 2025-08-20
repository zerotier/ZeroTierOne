/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_attributes-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace system
{

/**
  Deprecated, use @code cpu.logical_number @endcode instead.
 */
static constexpr const char *kSystemCpuLogicalNumber = "system.cpu.logical_number";

/**
  Deprecated, use @code cpu.mode @endcode instead.

  @deprecated
  {"note": "Replaced by @code cpu.mode @endcode.", "reason": "renamed", "renamed_to": "cpu.mode"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kSystemCpuState = "system.cpu.state";

/**
  The device identifier
 */
static constexpr const char *kSystemDevice = "system.device";

/**
  The filesystem mode
 */
static constexpr const char *kSystemFilesystemMode = "system.filesystem.mode";

/**
  The filesystem mount path
 */
static constexpr const char *kSystemFilesystemMountpoint = "system.filesystem.mountpoint";

/**
  The filesystem state
 */
static constexpr const char *kSystemFilesystemState = "system.filesystem.state";

/**
  The filesystem type
 */
static constexpr const char *kSystemFilesystemType = "system.filesystem.type";

/**
  The memory state
 */
static constexpr const char *kSystemMemoryState = "system.memory.state";

/**
  Deprecated, use @code network.connection.state @endcode instead.

  @deprecated
  {"note": "Replaced by @code network.connection.state @endcode.", "reason": "renamed",
  "renamed_to": "network.connection.state"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kSystemNetworkState = "system.network.state";

/**
  The paging access direction
 */
static constexpr const char *kSystemPagingDirection = "system.paging.direction";

/**
  The memory paging state
 */
static constexpr const char *kSystemPagingState = "system.paging.state";

/**
  The memory paging type
 */
static constexpr const char *kSystemPagingType = "system.paging.type";

/**
  The process state, e.g., <a
  href="https://man7.org/linux/man-pages/man1/ps.1.html#PROCESS_STATE_CODES">Linux Process State
  Codes</a>
 */
static constexpr const char *kSystemProcessStatus = "system.process.status";

/**
  Deprecated, use @code system.process.status @endcode instead.

  @deprecated
  {"note": "Replaced by @code system.process.status @endcode.", "reason": "renamed", "renamed_to":
  "system.process.status"}
 */
OPENTELEMETRY_DEPRECATED static constexpr const char *kSystemProcessesStatus =
    "system.processes.status";

namespace SystemCpuStateValues
{
/**
  none
 */
static constexpr const char *kUser = "user";

/**
  none
 */
static constexpr const char *kSystem = "system";

/**
  none
 */
static constexpr const char *kNice = "nice";

/**
  none
 */
static constexpr const char *kIdle = "idle";

/**
  none
 */
static constexpr const char *kIowait = "iowait";

/**
  none
 */
static constexpr const char *kInterrupt = "interrupt";

/**
  none
 */
static constexpr const char *kSteal = "steal";

}  // namespace SystemCpuStateValues

namespace SystemFilesystemStateValues
{
/**
  none
 */
static constexpr const char *kUsed = "used";

/**
  none
 */
static constexpr const char *kFree = "free";

/**
  none
 */
static constexpr const char *kReserved = "reserved";

}  // namespace SystemFilesystemStateValues

namespace SystemFilesystemTypeValues
{
/**
  none
 */
static constexpr const char *kFat32 = "fat32";

/**
  none
 */
static constexpr const char *kExfat = "exfat";

/**
  none
 */
static constexpr const char *kNtfs = "ntfs";

/**
  none
 */
static constexpr const char *kRefs = "refs";

/**
  none
 */
static constexpr const char *kHfsplus = "hfsplus";

/**
  none
 */
static constexpr const char *kExt4 = "ext4";

}  // namespace SystemFilesystemTypeValues

namespace SystemMemoryStateValues
{
/**
  none
 */
static constexpr const char *kUsed = "used";

/**
  none
 */
static constexpr const char *kFree = "free";

/**
  none
 */
static constexpr const char *kShared = "shared";

/**
  none
 */
static constexpr const char *kBuffers = "buffers";

/**
  none
 */
static constexpr const char *kCached = "cached";

}  // namespace SystemMemoryStateValues

namespace SystemNetworkStateValues
{
/**
  none
 */
static constexpr const char *kClose = "close";

/**
  none
 */
static constexpr const char *kCloseWait = "close_wait";

/**
  none
 */
static constexpr const char *kClosing = "closing";

/**
  none
 */
static constexpr const char *kDelete = "delete";

/**
  none
 */
static constexpr const char *kEstablished = "established";

/**
  none
 */
static constexpr const char *kFinWait1 = "fin_wait_1";

/**
  none
 */
static constexpr const char *kFinWait2 = "fin_wait_2";

/**
  none
 */
static constexpr const char *kLastAck = "last_ack";

/**
  none
 */
static constexpr const char *kListen = "listen";

/**
  none
 */
static constexpr const char *kSynRecv = "syn_recv";

/**
  none
 */
static constexpr const char *kSynSent = "syn_sent";

/**
  none
 */
static constexpr const char *kTimeWait = "time_wait";

}  // namespace SystemNetworkStateValues

namespace SystemPagingDirectionValues
{
/**
  none
 */
static constexpr const char *kIn = "in";

/**
  none
 */
static constexpr const char *kOut = "out";

}  // namespace SystemPagingDirectionValues

namespace SystemPagingStateValues
{
/**
  none
 */
static constexpr const char *kUsed = "used";

/**
  none
 */
static constexpr const char *kFree = "free";

}  // namespace SystemPagingStateValues

namespace SystemPagingTypeValues
{
/**
  none
 */
static constexpr const char *kMajor = "major";

/**
  none
 */
static constexpr const char *kMinor = "minor";

}  // namespace SystemPagingTypeValues

namespace SystemProcessStatusValues
{
/**
  none
 */
static constexpr const char *kRunning = "running";

/**
  none
 */
static constexpr const char *kSleeping = "sleeping";

/**
  none
 */
static constexpr const char *kStopped = "stopped";

/**
  none
 */
static constexpr const char *kDefunct = "defunct";

}  // namespace SystemProcessStatusValues

namespace SystemProcessesStatusValues
{
/**
  none
 */
static constexpr const char *kRunning = "running";

/**
  none
 */
static constexpr const char *kSleeping = "sleeping";

/**
  none
 */
static constexpr const char *kStopped = "stopped";

/**
  none
 */
static constexpr const char *kDefunct = "defunct";

}  // namespace SystemProcessesStatusValues

}  // namespace system
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
