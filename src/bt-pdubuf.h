/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <sys/queue.h>
#include <sys/socket.h>
#include "bt-proto.h"

struct pdu_rbuf {
  unsigned long maxlen;
  unsigned long len;
  union {
    struct pdu pdu;
    unsigned char raw[0];
  } buf;
};

struct pdu_rbuf*
create_pdu_rbuf(unsigned long maxdatalen);

void
cleanup_pdu_rbuf(struct pdu_rbuf* rbuf);

int
pdu_rbuf_has_pdu_hdr(const struct pdu_rbuf* rbuf);

int
pdu_rbuf_has_pdu(const struct pdu_rbuf* rbuf);

int
pdu_rbuf_is_full(const struct pdu_rbuf* rbuf);

struct pdu_wbuf {
  STAILQ_ENTRY(pdu_wbuf) stailq;
  struct msghdr msg;
  unsigned long tailoff;
  unsigned long off;
  union {
    struct pdu pdu;
    unsigned char raw[0];
  } buf;
  unsigned char tail[0];
};

struct pdu_wbuf*
create_pdu_wbuf(unsigned long maxdatalen, unsigned long taillen);

void
cleanup_pdu_wbuf(struct pdu_wbuf* wbuf);

int
pdu_wbuf_consumed(const struct pdu_wbuf* wbuf);

void*
pdu_wbuf_tail(struct pdu_wbuf* wbuf);
