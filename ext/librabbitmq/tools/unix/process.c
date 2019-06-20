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

#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "process.h"

extern char **environ;

void pipeline(const char *const *argv, struct pipeline *pl) {
  posix_spawn_file_actions_t file_acts;

  int pipefds[2];
  if (pipe(pipefds)) {
    die_errno(errno, "pipe");
  }

  die_errno(posix_spawn_file_actions_init(&file_acts),
            "posix_spawn_file_actions_init");
  die_errno(posix_spawn_file_actions_adddup2(&file_acts, pipefds[0], 0),
            "posix_spawn_file_actions_adddup2");
  die_errno(posix_spawn_file_actions_addclose(&file_acts, pipefds[0]),
            "posix_spawn_file_actions_addclose");
  die_errno(posix_spawn_file_actions_addclose(&file_acts, pipefds[1]),
            "posix_spawn_file_actions_addclose");

  die_errno(posix_spawnp(&pl->pid, argv[0], &file_acts, NULL,
                         (char *const *)argv, environ),
            "posix_spawnp: %s", argv[0]);

  die_errno(posix_spawn_file_actions_destroy(&file_acts),
            "posix_spawn_file_actions_destroy");

  if (close(pipefds[0])) {
    die_errno(errno, "close");
  }

  pl->infd = pipefds[1];
}

int finish_pipeline(struct pipeline *pl) {
  int status;

  if (close(pl->infd)) {
    die_errno(errno, "close");
  }
  if (waitpid(pl->pid, &status, 0) < 0) {
    die_errno(errno, "waitpid");
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
