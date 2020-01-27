INTERFACE [amd64]:

#include "types.h"

class Syscall_entry_data
{
  Unsigned64 _kern_sp = 0;
  Unsigned64 _user_sp = 0;
} __attribute__((packed, aligned(64))); // Enforce cacheline alignment

struct Syscall_entry_text
{
  char _res[32]; // Keep this in sync with code in syscall_entry_text!
} __attribute__((packed, aligned(32)));

IMPLEMENTATION [amd64]:

PUBLIC inline
void
Syscall_entry_data::set_rsp(Address rsp)
{
  _kern_sp = rsp;
}


