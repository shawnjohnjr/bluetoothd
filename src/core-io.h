/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

struct pdu_wbuf;

int
init_core_io(void (*send_pdu_cb)(struct pdu_wbuf*));

void
uninit_core_io(void);
