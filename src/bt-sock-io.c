/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include "bt-proto.h"
#include "bt-pdubuf.h"
#include "bt-sock.h"
#include "bt-sock-io.h"

enum {
  OPCODE_LISTEN = 0x01,
  OPCODE_CONNECT = 0x02
};

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

static struct pdu_wbuf*
build_pdu_wbuf_msg_with_fd(struct pdu_wbuf* wbuf, int fd)
{
  struct iovec* iov;
  union {
    struct cmsghdr chdr;
    unsigned char data[CMSG_SPACE(sizeof(fd))];
  } cmsg;
  struct cmsghdr* chdr;

  assert(wbuf);

  iov = pdu_wbuf_tail(wbuf);
  iov->iov_base = wbuf->buf.raw;
  iov->iov_len = wbuf->off;

  memset(&wbuf->msg, 0, sizeof(wbuf->msg));
  wbuf->msg.msg_iov = iov;
  wbuf->msg.msg_iovlen = 1;
  wbuf->msg.msg_control = iov + 1;
  wbuf->msg.msg_controllen = sizeof(cmsg);

  chdr = CMSG_FIRSTHDR(&wbuf->msg);
  chdr->cmsg_len = sizeof(fd);
  chdr->cmsg_level = SOL_SOCKET;
  chdr->cmsg_type = SCM_RIGHTS;
  *((int*)CMSG_DATA(chdr)) = fd;

  return wbuf;
}

/*
 * Commands/Responses
 */

static bt_status_t
opcode_listen(const struct pdu* cmd)
{
  uint8_t type;
  int8_t service_name[256];
  uint8_t uuid[16];
  uint16_t channel;
  uint8_t flags;
  int sock_fd;
  struct pdu_wbuf* wbuf;
  bt_status_t status;

  if (read_pdu_at(cmd, 0, "CmmSC", &type,
                                   service_name, (size_t)sizeof(service_name),
                                   uuid, (size_t)uuid, &channel, &flags) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_sock_listen(type, (char*)service_name, uuid, channel, &sock_fd, flags);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_sock_listen;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg_with_fd(wbuf, sock_fd));

  return BT_STATUS_SUCCESS;
err_bt_sock_listen:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
opcode_connect(const struct pdu* cmd)
{
  long off;
  bt_bdaddr_t bd_addr;
  uint8_t type;
  uint8_t uuid[16];
  uint16_t channel;
  uint8_t flags;
  int sock_fd;
  struct pdu_wbuf* wbuf;
  bt_status_t status;

  off = read_bt_bdaddr_t(cmd, 0, &bd_addr);
  if (off < 0)
    return BT_STATUS_PARM_INVALID;
  if (read_pdu_at(cmd, off, "CmSC", &type, uuid, (size_t)uuid,
                                   &channel, &flags) < 0)
    return BT_STATUS_PARM_INVALID;

  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return BT_STATUS_NOMEM;

  status = bt_sock_connect(&bd_addr, type, uuid, channel, &sock_fd, flags);
  if (status != BT_STATUS_SUCCESS)
    goto err_bt_sock_listen;

  init_pdu(&wbuf->buf.pdu, cmd->service, cmd->opcode);
  send_pdu(build_pdu_wbuf_msg_with_fd(wbuf, sock_fd));

  return BT_STATUS_SUCCESS;
err_bt_sock_listen:
  cleanup_pdu_wbuf(wbuf);
  return status;
}

static bt_status_t
bt_sock_handler(const struct pdu* cmd)
{
  static bt_status_t (* const handler[256])(const struct pdu*) = {
    [OPCODE_LISTEN] = opcode_listen,
    [OPCODE_CONNECT] = opcode_connect,
  };

  return handle_pdu_by_opcode(cmd, handler);
}

bt_status_t
(*register_bt_sock(unsigned char mode,
                   void (*send_pdu_cb)(struct pdu_wbuf*)))(const struct pdu*)
{
  if (init_bt_sock() < 0)
    return NULL;

  send_pdu = send_pdu_cb;

  return bt_sock_handler;
}

int
unregister_bt_sock()
{
  uninit_bt_sock();
  return 0;
}
