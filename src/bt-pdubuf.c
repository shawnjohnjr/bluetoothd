/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdlib.h>
#include "log.h"
#include "bt-pdubuf.h"

struct pdu_rbuf*
create_pdu_rbuf(unsigned long maxdatalen)
{
  struct pdu_rbuf* rbuf;

  errno = 0;
  rbuf = malloc(sizeof(*rbuf) + maxdatalen);
  if (errno) {
    ALOGE_ERRNO("malloc");
    goto err_malloc;
  }

  rbuf->maxlen = sizeof(rbuf->buf) + maxdatalen;
  rbuf->len = 0;

  return rbuf;
err_malloc:
  return NULL;
}

void
cleanup_pdu_rbuf(struct pdu_rbuf* rbuf)
{
  assert(buf);
  free(rbuf);
}

int
pdu_rbuf_has_pdu_hdr(const struct pdu_rbuf* rbuf)
{
  assert(rbuf);
  return rbuf->len >= sizeof(rbuf->buf.pdu);
}

int
pdu_rbuf_has_pdu(const struct pdu_rbuf* rbuf)
{
  assert(rbuf);
  return pdu_rbuf_has_pdu_hdr(rbuf) && (rbuf->len == pdu_size(&rbuf->buf.pdu));
}

int
pdu_rbuf_is_full(const struct pdu_rbuf* rbuf)
{
  assert(rbuf);
  return rbuf->len == rbuf->maxlen;
}

struct pdu_wbuf*
create_pdu_wbuf(unsigned long maxdatalen, unsigned long taillen)
{
  struct pdu_wbuf* wbuf;

  errno = 0;
  wbuf = malloc(sizeof(*wbuf) + maxdatalen + taillen);
  if (errno) {
    ALOGE_ERRNO("malloc");
    goto err_malloc;
  }

  wbuf->stailq.stqe_next = NULL;
  wbuf->tailoff = sizeof(*wbuf) + maxdatalen;
  wbuf->off = 0;

  return wbuf;
err_malloc:
  return NULL;
}

void
cleanup_pdu_wbuf(struct pdu_wbuf* wbuf)
{
  assert(buf);
  free(wbuf);
}

int
pdu_wbuf_consumed(const struct pdu_wbuf* wbuf)
{
  assert(wbuf);
  return wbuf->off == pdu_size(&wbuf->buf.pdu);
}

void*
pdu_wbuf_tail(struct pdu_wbuf* wbuf)
{
  assert(wbuf);
  return wbuf->tail + wbuf->tailoff;
}
