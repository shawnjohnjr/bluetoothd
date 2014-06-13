/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <stdint.h>
#include <hardware/bluetooth.h>

enum {
  SERVICE_CORE = 0x00,
  SERVICE_BT_CORE = 0x01,
  SERVICE_BT_SOCK = 0x02
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
read_bt_property_t(const struct pdu* pdu, unsigned long off,
                   bt_property_t* property);

long
read_bt_bdaddr_t(const struct pdu* pdu, unsigned long off, bt_bdaddr_t* addr);

long
read_bt_uuid_t(const struct pdu* pdu, unsigned long off, bt_uuid_t* uuid);

long
read_bt_pin_code_t(const struct pdu* pdu, unsigned long off,
                   bt_pin_code_t* pin_code);

long
write_pdu_at(struct pdu* pdu, unsigned long off, const char* fmt, ...);

long
append_bt_property_t(struct pdu* pdu, const bt_property_t* property);

long
append_to_pdu(struct pdu* pdu, const char* fmt, ...);

long
append_bt_property_t(struct pdu* pdu, const bt_property_t* property);

long
append_bt_bdaddr_t(struct pdu* pdu, const bt_bdaddr_t* addr);

long
append_bt_bdname_t(struct pdu* pdu, const bt_bdname_t* name);
