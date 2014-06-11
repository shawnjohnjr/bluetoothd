/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bt-proto.h"
#include "bt-core-io.h"
#include "core-io.h"
#include "service.h"

bt_status_t (*service_handler[256])(const struct pdu*);

register_func
  (* const register_service[256])(unsigned char, void (*)(struct pdu_wbuf*)) = {
  /* SERVICE_CORE is special and not handled here */
  [SERVICE_BT_CORE] = register_bt_core
};

int (*unregister_service[256])() = {
  [SERVICE_BT_CORE] = unregister_bt_core
};
