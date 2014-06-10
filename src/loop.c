/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "log.h"
#include "loop.h"

#define MAXNFDS 64

#define ARRAYLEN(x) \
  (sizeof(x) / sizeof(x[0]))

struct fd_state {
  struct epoll_event event;
  void (*func)(int, uint32_t, void*);
  void* data;
};

static struct fd_state fd_state[MAXNFDS];
static int epfd;

int
add_fd_to_epoll_loop(int fd, uint32_t epoll_events,
                     void (*func)(int, uint32_t, void*), void* data)
{
  int enabled;
  int res;

  assert(fd >= 0);
  assert(func);

  if ((ssize_t)fd >= (ssize_t)ARRAYLEN(fd_state)) {
    ALOGE("too many open file descriptors");
    goto err_fd;
  }

  enabled = !!fd_state[fd].event.events;

  fd_state[fd].event.events = epoll_events;
  fd_state[fd].event.data.fd = fd;

  if (enabled)
    res = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &fd_state[fd].event);
  else
    res = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &fd_state[fd].event);

  if (res < 0) {
    ALOGE_ERRNO("epoll_ctl");
    goto err_epoll_ctl;
  }

  fd_state[fd].func = func;
  fd_state[fd].data = data;

  return 0;
err_epoll_ctl:
err_fd:
  return -1;
}

void
remove_fd_from_epoll_loop(int fd)
{
  int res;

  assert(fd_state[fd].events);

  res = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
  if (res < 0)
    ALOGW_ERRNO("epoll_ctl");
}

static int
epoll_loop_iteration(void)
{
  struct epoll_event events[MAXNFDS];
  int nevents, i;

  nevents = TEMP_FAILURE_RETRY(epoll_wait(epfd, events, ARRAYLEN(events), -1));
  if (nevents < 0) {
    ALOGE_ERRNO("epoll_wait");
    goto err_epoll_wait;
  }

  for (i = 0; i < nevents; ++i) {
    int fd = events[i].data.fd;

    assert(fd_state[fd].func);
    fd_state[fd].func(fd, events[i].events, fd_state[fd].data);
  }
  return 0;

err_epoll_wait:
  return -1;
}

int
epoll_loop(int (*init)(void*), void* data)
{
  int res;

  epfd = epoll_create(ARRAYLEN(fd_state));
  if (epfd < 0) {
    ALOGE_ERRNO("epoll_create");
    goto err_epoll_create;
  }

  if (init && (init(data) < 0))
    goto err_init;

  do {
    res = epoll_loop_iteration();
  } while (!res);

  if (res < 0)
    goto err_epoll_loop_iteration;

  if (TEMP_FAILURE_RETRY(close(epfd)) < 0)
    ALOGW_ERRNO("close");

  return 0;
err_epoll_loop_iteration:
err_init:
  if (TEMP_FAILURE_RETRY(close(epfd)) < 0)
    ALOGW_ERRNO("close");
err_epoll_create:
  return -1;
}
