/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <hardware/bluetooth.h>

struct pdu;
struct pdu_wbuf;

int
core_register_module(unsigned char service, unsigned char mode);

int
core_unregister_module(unsigned char service);

int
init_core(bt_status_t (*core_handler)(const struct pdu*),
          void (*send_pdu_cb)(struct pdu_wbuf*));

void
uninit_core(void);
