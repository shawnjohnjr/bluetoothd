/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "log.h"
#include "bt-core.h"

static bluetooth_device_t*   bt_device;
static const bt_interface_t* bt_interface;

#define container(_t, _v, _m) \
  ( (_t*)( ((const unsigned char*)(_v)) - offsetof(_t, _m) ) )

int
init_bt_core()
{
  int err;
  hw_module_t* module;
  hw_device_t* device;

  if (bt_device) {
    ALOGE("Bluetooth device already open");
    goto err_bt_device;
  }
  if (bt_interface) {
    ALOGE("Bluetooth interface already set up");
    goto err_bt_interface;
  }

  err = hw_get_module(BT_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
  if (err) {
    ALOGE("hw_get_module failed: %s", strerror(err));
    goto err_hw_get_module;
  }

  err = module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
  if (err) {
    ALOGE("open failed: %s", strerror(err));
    goto err_open;
  }

  bt_device = container(bluetooth_device_t, device, common);

  bt_interface = bt_device->get_bluetooth_interface();
  if (!bt_interface) {
    ALOGE("get_bluetooth_interface failed");
    goto err_get_bluetooth_interface;
  }

  return 0;
err_get_bluetooth_interface:
  err = device->close(device);
  if (err)
    ALOGW("get_bluetooth_interface failed: %s", strerror(err));
  bt_device = NULL;
err_open:
err_hw_get_module:
err_bt_interface:
err_bt_device:
  return -1;
}

void
uninit_bt_core()
{
  int err;

  assert(bt_device);

  bt_interface = NULL;

  err = bt_device->common.close(&bt_device->common);
  if (err)
    ALOGW("get_bluetooth_interface failed: %s", strerror(err));
  bt_device = NULL;
}

/*
 * Bluedroid wrapper functions
 */

int
bt_core_init(bt_callbacks_t* callbacks)
{
  assert(bt_interface);
  assert(bt_interface->init);

  return bt_interface->init(callbacks);
}

int
bt_core_enable()
{
  assert(bt_interface);
  assert(bt_interface->enabled);

  return bt_interface->enable();
}

int
bt_core_disable()
{
  assert(bt_interface);
  assert(bt_interface->disable);

  return bt_interface->disable();
}

void
bt_core_cleanup()
{
  assert(bt_interface);
  assert(bt_interface->cleanup);

  bt_interface->cleanup();
}

int
bt_core_get_adapter_properties()
{
  assert(bt_interface);
  assert(bt_interface->get_adapter_properties);

  return bt_interface->get_adapter_properties();
}

int
bt_core_get_adapter_property(bt_property_type_t type)
{
  assert(bt_interface);
  assert(bt_interface->get_adapter_property);

  return bt_interface->get_adapter_property(type);
}

int
bt_core_set_adapter_property(const bt_property_t* property)
{
  assert(bt_interface);
  assert(bt_interface->set_adapter_property);

  return bt_interface->set_adapter_property(property);
}

int
bt_core_get_remote_device_properties(bt_bdaddr_t* remote_addr)
{
  assert(bt_interface);
  assert(bt_interface->get_remote_device_properties);

  return bt_interface->get_remote_device_properties(remote_addr);
}

int
bt_core_get_remote_device_property(bt_bdaddr_t *remote_addr,
                              bt_property_type_t type)
{
  assert(bt_interface);
  assert(bt_interface->get_remote_device_property);

  return bt_interface->get_remote_device_property(remote_addr, type);
}

int
bt_core_set_remote_device_property(bt_bdaddr_t* remote_addr,
                        const bt_property_t *property)
{
  assert(bt_interface);
  assert(bt_interface->set_remote_device_property);

  return bt_interface->set_remote_device_property(remote_addr, property);
}

int
bt_core_get_remote_service_record(bt_bdaddr_t* remote_addr, bt_uuid_t* uuid)
{
  assert(bt_interface);
  assert(bt_interface->get_remote_service_record);

  return bt_interface->get_remote_service_record(remote_addr, uuid);
}

int
bt_core_get_remote_services(bt_bdaddr_t* remote_addr)
{
  assert(bt_interface);
  assert(bt_interface->get_remote_services);

  return bt_interface->get_remote_services(remote_addr);
}

int
bt_core_start_discovery()
{
  assert(bt_interface);
  assert(bt_interface->start_discovery);

  return bt_interface->start_discovery();
}

int
bt_core_cancel_discovery()
{
  assert(bt_interface);
  assert(bt_interface->cancel_discovery);

  return bt_interface->cancel_discovery();
}

int
bt_core_create_bond(const bt_bdaddr_t* bd_addr)
{
  assert(bt_interface);
  assert(bt_interface->create_bond);

  return bt_interface->create_bond(bd_addr);
}

int
bt_core_remove_bond(const bt_bdaddr_t* bd_addr)
{
  assert(bt_interface);
  assert(bt_interface->remove_bond);

  return bt_interface->remove_bond(bd_addr);
}

int
bt_core_cancel_bond(const bt_bdaddr_t* bd_addr)
{
  assert(bt_interface);
  assert(bt_interface->cancel_bond);

  return bt_interface->cancel_bond(bd_addr);
}

int
bt_core_pin_reply(const bt_bdaddr_t* bd_addr, uint8_t accept, uint8_t pin_len,
                   bt_pin_code_t* pin_code)
{
  assert(bt_interface);
  assert(bt_interface->pin_reply);

  return bt_interface->pin_reply(bd_addr, accept, pin_len, pin_code);
}

int
bt_core_ssp_reply(const bt_bdaddr_t* bd_addr, bt_ssp_variant_t variant, uint8_t accept,
             uint32_t passkey)
{
  assert(bt_interface);
  assert(bt_interface->ssp_reply);

  return bt_interface->ssp_reply(bd_addr, variant, accept, passkey);
}

const void*
bt_core_get_profile_interface(const char* profile_id)
{
  assert(bt_interface);
  assert(bt_interface->get_profile_interface);

  return bt_interface->get_profile_interface(profile_id);
}

int
bt_core_dut_mode_configure(uint8_t enable)
{
  assert(bt_interface);
  assert(bt_interface->dut_mode_configure);

  return bt_interface->dut_mode_configure(enable);
}

int
bt_core_dut_mode_send(uint16_t opcode, uint8_t* buf, uint8_t len)
{
  assert(bt_interface);
  assert(bt_interface->dut_mode_send);

  return bt_interface->dut_mode_send(opcode, buf, len);
}

int
bt_core_le_test_mode(uint16_t opcode, uint8_t* buf, uint8_t len)
{
  assert(bt_interface);
  assert(bt_interface->le_test_mode);

  return bt_interface->le_test_mode(opcode, buf, len);
}

int
bt_core_config_hci_snoop_log(uint8_t enable)
{
  assert(bt_interface);
#if ANDROID_VERSION >= 19
  assert(bt_interface->config_hci_snoop_log);

  return bt_interface->config_hci_snoop_log(enable);
#else
  return BT_STATUS_UNSUPPORTED;
#endif
}
