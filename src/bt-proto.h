/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <stdint.h>

enum {
  SERVICE_CORE = 0x00
};

struct pdu {
  uint8_t service;
  uint8_t opcode;
  uint16_t len;
  unsigned char data[];
} __attribute__((packed));

void
init_pdu(struct pdu* pdu, uint8_t service, uint8_t opcode);

size_t
pdu_size(const struct pdu* pdu);

bt_status_t
handle_pdu_by_service(const struct pdu* cmd,
                      bt_status_t (* const handler[256])(const struct pdu*));

bt_status_t
handle_pdu_by_opcode(const struct pdu* cmd,
                     bt_status_t (* const handler[256])(const struct pdu*));

long
read_pdu_at(const struct pdu* pdu, unsigned long off, const char* fmt, ...);

long
write_pdu_at(struct pdu* pdu, unsigned long off, const char* fmt, ...);

long
append_to_pdu(struct pdu* pdu, const char* fmt, ...);
