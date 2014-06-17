/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include "log.h"
#include "task.h"
#include "bt-proto.h"
#include "bt-pdubuf.h"
#include "bt-core.h"
#include "bt-core-io.h"

enum {
  /* commands/responses */
  OPCODE_ENABLE = 0x01,
  OPCODE_DISABLE = 0x02,
  OPCODE_GET_ADAPTER_PROPERTIES = 0x03,
  OPCODE_GET_ADAPTER_PROPERTY = 0x04,
  OPCODE_SET_ADAPTER_PROPERTY = 0x05,
  OPCODE_GET_REMOTE_DEVICE_PROPERTIES = 0x06,
  OPCODE_GET_REMOTE_DEVICE_PROPERTY = 0x07,
  OPCODE_SET_REMOTE_DEVICE_PROPERTY = 0x08,
  OPCODE_GET_REMOTE_SERVICE_RECORD = 0x09,
  OPCODE_GET_REMOTE_SERVICES = 0x0a,
  OPCODE_START_DISCOVERY = 0x0b,
  OPCODE_CANCEL_DISCOVERY = 0x0c,
  OPCODE_CREATE_BOND = 0x0d,
  OPCODE_REMOVE_BOND = 0x0e,
  OPCODE_CANCEL_BOND = 0x0f,
  OPCODE_PIN_REPLY = 0x10,
  OPCODE_SSP_REPLY = 0x11,
  OPCODE_DUT_MODE_CONFIGURE = 0x12,
  OPCODE_DUT_MODE_SEND = 0x13,
  OPCODE_LE_TEST_MODE = 0x14,
  /* notifications */
  OPCODE_ADAPTER_STATE_CHANGED_NTF = 0x81,
  OPCODE_ADAPTER_PROPERTIES_CHANGED_NTF = 0x82,
  OPCODE_REMOTE_DEVICE_PROPERTIES_NTF = 0x83,
  OPCODE_DEVICE_FOUND_NTF = 0x84,
  OPCODE_DISCOVERY_STATE_CHANGED_NTF = 0x85,
  OPCODE_PIN_REQUEST_NTF = 0x86,
  OPCODE_SSP_REQUEST_NTF = 0x87,
  OPCODE_BOND_STATE_CHANGED_NTF = 0x88,
  OPCODE_ACL_STATE_CHANGED_NTF = 0x89,
  OPCODE_DUT_MODE_RECEIVE_NTF = 0x8a,
  OPCODE_LE_TEST_MODE_NTF = 0x8b
};

static void (*send_pdu)(struct pdu_wbuf* wbuf);

static int
send_ntf_pdu(void* data)
{
  /* send notification on I/O thread */
  if (!send_pdu) {
    ALOGE("send_pdu is NULL");
    return 0;
  }
  send_pdu(data);
  return 0;
}

static unsigned long
properties_length(int num_properties, const bt_property_t* properties)
{
  unsigned long len;
  int i;

  for (len = 3 * num_properties, i = 0; i < num_properties; ++i) {
    len += properties[i].len;
  }
  return len;
}

static struct pdu_wbuf*
build_pdu_wbuf_msg(struct pdu_wbuf* wbuf)
{
  struct iovec* iov;

  assert(wbuf);

  iov = pdu_wbuf_tail(wbuf);
  iov->iov_base = wbuf->buf.raw;
  iov->iov_len = wbuf->off;

  memset(&wbuf->msg, 0, sizeof(wbuf->msg));
  wbuf->msg.msg_iov = iov;
  wbuf->msg.msg_iovlen = 1;

  return wbuf;
}

/*
 * Notifications
 */

static void
adapter_state_changed_cb(bt_state_t state)
{
  struct pdu_wbuf* wbuf;

  wbuf = create_pdu_wbuf(1, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE, OPCODE_ADAPTER_STATE_CHANGED_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)state) < 0)
    goto cleanup;

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

