INTERFACE:

#include "l4_types.h"

class Mem_layout
{
public:
  /// reflect symbols in linker script
  static const char load            asm ("_load");
  static const char image_start     asm ("_kernel_image_start");
  static const char start           asm ("_start");
  static const char end             asm ("_end");
  static const char ecode           asm ("_ecode");
  static const char etext           asm ("_etext");
  static const char data_start      asm ("_kernel_data_start");
  static const char edata           asm ("_edata");
  static const char initcall_start  asm ("_initcall_start");
  static const char initcall_end    asm ("_initcall_end");

  static Mword in_tcbs (Address a); // FIXME
  static Mword in_kernel (Address a); // XXX: not right for UX
};

IMPLEMENTATION [obj_space_virt]:

PUBLIC static inline
bool
Mem_layout::is_caps_area(Address a)
{ return (a >= Caps_start) && (a < Caps_end); }

IMPLEMENTATION [!obj_space_virt]:

PUBLIC static inline
bool
Mem_layout::is_caps_area(Address)
{ return false; }

IMPLEMENTATION:

#include "config.h"

IMPLEMENT inline
Mword
Mem_layout::in_kernel (Address a)
{
  return a > User_max;
}

PUBLIC static inline
Mword
Mem_layout::in_kernel_code (Address a)
{
  return a >= (Address)&start && a < (Address)&ecode;
}

