/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cutils/sockets.h>
#include "log.h"
#include "loop.h"
#include "bt-proto.h"
#include "bt-pdubuf.h"
#include "service.h"
#include "core.h"
#include "core-io.h"
#include "bt-io.h"

#define BLUETOOTHD_SOCKET "bluetoothd"

/*
 * Socket I/O
 */

static int io_fd[2];

static void
io_fd0_event_err(int fd, void* data)
{
  cleanup_pdu_rbuf(data);
  remove_fd_from_epoll_loop(fd);
  io_fd[0] = 0;
  uninit_core_io();
}

#if 0
static int
send_pdu(int fd, struct pdu_wbuf* wbuf, int* sock_fd)
{
  struct iovec iov;
  struct msghdr msg;
  union {
    struct cmsghdr cmsghdr;
    unsigned char data[CMSG_DATA(sizeof(sockfd))];
  } cmsg;
  struct cmsghdr* cmsghdr;

  assert(wbuf);

  iov.iov_base = wbuf->buf.raw;
  iov.iov_len = wbuf->off;

  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  if (sock_fd) {
    msg.control = cmsg.data;
    msg.controllen = sizeof(cmsg.data);
    cmsghdr = CMSG_FIRSTHDR(&msg);
    cmsghdr->cmsg_len = CMSG_LEN(sizeof(fd));
    cmsghdr->cmsg_level = SOL_SOCKET;
    cmsghdr->cmsg_type = SCM_RIGHTS;
    *((int*)CMSG_DATA(cmsghdr)) = *sock_fd;
  }

  if (TEMP_FAILURE_RETRY(sendmsg(fd, &msg, 0)) < 0) {
    ALOGE_ERRNO("sendmsg");
    return -1;
  }

  /* TODO: append to send queue 1 */
  /* TODO: epoll fd for writing */

  return 0;
}

static int
send_rsp_pdu(struct pdu_wbuf* wbuf, int* sock_fd)
{
  /* TODO: append to send queue 0 */
  /* TODO: epoll fd for writing */

  return 0;
}

static int
send_ntf_pdu(struct pdu_wbuf* wbuf)
{
  /* TODO: append to send queue 1 */
  /* TODO: epoll fd for writing */

  return 0;
}
#else
static void
send_pdu(struct pdu_wbuf* wbuf)
{
  /* TODO: append to send queue 1 */
  /* TODO: epoll fd for writing */
}
#endif

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

static int
handle_pdu(const struct pdu* cmd)
{
  bt_status_t status;
  struct pdu_wbuf* wbuf;

  status = handle_pdu_by_service(cmd, service_handler);
  if (status != BT_STATUS_SUCCESS)
    goto err_handle_pdu_by_service;

  return 0;
err_handle_pdu_by_service:
  /* reply with an error */
  wbuf = create_pdu_wbuf(0, sizeof(*wbuf->msg.msg_iov));
  if (!wbuf)
    return -1;
  init_pdu(&wbuf->buf.pdu, cmd->service, 0);
  append_to_pdu(&wbuf->buf.pdu, 0, "C", (uint8_t)status);
  send_pdu(build_pdu_wbuf_msg(wbuf));
  return -1;
}

static void
io_fd0_event_in(int fd, void* data)
{
  struct pdu_rbuf* rbuf;
  ssize_t res;
  size_t len;
  static const size_t pdusize = sizeof(rbuf->buf.pdu);

  assert(data);
  rbuf = data;

  if (rbuf->len < pdusize) {
    /* read PDU header */
    len = pdusize-rbuf->len;
  } else {
    /* read PDU data */
    len = rbuf->len-pdusize-rbuf->len;
  }

  res = TEMP_FAILURE_RETRY(read(fd, rbuf->buf.raw+rbuf->len, len));
  if (res < 0) {
    ALOGE_ERRNO("read");
    goto err_read;
  }

  rbuf->len += res;

  if (pdu_rbuf_has_pdu(rbuf)) {
    if (handle_pdu(&rbuf->buf.pdu) < 0)
      goto err_pdu;
    rbuf->len = 0;
  } else if (pdu_rbuf_is_full(rbuf)) {
    ALOGE("buffer too small for PDU(0x%x:0x%x)",
          rbuf->buf.pdu.service, rbuf->buf.pdu.opcode);
    goto err_pdu;
  }

  return;
err_pdu:
err_read:
  if (TEMP_FAILURE_RETRY(close(fd)) < 0)
    ALOGW_ERRNO("close");
  return;
}