static void
adapter_properties_cb(bt_status_t status, int num_properties,
                      bt_property_t* properties)
{
  struct pdu_wbuf* wbuf;
  int i;

  wbuf = create_pdu_wbuf(2 + properties_length(num_properties, properties),
                         sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE,
           OPCODE_ADAPTER_PROPERTIES_CHANGED_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "CC",
                    (uint8_t)status, (uint8_t)num_properties) < 0)
    goto cleanup;

  for (i = 0; i < num_properties; ++i) {
    if (append_bt_property_t(&wbuf->buf.pdu, properties+i) < 0)
      goto cleanup;
  }

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

static void
remote_device_properties_cb(bt_status_t status,
                            bt_bdaddr_t* bd_addr,
                            int num_properties,
                            bt_property_t* properties)
{
  struct pdu_wbuf* wbuf;
  long off;
  int i;

  wbuf = create_pdu_wbuf(8 + properties_length(num_properties, properties),
                         sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE,
           OPCODE_REMOTE_DEVICE_PROPERTIES_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)status) < 0)
    goto cleanup;
  if (append_bt_bdaddr_t(&wbuf->buf.pdu, bd_addr) < 0)
    goto cleanup;
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)num_properties) < 0)
    goto cleanup;

  for (i = 0; i < num_properties; ++i) {
    if (append_bt_property_t(&wbuf->buf.pdu, properties+i) < 0)
      goto cleanup;
  }

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

static void
device_found_cb(int num_properties, bt_property_t* properties)
{
  struct pdu_wbuf* wbuf;
  long off;
  int i;

  wbuf = create_pdu_wbuf(1 + properties_length(num_properties, properties),
                         sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE,
           OPCODE_REMOTE_DEVICE_PROPERTIES_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)num_properties) < 0)
    goto cleanup;

  for (i = 0; i < num_properties; ++i) {
    if (append_bt_property_t(&wbuf->buf.pdu, properties+i) < 0)
      goto cleanup;
  }

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

static void
discovery_state_changed_cb(bt_discovery_state_t state)
{
  struct pdu_wbuf* wbuf;

  wbuf = create_pdu_wbuf(1, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE,
           OPCODE_DISCOVERY_STATE_CHANGED_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)state) < 0)
    goto cleanup;

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

static void
pin_request_cb(bt_bdaddr_t* remote_bd_addr, bt_bdname_t* bd_name,
               uint32_t cod)
{
  struct pdu_wbuf* wbuf;
  long off;

  wbuf = create_pdu_wbuf(259, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE, OPCODE_PIN_REQUEST_NTF);
  if (append_bt_bdaddr_t(&wbuf->buf.pdu, remote_bd_addr) < 0)
    goto cleanup;
  if (append_bt_bdname_t(&wbuf->buf.pdu, bd_name) < 0)
    goto cleanup;
  if (append_to_pdu(&wbuf->buf.pdu, "I", cod) < 0)
    goto cleanup;

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

static void
ssp_request_cb(bt_bdaddr_t* remote_bd_addr, bt_bdname_t* bd_name,
               uint32_t cod, bt_ssp_variant_t pairing_variant,
               uint32_t pass_key)
{
  struct pdu_wbuf* wbuf;
  long off;

  wbuf = create_pdu_wbuf(264, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE, OPCODE_SSP_REQUEST_NTF);
  if (append_bt_bdaddr_t(&wbuf->buf.pdu, remote_bd_addr) < 0)
    goto cleanup;
  if (append_bt_bdname_t(&wbuf->buf.pdu, bd_name) < 0)
    goto cleanup;
  if (append_to_pdu(&wbuf->buf.pdu, "ICI", cod,
                    (uint8_t)pairing_variant, pass_key) < 0)
    goto cleanup;

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

static void
bond_state_changed_cb(bt_status_t status, bt_bdaddr_t* remote_bd_addr,
                      bt_bond_state_t state)
{
  struct pdu_wbuf* wbuf;
  long off;

  wbuf = create_pdu_wbuf(8, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE, OPCODE_BOND_STATE_CHANGED_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)status) < 0)
    goto cleanup;
  if (append_bt_bdaddr_t(&wbuf->buf.pdu, remote_bd_addr) < 0)
    goto cleanup;
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)state) < 0)
    goto cleanup;

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(build_pdu_wbuf_msg(wbuf));
}

