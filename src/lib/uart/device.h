#pragma once

#include <stdint.h>

struct l4re_device_spec_dt_ids
{
  const char *compatible;
};

struct l4re_device_spec_pcidev_pciids
{
  uint16_t vendor;
  uint16_t function;
};

struct l4re_device_spec_pcidev_ids
{
  struct l4re_device_spec_pcidev_pciids *pcidevs;
};

struct l4re_device_ids
{
  struct l4re_device_spec_dt_ids     *dt = nullptr;
  struct l4re_device_spec_pcidev_ids *pcidev = nullptr;
};

// ---- Internal device
namespace L4 {

  class Dev_obj
  {
  };

}

struct l4re_device_type_base
{
  virtual L4::Dev_obj *create(void *obj_mem, unsigned arg1) = 0;
  virtual unsigned obj_size() = 0;
  virtual const l4re_device_ids *ids() = 0;
};

#include <stdio.h>
#include <string.h>

static inline
L4::Dev_obj *
l4re_dev_create_by_dt_compatible(const char *dt_compatible,
                                 void *obj_store, unsigned obj_store_size,
                                 unsigned arg1)
{
  l4re_device_type_base *device = nullptr;

  extern l4re_device_type_base *__L4RE_DEVICE_BEGIN[];
  extern l4re_device_type_base *__L4RE_DEVICE_END[];

  for (auto const *d = __L4RE_DEVICE_BEGIN; d < __L4RE_DEVICE_END; ++d)
    {
      const l4re_device_ids *ids = (*d)->ids();
      if (ids->dt)
        for (l4re_device_spec_dt_ids *dt_id = ids->dt; dt_id->compatible; ++dt_id)
          if (!strcmp(dt_compatible, dt_id->compatible))
            {
              device = *d;
              break;
            }
    }

  if (!device)
    return nullptr;

  if (device->obj_size() > obj_store_size)
    return nullptr;

  return device->create(reinterpret_cast<void *>(obj_store), arg1);
}

#define l4re_register_device(Device_class_base, Device_class, \
                             instance_name, __ids) \
  struct l4re_device_type_##instance_name \
  : public l4re_device_type_base \
  { \
    Device_class_base *create(void *objmem, unsigned arg1) override \
    { return new (objmem) Device_class(arg1); } \
    \
    unsigned obj_size() override { return sizeof(Device_class); } \
    const l4re_device_ids *ids() override { return &__ids; } \
  }; \
  static const l4re_device_type_##instance_name \
    l4re_device_inst_##instance_name; \
  static const l4re_device_type_base * const \
    __attribute__((section(".data.l4redevice"),used)) \
    l4re_device_inst_p_##instance_name \
    = &l4re_device_inst_##instance_name
