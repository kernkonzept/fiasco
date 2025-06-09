INTERFACE [debug]:

EXTENSION class Kip
{
private:
  void debug_print_syscalls() const;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include <cstdio>
#include <cstring>
#include "simpleio.h"

static char const *const memory_desc_types[] = {
    "Undefined",
    "Conventional",
    "Reserved",
    "Dedicated",
    "Shared",
    "(undef)",
    "(undef)",
    "(undef)",
    "(undef)",
    "(undef)",
    "(undef)",
    "(undef)",
    "(undef)",
    "(undef)",
    "Bootloader",
    "Arch"
};

PUBLIC
void
Mem_desc::dump() const
{
  printf("%s [%016llx-%016llx] %s", is_virtual() ? "virt" : "phys",
         Unsigned64{start()}, Unsigned64{end()} + 1, memory_desc_types[type()]);
}

PRIVATE
void
Kip::debug_print_memory() const
{
  printf("Memory (max %u descriptors):\n",num_mem_descs());
  for (auto const &m: mem_descs_a())
    {
      if (m.type() != Mem_desc::Undefined)
        {
          printf(" %2u:", mem_descs_a().index(m) + 1);
          m.dump();
          puts("");
        }
    }
}

PRIVATE
void
Kip::debug_print_features() const
{
  printf("Kernel features:");
  char const *f = version_string();
  for (f += strlen(f) + 1; *f; f += strlen(f) + 1)
    {
      putchar(' ');
      putstr(f);
    }
  putchar('\n');
}

IMPLEMENT
void
Kip::print() const
{
  printf("KIP @ %p\n", static_cast<void const *>(this));
  printf("magic: %.4s  version: 0x%x\n",
         reinterpret_cast<char const *>(&magic), version);
  print_clock();
  printf("freq_cpu: %llukHz\n", frequency_cpu);

  printf("sigma0_ip: %016llx\n", sigma0_ip);
  printf("root_ip:   %016llx\n", root_ip);
  debug_print_memory();
  debug_print_syscalls();

  printf("user_ptr: %016llx   acpi_rsdp_addr: %016llx\n",
         user_ptr, acpi_rsdp_addr);

  debug_print_features();
}

//----------------------------------------------------------------------------
IMPLEMENTATION[!sync_clock && debug]:

PRIVATE inline
void
Kip::print_clock() const
{
  Cpu_time c = clock();
  printf("clock: " L4_X64_FMT " (%llu)\n", c, c);
  printf("uptime: %llu day(s), %llu hour(s), %llu min(s), %llu sec(s)\n",
          c / (1000000ULL * 60 * 60 * 24),
         (c / (1000000ULL * 60 * 60))    % 24,
         (c / (1000000ULL * 60))         % 60,
         (c /  1000000ULL)               % 60);
}

//----------------------------------------------------------------------------
IMPLEMENTATION[sync_clock && debug]:

PRIVATE inline
void
Kip::print_clock() const
{}