static void
acl_state_changed_cb(bt_status_t status, bt_bdaddr_t* remote_bd_addr,
                     bt_acl_state_t state)
{
  struct pdu_wbuf* wbuf;
  long off;

  wbuf = create_pdu_wbuf(8, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE, OPCODE_ACL_STATE_CHANGED_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)status) < 0)
    goto cleanup;
  if (append_bt_bdaddr_t(&wbuf->buf.pdu, remote_bd_addr) < 0)
    goto cleanup;
  if (append_to_pdu(&wbuf->buf.pdu, "C", (uint8_t)state) < 0)
    goto cleanup;

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

static void
thread_evt_cb(bt_cb_thread_evt evt)
{
  /* nothing to do */
}

static void
dut_mode_recv_cb(uint16_t opcode, uint8_t* buf, uint8_t len)
{
  struct pdu_wbuf* wbuf;
  long off;

  wbuf = create_pdu_wbuf(3 + len, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE, OPCODE_DUT_MODE_RECEIVE_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "SCm", opcode, len, buf, (size_t)len) < 0)
    goto cleanup;

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}

#if ANDROID_VERSION >= 18
static void
le_test_mode_cb(bt_status_t status, uint16_t num_packets)
{
  struct pdu_wbuf* wbuf;
  long off;

  wbuf = create_pdu_wbuf(3, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return;

  init_pdu(&wbuf->buf.pdu, SERVICE_BT_CORE, OPCODE_LE_TEST_MODE_NTF);
  if (append_to_pdu(&wbuf->buf.pdu, "CS",
                    (uint8_t)status, (uint16_t)num_packets) < 0)
    goto cleanup;

  if (run_task(send_ntf_pdu, build_pdu_wbuf_msg(wbuf)) < 0)
    goto cleanup;

  return;
cleanup:
  cleanup_pdu_wbuf(wbuf);
}
#endif

static const bt_callbacks_t bt_callbacks = {
  .size = sizeof(bt_callbacks),
  .adapter_state_changed_cb = adapter_state_changed_cb,
  .adapter_properties_cb = adapter_properties_cb,
  .remote_device_properties_cb = remote_device_properties_cb,
  .device_found_cb = device_found_cb,
  .discovery_state_changed_cb = discovery_state_changed_cb,
  .pin_request_cb = pin_request_cb,
  .ssp_request_cb = ssp_request_cb,
  .bond_state_changed_cb = bond_state_changed_cb,
  .acl_state_changed_cb = acl_state_changed_cb,
  .thread_evt_cb = thread_evt_cb,
  .dut_mode_recv_cb = dut_mode_recv_cb,
#if ANDROID_VERSION >= 18
  .le_test_mode_cb = le_test_mode_cb
#endif
};

/*
 * Commands/Responses
 */

static bt_status_t
enable(const struct pdu* cmd)
{
  struct pdu_wbuf* wbuf;
  int status;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_enable();
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_enable;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_enable:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
disable(const struct pdu* cmd)
{
  struct pdu_wbuf* wbuf;
  int status;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_disable();
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_disable;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_disable:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
get_adapter_properties(const struct pdu* cmd)
{
  struct pdu_wbuf* wbuf;
  int status;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_get_adapter_properties();
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_get_adapter_properties;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_get_adapter_properties:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
get_adapter_property(const struct pdu* cmd)
{
  uint8_t type;
  struct pdu_wbuf* wbuf;
  int status;

  if (read_pdu_at(cmd, 0, "C", &type) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_get_adapter_property(type);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_get_adapter_property;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_get_adapter_property:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
set_adapter_property(const struct pdu* cmd)
{
  bt_property_t property;
  int status;
  struct pdu_wbuf* wbuf;

  if (read_bt_property_t(cmd, 0, &property) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf) {
    status = BT_STATUS_NOMEM;
    goto err_create_pdu_wbuf;
  }

  status = bt_core_set_adapter_property(&property);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_get_adapter_properties;

  free(property.val);

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_get_adapter_properties:
  cleanup_pdu_wbuf(wbuf);
err_create_pdu_wbuf:
  free(property.val);
  return status;
}

static bt_status_t
get_remote_device_properties(const struct pdu* cmd)
{
  bt_bdaddr_t remote_addr;
  struct pdu_wbuf* wbuf;
  int status;

  if (read_bt_bdaddr_t(cmd, 0, &remote_addr) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_get_remote_device_properties(&remote_addr);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_get_remote_device_properties;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_get_remote_device_properties:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
get_remote_device_property(const struct pdu* cmd)
{
  long off;
  bt_bdaddr_t remote_addr;
  uint8_t type;
  struct pdu_wbuf* wbuf;
  int status;

  off = read_bt_bdaddr_t(cmd, 0, &remote_addr);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  if (read_pdu_at(cmd, off, "C", &type) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_get_remote_device_property(&remote_addr, type);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_get_remote_device_property;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_get_remote_device_property:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
set_remote_device_property(const struct pdu* cmd)
{
  long off;
  bt_bdaddr_t remote_addr;
  bt_property_t property;
  struct pdu_wbuf* wbuf;
  int status;

  off = read_bt_bdaddr_t(cmd, 0, &remote_addr);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  if (read_bt_property_t(cmd, off, &property) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf) {
    status = BT_STATUS_NOMEM;
    goto err_create_pdu_wbuf;
  }

  status = bt_core_set_remote_device_property(&remote_addr, &property);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_set_remote_device_property;

  free(property.val);

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_set_remote_device_property:
  cleanup_pdu_wbuf(wbuf);
err_create_pdu_wbuf:
  free(property.val);
  return status;
}

static bt_status_t
get_remote_service_record(const struct pdu* cmd)
{
  long off;
  bt_bdaddr_t remote_addr;
  bt_uuid_t uuid;
  struct pdu_wbuf* wbuf;
  int status;

  off = read_bt_bdaddr_t(cmd, 0, &remote_addr);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  if (read_bt_uuid_t(cmd, off, &uuid) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_get_remote_service_record(&remote_addr, &uuid);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_get_remote_service_record;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_get_remote_service_record:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
get_remote_services(const struct pdu* cmd)
{
  bt_bdaddr_t remote_addr;
  struct pdu_wbuf* wbuf;
  int status;

  if (read_bt_bdaddr_t(cmd, 0, &remote_addr) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_get_remote_services(&remote_addr);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_get_remote_services;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_get_remote_services:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
start_discovery(const struct pdu* cmd)
{
  struct pdu_wbuf* wbuf;
  int status;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_start_discovery();
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_start_discovery;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_start_discovery:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
cancel_discovery(const struct pdu* cmd)
{
  struct pdu_wbuf* wbuf;
  int status;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_cancel_discovery();
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_cancel_discovery;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_cancel_discovery:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
create_bond(const struct pdu* cmd)
{
  bt_bdaddr_t bd_addr;
  struct pdu_wbuf* wbuf;
  int status;

  if (read_bt_bdaddr_t(cmd, 0, &bd_addr) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_create_bond(&bd_addr);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_create_bond;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_create_bond:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
remove_bond(const struct pdu* cmd)
{
  bt_bdaddr_t bd_addr;
  struct pdu_wbuf* wbuf;
  int status;

  if (read_bt_bdaddr_t(cmd, 0, &bd_addr) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_remove_bond(&bd_addr);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_remove_bond;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_remove_bond:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
cancel_bond(const struct pdu* cmd)
{
  bt_bdaddr_t bd_addr;
  struct pdu_wbuf* wbuf;
  int status;

  if (read_bt_bdaddr_t(cmd, 0, &bd_addr) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_cancel_bond(&bd_addr);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_remove_bond;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_remove_bond:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
pin_reply(const struct pdu* cmd)
{
  long off;
  bt_bdaddr_t bd_addr;
  uint8_t accept;
  uint8_t pin_len;
  bt_pin_code_t pin_code;
  struct pdu_wbuf* wbuf;
  int status;

  off = read_bt_bdaddr_t(cmd, 0, &bd_addr);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  off = read_pdu_at(cmd, off, "CC", &accept, &pin_len);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  if (read_bt_pin_code_t(cmd, off, &pin_code) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_pin_reply(&bd_addr, accept, pin_len, &pin_code);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_pin_reply;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_pin_reply:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
ssp_reply(const struct pdu* cmd)
{
  long off;
  bt_bdaddr_t bd_addr;
  uint8_t variant;
  uint8_t accept;
  uint32_t passkey;
  struct pdu_wbuf* wbuf;
  int status;

  off = read_bt_bdaddr_t(cmd, 0, &bd_addr);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  if (read_pdu_at(cmd, off, "CCI", &variant, &accept, &passkey) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_ssp_reply(&bd_addr, variant, accept, passkey);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_ssp_reply;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_ssp_reply:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
dut_mode_configure(const struct pdu* cmd)
{
  uint8_t enable;
  struct pdu_wbuf* wbuf;
  int status;

  if (read_pdu_at(cmd, 0, "C", &enable) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_dut_mode_configure(enable);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_dut_mode_configure;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_dut_mode_configure:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
dut_mode_send(const struct pdu* cmd)
{
  long off;
  uint16_t opcode;
  uint8_t len;
  uint8_t buf[256];
  struct pdu_wbuf* wbuf;
  int status;

  off = read_pdu_at(cmd, 0, "SC", &opcode, &len);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  if (read_pdu_at(cmd, off, "m", buf, len) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_dut_mode_send(opcode, buf, len);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_dut_mode_send;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_dut_mode_send:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
le_test_mode(const struct pdu* cmd)
{
  long off;
  uint16_t opcode;
  uint8_t len;
  uint8_t buf[256];
  struct pdu_wbuf* wbuf;
  int status;

  off = read_pdu_at(cmd, 0, "SC", &opcode, &len);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  if (read_pdu_at(cmd, off, "m", buf, len) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_core_le_test_mode(opcode, buf, len);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_core_le_test_mode;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_bt_core_le_test_mode:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
bt_core_handler(const struct pdu* cmd)
{
  static bt_status_t (* const handler[256])(const struct pdu*) = {
    [OPCODE_ENABLE] = enable,
    [OPCODE_DISABLE] = disable,
    [OPCODE_GET_ADAPTER_PROPERTIES] = get_adapter_properties,
    [OPCODE_GET_ADAPTER_PROPERTY] = get_adapter_property,
    [OPCODE_SET_ADAPTER_PROPERTY] = set_adapter_property,
    [OPCODE_GET_REMOTE_DEVICE_PROPERTIES] = get_remote_device_properties,
    [OPCODE_GET_REMOTE_DEVICE_PROPERTY] = get_remote_device_property,
    [OPCODE_SET_REMOTE_DEVICE_PROPERTY] = set_remote_device_property,
    [OPCODE_GET_REMOTE_SERVICE_RECORD] = get_remote_service_record,
    [OPCODE_GET_REMOTE_SERVICES] = get_remote_services,
    [OPCODE_START_DISCOVERY] = start_discovery,
    [OPCODE_CANCEL_DISCOVERY] = cancel_discovery,
    [OPCODE_CREATE_BOND] = create_bond,
    [OPCODE_REMOVE_BOND] = remove_bond,
    [OPCODE_CANCEL_BOND] = cancel_bond,
    [OPCODE_PIN_REPLY] = pin_reply,
    [OPCODE_SSP_REPLY] = ssp_reply,
    [OPCODE_DUT_MODE_CONFIGURE] = dut_mode_configure,
    [OPCODE_DUT_MODE_SEND] = dut_mode_send,
    [OPCODE_LE_TEST_MODE] = le_test_mode
  };

  return handle_pdu_by_opcode(cmd, handler);
}

bt_status_t
(*register_bt_core(unsigned char mode,
                   void (*send_pdu_cb)(struct pdu_wbuf*)))(const struct pdu*)
{
  assert(send_buf);

  if (init_bt_core() < 0)
    return NULL;

  send_pdu = send_pdu_cb;

  return bt_core_handler;
}

int
unregister_bt_core()
{
  return 0;
}
