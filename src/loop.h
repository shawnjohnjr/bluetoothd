/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <stdint.h>

int
add_fd_to_epoll_loop(int fd, uint32_t epoll_events,
                     void (*func)(int, uint32_t, void*), void* data);

void
remove_fd_from_epoll_loop(int fd);

int
epoll_loop(int (*init)(void*), void* data);
