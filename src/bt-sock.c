/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include "log.h"
#include "bt-core.h"
#include "bt-sock.h"

static const btsock_interface_t* btsock_interface;

bt_status_t
bt_sock_listen(btsock_type_t type,
               const char* service_name, const uint8_t* service_uuid,
               int channel, int* sock_fd, int flags)
{
  assert(btsock_interface);
  assert(btsock_interface->listen);

  return btsock_interface->listen(type, service_name, service_uuid,
                                  channel, sock_fd, flags);
}

bt_status_t
bt_sock_connect(const bt_bdaddr_t* bd_addr, btsock_type_t type,
                const uint8_t* uuid, int channel, int* sock_fd, int flags)
{
  assert(btsock_interface);
  assert(btsock_interface->connect);

  return btsock_interface->connect(bd_addr, type, uuid,
                                   channel, sock_fd, flags);
}

int
init_bt_sock()
{
  if (btsock_interface) {
    ALOGE("Socket interface already set up");
    return -1;
  }

  btsock_interface = bt_core_get_profile_interface(BT_PROFILE_SOCKETS_ID);

  return 0;
}

void
uninit_bt_sock()
{
  assert(btsock_interface);

  btsock_interface = NULL;
}
