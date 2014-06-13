/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>

bt_status_t
bt_sock_listen(btsock_type_t type,
               const char* service_name, const uint8_t* service_uuid,
               int channel, int* sock_fd, int flags);

bt_status_t
bt_sock_connect(const bt_bdaddr_t* bd_addr, btsock_type_t type,
                const uint8_t* uuid, int channel, int* sock_fd, int flags);

int
init_bt_sock(void);

void
uninit_bt_sock(void);
