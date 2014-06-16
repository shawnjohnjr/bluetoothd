/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "core-io.h"
#include "service.h"

bt_status_t (*service_handler[256])(const struct pdu*);

register_func
  (* const register_service[256])(unsigned char, void (*)(struct pdu_wbuf*)) = {
  /* SERVICE_CORE is special and not handled here */
};

int (*unregister_service[256])() = {
};
