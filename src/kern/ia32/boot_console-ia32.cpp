INTERFACE[ia32 || amd64]:

class Console;

class Boot_console
{
public:
  static void init();
};

IMPLEMENTATION[ia32 || amd64]:

#include <cstring>
#include <cstdio>
#include <cxx/conditionals>

#include "kernel_console.h"
#include "keyb.h"
#include "mux_console.h"
#include "initcalls.h"
#include "koptions.h"
#include "static_init.h"
#include "vga_console.h"
#include "mem_layout.h"

static Static_object<Vga_console> vga;
static Static_object<Keyb> keyb;

IMPLEMENT FIASCO_INIT
void Boot_console::init()
{
  keyb.construct();
  if (Koptions::o()->opt(Koptions::F_keymap_de))
    keyb->set_keymap(Keyb::Keymap_de);
  Kconsole::console()->register_console(keyb);

  if (Koptions::o()->opt(Koptions::F_noscreen))
    return;

  vga.construct(Address{Mem_layout::Adap_vram_cga_beg}, 80,
                cxx::const_ite<TAG_ENABLED(irq_spinner)>(20, 25), true, true);

  if (vga->is_working())
    Kconsole::console()->register_console(vga);

  if constexpr (TAG_ENABLED(irq_spinner))
    for (int y = 20; y < 25; ++y)
      for (int x = 0; x < 80; ++x)
        vga->printchar(x, y, ' ', 8);
};
