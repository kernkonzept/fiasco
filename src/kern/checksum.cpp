INTERFACE:

class Checksum 
{
};


IMPLEMENTATION:

#include "mem_layout.h"
#include "boot_info.h"

// calculate simple checksum over kernel text section and read-only data
PUBLIC static 
unsigned Checksum::get_checksum_ro()
{
  unsigned *p, sum = 0;

  for (p = (unsigned*)&Mem_layout::start; 
       p < (unsigned*)&Mem_layout::etext; sum += *p++)
    ;

  return sum;
}

PUBLIC static
bool
Checksum::check_ro()
{ return Boot_info::get_checksum_ro() == get_checksum_ro(); }


// calculate simple checksum over kernel data section
PUBLIC static
unsigned Checksum::get_checksum_rw()
{
  unsigned *p, sum = 0;

  for (p = (unsigned*)&Mem_layout::data_start;
       p < (unsigned*)&Mem_layout::edata; sum += *p++)
    ;

  return sum;
}

