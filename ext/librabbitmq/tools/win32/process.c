/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2013
 * Alan Antonuk. All Rights Reserved.
 *
 * Portions created by VMware are Copyright (c) 2007-2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Portions created by Tony Garnock-Jones are Copyright (c) 2009-2010
 * VMware, Inc. and Tony Garnock-Jones. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <io.h>
#include <stdio.h>
#include <windows.h>

#include "common.h"
#include "process.h"

void die_windows_error(const char *fmt, ...) {
  char *msg;

  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (!FormatMessage(
          FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL,
          GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          (LPSTR)&msg, 0, NULL)) {
    msg = "(failed to retrieve Windows error message)";
  }

  fprintf(stderr, ": %s\n", msg);
  exit(1);
}

static char *make_command_line(const char *const *argv) {
  int i;
  size_t len = 1; /* initial quotes */
  char *buf;
  char *dest;

  /* calculate the length of the required buffer, making worst
     case assumptions for simplicity */
  for (i = 0;;) {
    /* each character could need escaping */
    len += strlen(argv[i]) * 2;

    if (!argv[++i]) {
      break;
    }

    len += 3; /* quotes, space, quotes */
  }

  len += 2; /* final quotes and the terminating zero */

  dest = buf = malloc(len);
  if (!buf) {
    die("allocating memory for subprocess command line");
  }

  /* Here we perform the inverse of the CommandLineToArgvW
     function.  Note that its rules are slightly crazy: A
     sequence of backslashes only act to escape if followed by
     double quotes.  A sequence of backslashes not followed by
     double quotes is untouched. */

  for (i = 0;;) {
    const char *src = argv[i];
    int backslashes = 0;

    *dest++ = '\"';

    for (;;) {
      switch (*src) {
        case 0:
          goto done;

        case '\"':
          for (; backslashes; backslashes--) {
            *dest++ = '\\';
          }

          *dest++ = '\\';
          *dest++ = '\"';
          break;

        case '\\':
          backslashes++;
          *dest++ = '\\';
          break;

        default:
          backslashes = 0;
          *dest++ = *src;
          break;
      }

      src++;
    }
  done:
    for (; backslashes; backslashes--) {
      *dest++ = '\\';
    }

    *dest++ = '\"';

    if (!argv[++i]) {
      break;
    }

    *dest++ = ' ';
  }

  *dest++ = 0;
  return buf;
}

void pipeline(const char *const *argv, struct pipeline *pl) {
  HANDLE in_read_handle, in_write_handle;
  SECURITY_ATTRIBUTES sec_attr;
  PROCESS_INFORMATION proc_info;
  STARTUPINFO start_info;
  char *cmdline = make_command_line(argv);

  sec_attr.nLength = sizeof sec_attr;
  sec_attr.bInheritHandle = TRUE;
  sec_attr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&in_read_handle, &in_write_handle, &sec_attr, 0)) {
    die_windows_error("CreatePipe");
  }

  if (!SetHandleInformation(in_write_handle, HANDLE_FLAG_INHERIT, 0)) {
    die_windows_error("SetHandleInformation");
  }

  /* when in Rome... */
  ZeroMemory(&proc_info, sizeof proc_info);
  ZeroMemory(&start_info, sizeof start_info);

  start_info.cb = sizeof start_info;
  start_info.dwFlags |= STARTF_USESTDHANDLES;

  if ((start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE)) ==
          INVALID_HANDLE_VALUE ||
      (start_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE)) ==
          INVALID_HANDLE_VALUE) {
    die_windows_error("GetStdHandle");
  }

  start_info.hStdInput = in_read_handle;

  if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL,
                     &start_info, &proc_info)) {
    die_windows_error("CreateProcess");
  }

  free(cmdline);

  if (!CloseHandle(proc_info.hThread)) {
    die_windows_error("CloseHandle for thread");
  }
  if (!CloseHandle(in_read_handle)) {
    die_windows_error("CloseHandle");
  }

  pl->proc_handle = proc_info.hProcess;
  pl->infd = _open_osfhandle((intptr_t)in_write_handle, 0);
}

int finish_pipeline(struct pipeline *pl) {
  DWORD code;

  if (close(pl->infd)) {
    die_errno(errno, "close");
  }

  for (;;) {
    if (!GetExitCodeProcess(pl->proc_handle, &code)) {
      die_windows_error("GetExitCodeProcess");
    }
    if (code != STILL_ACTIVE) {
      break;
    }

    if (WaitForSingleObject(pl->proc_handle, INFINITE) == WAIT_FAILED) {
      die_windows_error("WaitForSingleObject");
    }
  }

  if (!CloseHandle(pl->proc_handle)) {
    die_windows_error("CloseHandle for process");
  }

  return code;
}