static void
io_fd0_event(int fd, uint32_t events, void* data)
{
  if (events & EPOLLERR) {
    io_fd0_event_err(fd, data);
  } else if (events & EPOLLIN) {
    io_fd0_event_in(fd, data);
  } else {
    ALOGW("unsupported event mask: %u", events);
  }
}

/*
 * Listening socket I/O
 */

static void
fd_event_err(int fd, void* data)
{
  remove_fd_from_epoll_loop(fd);
}

static int
setup_cmd_socket(int fd)
{
  struct pdu_rbuf* rbuf;

  rbuf = create_pdu_rbuf(1024);
  if (!rbuf)
    goto err_create_pdu_rbuf;

  if (add_fd_to_epoll_loop(fd, EPOLLERR|EPOLLIN, io_fd0_event, rbuf) < 0)
    goto err_add_fd_to_epoll_loop;

  if (init_core_io(send_pdu) < 0)
    goto err_init_core_io;

  io_fd[0] = fd;

  return 0;
err_init_core_io:
err_add_fd_to_epoll_loop:
  cleanup_pdu_rbuf(rbuf);
err_create_pdu_rbuf:
  return -1;
}

static int
setup_ntf_socket(int fd)
{
  io_fd[1] = fd;
  return 0;
}

static void
fd_event_in(int fd, void* data)
{
  int socket_fd, res;

  socket_fd = TEMP_FAILURE_RETRY(accept(fd, NULL, 0));
  if (socket_fd < 0) {
    ALOGE_ERRNO("accept");
    goto err_accept;
  }

  /* The client shall connect two sockets: the first is for
   * transmitting pairs of command/response PDUs, the second
   * is for notifications.
   */
  if (!io_fd[0]) {
    res = setup_cmd_socket(socket_fd);
  } else if (!io_fd[1]) {
    res = setup_ntf_socket(socket_fd);
  } else {
    ALOGE("too many connected sockets");
    goto err_io_fd;
  }
  if (res < 0)
    goto err_io_fd;

  return;
err_io_fd:
  if (TEMP_FAILURE_RETRY(close(socket_fd)) < 0)
    ALOGW_ERRNO("close");
err_accept:
  return;
}

static void
fd_event(int fd, uint32_t events, void* data)
{
  if (events & EPOLLERR) {
    fd_event_err(fd, data);
  } else if (events & EPOLLIN) {
    fd_event_in(fd, data);
  } else {
    ALOGW("unsupported event mask: %u", events);
  }
}

int
init_bt_io()
{
  int fd;

  fd = android_get_control_socket(BLUETOOTHD_SOCKET);
  if (fd < 0) {
    ALOGE_ERRNO("android_get_control_socket");
    goto err_android_get_control_socket;
  }

  if (listen(fd, 16) < 0) {
    ALOGE_ERRNO("listen");
    goto err_listen;
  }

  if (add_fd_to_epoll_loop(fd, EPOLLIN|EPOLLERR, fd_event, NULL) < 0)
    goto err_add_fd_to_epoll_loop;

  return 0;
err_add_fd_to_epoll_loop:
err_listen:
  if (TEMP_FAILURE_RETRY(close(fd)) < 0)
    ALOGW_ERRNO("close");
err_android_get_control_socket:
  return -1;
}
