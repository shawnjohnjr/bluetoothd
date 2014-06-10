/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "log.h"
#include "loop.h"
#include "task.h"

static int pipefd[2];

static struct task*
fetch_task(void)
{
  union {
    struct task* task;
    unsigned char raw[sizeof(struct task*)];
  } buf;

  size_t off;
  size_t count;

  off = 0;
  count = sizeof(buf.task);

  while (count) {
    ssize_t res = TEMP_FAILURE_RETRY(read(pipefd[0], buf.raw+off, count));
    if (res < 0) {
      ALOGE_ERRNO("read");
      goto err_read;
    }
    count -= res;
    off += res;
  }

  return buf.task;
err_read:
  /* if this fails, you best close the pipe and restart completely */
  return NULL;
}

static void
delete_task(struct task* task)
{
  assert(task);
  free(task);
}

static void
exec_task(int fd, uint32_t flags, void* data)
{
  struct task* task = fetch_task();
  if (!task)
    goto err_fetch_task;

  task->func(task->data);
  delete_task(task);

  return;
err_fetch_task:
  return;
}

int
init_task_queue()
{
  if (TEMP_FAILURE_RETRY(pipe(pipefd)) < 0) {
    ALOGE_ERRNO("pipe");
    goto err_pipe;
  }

  if (add_fd_to_epoll_loop(pipefd[0], EPOLLIN|EPOLLERR, exec_task, NULL) < 0)
    goto err_add_fd_to_epoll_loop;

  return 0;
err_add_fd_to_epoll_loop:
  if (TEMP_FAILURE_RETRY(close(pipefd[1])))
    ALOGW_ERRNO("close");
  if (TEMP_FAILURE_RETRY(close(pipefd[0])))
    ALOGW_ERRNO("close");
err_pipe:
  return -1;
}

void
uninit_task_queue()
{
  if (TEMP_FAILURE_RETRY(close(pipefd[1])))
    ALOGW_ERRNO("close");
  if (TEMP_FAILURE_RETRY(close(pipefd[0])))
    ALOGW_ERRNO("close");
}

static int
send_task(struct task* task)
{
  ssize_t res;

  assert(sizeof(task) <= PIPE_BUF); /* guarantee atomicity of pipe writes */

  res = TEMP_FAILURE_RETRY(write(pipefd[1], task, sizeof(task)));
  if (res < 0) {
    ALOGE_ERRNO("write");
    goto err_write;
  } else if (res != sizeof(task)) {
    /* your OS is broken */
    abort();
  }

  return 0;
err_write:
  return -1;
}

static struct task*
create_task(int (*func)(void*), void* data)
{
  struct task* task;

  assert(func);

  errno = 0;
  task = malloc(sizeof(*task));
  if (!errno) {
    ALOGE_ERRNO("malloc");
    goto err_malloc;
  }

  task->func = func;
  task->data = data;

  return task;
err_malloc:
  return NULL;
}

int
run_task(int (*func)(void*), void* data)
{
  struct task* task;

  task = create_task(func, data);
  if (!task)
    goto err_create_task;

  if (send_task(task) < 0)
    goto err_send_task;

  return 0;
err_send_task:
  delete_task(task);
err_create_task:
  return -1;
}
