/*
 * Portions created by Alan Antonuk are Copyright (c) 2013-2014 Alan Antonuk.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "amqp_time.h"
#include "amqp.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || \
     defined(__MINGW32__) || defined(__MINGW64__))
#define AMQP_WIN_TIMER_API
#elif (defined(machintosh) || defined(__APPLE__) || defined(__APPLE_CC__))
#define AMQP_MAC_TIMER_API
#else
#define AMQP_POSIX_TIMER_API
#endif

#ifdef AMQP_WIN_TIMER_API
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

uint64_t amqp_get_monotonic_timestamp(void) {
  static double NS_PER_COUNT = 0;
  LARGE_INTEGER perf_count;

  if (0 == NS_PER_COUNT) {
    LARGE_INTEGER perf_frequency;
    if (!QueryPerformanceFrequency(&perf_frequency)) {
      return 0;
    }
    NS_PER_COUNT = (double)AMQP_NS_PER_S / perf_frequency.QuadPart;
  }

  if (!QueryPerformanceCounter(&perf_count)) {
    return 0;
  }

  return (uint64_t)(perf_count.QuadPart * NS_PER_COUNT);
}
#endif /* AMQP_WIN_TIMER_API */

#ifdef AMQP_MAC_TIMER_API
#include <mach/mach_time.h>

uint64_t amqp_get_monotonic_timestamp(void) {
  static mach_timebase_info_data_t s_timebase = {0, 0};
  uint64_t timestamp;

  timestamp = mach_absolute_time();

  if (s_timebase.denom == 0) {
    mach_timebase_info(&s_timebase);
    if (0 == s_timebase.denom) {
      return 0;
    }
  }

  timestamp *= (uint64_t)s_timebase.numer;
  timestamp /= (uint64_t)s_timebase.denom;

  return timestamp;
}
#endif /* AMQP_MAC_TIMER_API */

#ifdef AMQP_POSIX_TIMER_API
#include <time.h>

uint64_t amqp_get_monotonic_timestamp(void) {
#ifdef __hpux
  return (uint64_t)gethrtime();
#else
  struct timespec tp;
  if (-1 == clock_gettime(CLOCK_MONOTONIC, &tp)) {
    return 0;
  }

  return ((uint64_t)tp.tv_sec * AMQP_NS_PER_S + (uint64_t)tp.tv_nsec);
#endif
}
#endif /* AMQP_POSIX_TIMER_API */

int amqp_time_from_now(amqp_time_t *time, struct timeval *timeout) {
  uint64_t now_ns;
  uint64_t delta_ns;

  assert(NULL != time);

  if (NULL == timeout) {
    *time = amqp_time_infinite();
    return AMQP_STATUS_OK;
  }
  if (0 == timeout->tv_sec && 0 == timeout->tv_usec) {
    *time = amqp_time_immediate();
    return AMQP_STATUS_OK;
  }

  if (timeout->tv_sec < 0 || timeout->tv_usec < 0) {
    return AMQP_STATUS_INVALID_PARAMETER;
  }

  delta_ns = (uint64_t)timeout->tv_sec * AMQP_NS_PER_S +
             (uint64_t)timeout->tv_usec * AMQP_NS_PER_US;

  now_ns = amqp_get_monotonic_timestamp();
  if (0 == now_ns) {
    return AMQP_STATUS_TIMER_FAILURE;
  }

  time->time_point_ns = now_ns + delta_ns;
  if (now_ns > time->time_point_ns || delta_ns > time->time_point_ns) {
    return AMQP_STATUS_INVALID_PARAMETER;
  }

  return AMQP_STATUS_OK;
}

int amqp_time_s_from_now(amqp_time_t *time, int seconds) {
  uint64_t now_ns;
  uint64_t delta_ns;
  assert(NULL != time);

  if (0 >= seconds) {
    *time = amqp_time_infinite();
    return AMQP_STATUS_OK;
  }

  now_ns = amqp_get_monotonic_timestamp();
  if (0 == now_ns) {
    return AMQP_STATUS_TIMER_FAILURE;
  }

  delta_ns = (uint64_t)seconds * AMQP_NS_PER_S;
  time->time_point_ns = now_ns + delta_ns;
  if (now_ns > time->time_point_ns || delta_ns > time->time_point_ns) {
    return AMQP_STATUS_INVALID_PARAMETER;
  }

  return AMQP_STATUS_OK;
}

amqp_time_t amqp_time_immediate(void) {
  amqp_time_t time;
  time.time_point_ns = 0;
  return time;
}

amqp_time_t amqp_time_infinite(void) {
  amqp_time_t time;
  time.time_point_ns = UINT64_MAX;
  return time;
}

int amqp_time_ms_until(amqp_time_t time) {
  uint64_t now_ns;
  uint64_t delta_ns;
  int left_ms;

  if (UINT64_MAX == time.time_point_ns) {
    return -1;
  }
  if (0 == time.time_point_ns) {
    return 0;
  }

  now_ns = amqp_get_monotonic_timestamp();
  if (0 == now_ns) {
    return AMQP_STATUS_TIMER_FAILURE;
  }

  if (now_ns >= time.time_point_ns) {
    return 0;
  }

  delta_ns = time.time_point_ns - now_ns;
  left_ms = (int)(delta_ns / AMQP_NS_PER_MS);

  return left_ms;
}

int amqp_time_tv_until(amqp_time_t time, struct timeval *in,
                       struct timeval **out) {
  uint64_t now_ns;
  uint64_t delta_ns;

  assert(in != NULL);
  if (UINT64_MAX == time.time_point_ns) {
    *out = NULL;
    return AMQP_STATUS_OK;
  }
  if (0 == time.time_point_ns) {
    in->tv_sec = 0;
    in->tv_usec = 0;
    *out = in;
    return AMQP_STATUS_OK;
  }

  now_ns = amqp_get_monotonic_timestamp();
  if (0 == now_ns) {
    return AMQP_STATUS_TIMER_FAILURE;
  }

  if (now_ns >= time.time_point_ns) {
    in->tv_sec = 0;
    in->tv_usec = 0;
    *out = in;
    return AMQP_STATUS_OK;
  }

  delta_ns = time.time_point_ns - now_ns;
  in->tv_sec = (int)(delta_ns / AMQP_NS_PER_S);
  in->tv_usec = (int)((delta_ns % AMQP_NS_PER_S) / AMQP_NS_PER_US);
  *out = in;

  return AMQP_STATUS_OK;
}

int amqp_time_has_past(amqp_time_t time) {
  uint64_t now_ns;
  if (UINT64_MAX == time.time_point_ns) {
    return AMQP_STATUS_OK;
  }

  now_ns = amqp_get_monotonic_timestamp();
  if (0 == now_ns) {
    return AMQP_STATUS_TIMER_FAILURE;
  }

  if (now_ns > time.time_point_ns) {
    return AMQP_STATUS_TIMEOUT;
  }
  return AMQP_STATUS_OK;
}

amqp_time_t amqp_time_first(amqp_time_t l, amqp_time_t r) {
  if (l.time_point_ns < r.time_point_ns) {
    return l;
  }
  return r;
}

int amqp_time_equal(amqp_time_t l, amqp_time_t r) {
  return l.time_point_ns == r.time_point_ns;
}
