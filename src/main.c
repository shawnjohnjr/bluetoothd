/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <unistd.h>
#include "loop.h"
#include "task.h"

static int
init(void* data)
{
  if (init_task_queue() < 0)
    goto err_init_task_queue;

  return 0;
err_init_task_queue:
  return -1;
}

int
main(int argc, char* argv[])
{
  if (epoll_loop(init, NULL) < 0)
    goto err_epoll_loop;

  exit(EXIT_SUCCESS);
err_epoll_loop:
  exit(EXIT_FAILURE);
}
