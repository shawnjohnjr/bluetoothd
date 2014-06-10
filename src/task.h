/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

struct task {
  int (*func)(void* data);
  void* data;
};

int
init_task_queue(void);

void
uninit_task_queue(void);

int
run_task(int (*func)(void*), void* data);
