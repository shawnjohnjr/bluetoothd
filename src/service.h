/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <hardware/bluetooth.h>

struct pdu;
struct pdu_wbuf;

typedef bt_status_t (*register_func)(const struct pdu*);

extern bt_status_t (*service_handler[256])(const struct pdu*);

extern register_func
  (* const register_service[256])(unsigned char, void (*)(struct pdu_wbuf*));

extern int (*unregister_service[256])(void);
