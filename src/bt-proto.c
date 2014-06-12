/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdarg.h>
#include "log.h"
#include "bt-proto.h"

void
init_pdu(struct pdu* pdu, uint8_t service, uint8_t opcode)
{
  assert(pdu);

  pdu->service = service;
  pdu->opcode = opcode;
  pdu->len = 0;
}

size_t
pdu_size(const struct pdu* pdu)
{
  return sizeof(*pdu) + pdu->len;
}

static bt_status_t
handle_pdu(const char* field, uint8_t value, const struct pdu* cmd,
           bt_status_t (* const handler[256])(const struct pdu*))
{
  assert(field);
  assert(pdu);
  assert(handler);

  if (!handler[value]) {
    ALOGE("unsupported %s 0x%x", field, value);
    return BT_STATUS_UNSUPPORTED;
  }
  return handler[value](cmd);
err_handler:
err_not_handler:
  return -1;
}

bt_status_t
handle_pdu_by_service(const struct pdu* cmd,
                      bt_status_t (* const handler[256])(const struct pdu*))
{
  return handle_pdu("service", cmd->service, cmd, handler);
}

bt_status_t
handle_pdu_by_opcode(const struct pdu* cmd,
                     bt_status_t (* const handler[256])(const struct pdu*))
{
  return handle_pdu("opcode", cmd->opcode, cmd, handler);
}

static long
read_pdu_at_va(const struct pdu* pdu, unsigned long off,
               const char* fmt, va_list ap)
{
  void* dst;
  size_t len;

  for (; *fmt; ++fmt) {
    switch (*fmt) {
      case 'c': /* signed 8 bit */
        dst = va_arg(ap, int8_t*);
        len = 1;
        break;
      case 'C': /* unsigned 8 bit*/
        dst = va_arg(ap, uint8_t*);
        len = 1;
        break;
      case 's': /* signed 16 bit */
        dst = va_arg(ap, int16_t*);
        len = 2;
        break;
      case 'S': /* unsigned 16 bit */
        dst = va_arg(ap, uint16_t*);
        len = 2;
        break;
      case 'i': /* signed 32 bit */
        dst = va_arg(ap, int32_t*);
        len = 4;
        break;
      case 'I': /* unsigned 32 bit */
        dst = va_arg(ap, uint32_t*);
        len = 4;
        break;
      case 'l': /* signed 64 bit */
        dst = va_arg(ap, int64_t*);
        len = 8;
        break;
      case 'L': /* unsigned 64 bit */
        dst = va_arg(ap, uint64_t*);
        len = 8;
        break;
      case 'm': /* raw memory + length */
        dst = va_arg(ap, void*);
        len = va_arg(ap, size_t);
        break;
      default:
        ALOGE("invalid format character %c", *fmt);
        return -1;
    }
    if (off+len > pdu->len) {
      ALOGE("PDU overflow");
      return -1;
    }
    memcpy(dst, pdu->data+off, len);
    off += len;
  }

  return off;
}

long
read_pdu_at(const struct pdu* pdu, unsigned long off, const char* fmt, ...)
{
  va_list ap;
  long res;

  va_start(ap, fmt);
  res = read_pdu_at_va(pdu, off, fmt, ap);
  va_end(ap);

  return res;
}

static long
write_pdu_at_va(struct pdu* pdu, unsigned long off, const char* fmt, va_list ap)
{
  int8_t c;
  uint8_t C;
  int16_t s;
  uint16_t S;
  int32_t i;
  uint32_t I;
  int64_t l;
  uint64_t L;
  const void* src;
  size_t len;

  for (; *fmt; ++fmt) {
    switch (*fmt) {
      case 'c': /* signed 8 bit */
        c = va_arg(ap, int8_t);
        src = &c;
        len = 1;
        break;
      case 'C': /* unsigned 8 bit*/
        C = va_arg(ap, uint8_t);
        src = &C;
        len = 1;
        break;
      case 's': /* signed 16 bit */
        s = va_arg(ap, int16_t);
        src = &s;
        len = 2;
        break;
      case 'S': /* unsigned 16 bit */
        S = va_arg(ap, uint16_t);
        src = &S;
        len = 2;
        break;
      case 'i': /* signed 32 bit */
        i = va_arg(ap, int32_t);
        src = &i;
        len = 4;
        break;
      case 'I': /* unsigned 32 bit */
        I = va_arg(ap, uint32_t);
        src = &I;
        len = 4;
        break;
      case 'l': /* signed 64 bit */
        l = va_arg(ap, int64_t);
        src = &l;
        len = 8;
        break;
      case 'L': /* unsigned 64 bit */
        L = va_arg(ap, uint64_t);
        src = &L;
        len = 8;
        break;
      case 'm': /* raw memory + length */
        src = va_arg(ap, void*);
        len = va_arg(ap, size_t);
        break;
      default:
        ALOGE("invalid format character %c", *fmt);
        return -1;
    }
    if (off+len > pdu->len) {
      ALOGE("PDU overflow");
      return -1;
    }
    memcpy(pdu->data+off, src, len);
    off += len;
  }

  return off;
}

long
write_pdu_at(struct pdu* pdu, unsigned long off, const char* fmt, ...)
{
  va_list ap;
  long res;

  va_start(ap, fmt);
  res = write_pdu_at(pdu, off, fmt, ap);
  va_end(ap);

  return res;
}

long
append_to_pdu(struct pdu* pdu, const char* fmt, ...)
{
  va_list ap;
  long res;

  va_start(ap, fmt);
  res = write_pdu_at(pdu, pdu->len, fmt, ap);
  va_end(ap);

  if (res > 0)
    pdu->len = res;

  return res;
}
