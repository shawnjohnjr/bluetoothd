/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <string.h>
#include "bt-proto.h"
#include "bt-pdubuf.h"
#include "core.h"
#include "core-io.h"

static void (*send_pdu)(struct pdu_wbuf* wbuf);

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

enum {
  OPCODE_REGISTER_MODULE = 0x01,
  OPCODE_UNREGISTER_MODULE = 0x02
};

static bt_status_t
register_module(const struct pdu* cmd)
{
  uint8_t service;
  uint8_t mode;
  struct pdu_wbuf* wbuf;

  if (read_pdu_at(cmd, 0, "CC", &service, &mode) < 0)
    return BT_STATUS_FAIL;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_FAIL;

  if (core_register_module(service, mode) < 0)
    goto err_core_register_module;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_core_register_module:
  cleanup_pdu_wbuf(wbuf);
  return BT_STATUS_FAIL;
}

static bt_status_t
unregister_module(const struct pdu* cmd)
{
  uint8_t service;
  struct pdu_wbuf* wbuf;

  if (read_pdu_at(cmd, 0, "C", &service) < 0)
    return BT_STATUS_FAIL;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_FAIL;

  if (core_unregister_module(service) < 0)
    goto err_core_unregister_module;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg(wbuf));

  return BT_STATUS_SUCCESS;
err_core_unregister_module:
  cleanup_pdu_wbuf(wbuf);
  return BT_STATUS_FAIL;
}

static bt_status_t
core_handler(const struct pdu* cmd)
{
  static bt_status_t (* const handler[256])(const struct pdu*) = {
    [OPCODE_REGISTER_MODULE] = register_module,
    [OPCODE_UNREGISTER_MODULE] = unregister_module
  };

  return handle_pdu_by_opcode(cmd, handler);
}

int
init_core_io(void (*send_pdu_cb)(struct pdu_wbuf*))
{
  assert(send_pdu_cb);

  if (init_core(core_handler, send_pdu_cb) < 0)
    return -1;

  send_pdu = send_pdu_cb;

  return 0;
}

void
uninit_core_io()
{
  send_pdu = NULL;
}
