INTERFACE [jdb]:

EXTENSION class Kip
{
private:
  void debug_print_syscalls() const;
};

IMPLEMENTATION [jdb]:


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
  printf("%s [%016lx-%016lx] %s", is_virtual() ? "virt" : "phys",
         start(), end() + 1, memory_desc_types[type()]);
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
          printf(" %2d:", ((int)mem_descs_a().index(m)) + 1);
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
void Kip::print() const
{
  Cpu_time c = clock();
  printf("KIP @ %p\n", this);
  printf("magic: %.4s  version: 0x%lx\n", (char*)&magic, version);
  printf("clock: " L4_X64_FMT " (%llu)\n", c, c);
  printf("uptime: %llu day(s), %llu hour(s), %llu min(s), %llu sec(s)\n",
          c / (1000000ULL * 60 * 60 * 24),
         (c / (1000000ULL * 60 * 60))    % 24,
         (c / (1000000ULL * 60))         % 60,
         (c /  1000000ULL)               % 60);
  printf("freq_cpu: %lukHz\n", frequency_cpu);
  printf("freq_bus: %lukHz\n", frequency_bus);

  for (int i = 0; i < 8; i++)
    {
      printf("sigma0[%d]: " L4_PTR_FMT "\n", i, sigma0[i]);
      printf("root[%d]:   " L4_PTR_FMT "\n", i, root[i]);
    }

  debug_print_memory();
  debug_print_syscalls();

  printf("user_ptr: %p   vhw_offset: " L4_MWORD_FMT "\n",
         (void*)user_ptr, vhw_offset);

  debug_print_features();
}
