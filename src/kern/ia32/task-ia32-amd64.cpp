IMPLEMENTATION [ia32 || amd64]:

#include "gdt.h"
#include "std_macros.h"
#include "x86desc.h"

PRIVATE inline NEEDS["gdt.h"]
bool
Task::invoke_arch(L4_msg_tag &tag, Utcb *utcb)
{
  switch (utcb->values[0])
    {
    case Ldt_set_x86:
        {
          enum
          {
            Utcb_values_per_ldt_entry
              = Cpu::Ldt_entry_size / sizeof(utcb->values[0]),
          };
          if (EXPECT_FALSE(tag.words() < 3
                           || tag.words() % Utcb_values_per_ldt_entry))
            {
              tag = commit_result(-L4_err::EInval);
              return true;
            }

          unsigned entry_number  = utcb->values[1];
          unsigned size          = (tag.words() - 2) * sizeof(utcb->values[0]);

          // Allocate the memory if not yet done
          if (!_ldt.addr())
            _ldt.alloc();

          if (entry_number * Cpu::Ldt_entry_size + size > Config::PAGE_SIZE)
            {
              WARN("set_ldt: LDT size exceeds one page, not supported.");
              tag = commit_result(-L4_err::EInval);
              return true;
            }

          _ldt.size(size + Cpu::Ldt_entry_size * entry_number);

          Address desc_addr = reinterpret_cast<Address>(&utcb->values[2]);
          Gdt_entry desc;
          Gdt_entry *ldtp
            = reinterpret_cast<Gdt_entry *>(_ldt.addr()) + entry_number;

          while (size >= Cpu::Ldt_entry_size)
            {
              desc = *reinterpret_cast<Gdt_entry const *>(desc_addr);
              if (desc.unsafe())
                {
                  WARN("set_ldt: Bad descriptor.");
                  tag = commit_result(-L4_err::EInval);
                  return true;
                }

              *ldtp      = desc;
              size      -= Cpu::Ldt_entry_size;
              desc_addr += Cpu::Ldt_entry_size;
              ldtp++;
            }

          if (this == current()->space())
            Cpu::cpus.cpu(current_cpu()).enable_ldt(_ldt.addr(), _ldt.size());

          tag = commit_result(0);
        }
      return true;
    }




  return false;
}
