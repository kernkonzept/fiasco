INTERFACE:

class Irq_base;

/**
 * Processes and buffers kernel console input.
 *
 * Vkey is designed to be used by a single producer and a single consumer.
 *
 * The producer calls `Vkey::check_()` when there might be new input on the
 * kernel console.
 *
 * The consumer calls `Vkey::get()` and `Vkey::clear()` to read the buffered
 * input. In addition, the consumer can register a notification IRQ with Vkey,
 * which is triggered when an input character is added to the buffer while
 * it is empty.
 */
class Vkey
{
public:
  enum Echo_type { Echo_off = 0, Echo_on = 1, Echo_crnl = 2 };
};


// ---------------------------------------------------------------------------
IMPLEMENTATION:

#include "irq_chip.h"
#include "global_data.h"

static DEFINE_GLOBAL Global_data<Irq_base *const *> vkey_irq;

PUBLIC static
void
Vkey::irq(Irq_base *const *i)
{ vkey_irq = i; }

// ------------------------------------------------------------------------
IMPLEMENTATION [serial && debug]:

PRIVATE static inline
bool
Vkey::is_debugger_entry_key(int key)
{
  return key == KEY_SINGLE_ESC;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [serial && !debug]:

PRIVATE static inline
bool
Vkey::is_debugger_entry_key(int)
{
  return false;
}

// ---------------------------------------------------------------------------
IMPLEMENTATION [serial]:

#include <cstdio>

#include "config.h"
#include "cpu.h"
#include "globals.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "mem.h"

static DEFINE_GLOBAL Global_data<Vkey::Echo_type> vkey_echo;
static DEFINE_GLOBAL Global_data<char[256]> vkey_buffer;
static DEFINE_GLOBAL Global_data<unsigned> vkey_tail, vkey_head;
static DEFINE_GLOBAL Global_data<bool> enable_rcv;

PUBLIC static
void
Vkey::enable_receive()
{ enable_rcv = true; }

PUBLIC static
bool
Vkey::receive_enabled()
{ return enable_rcv; }

PUBLIC static
void
Vkey::set_echo(Echo_type echo)
{
  vkey_echo = echo;
}

PRIVATE static
bool
Vkey::add(int c)
{
  bool hit = false;

  unsigned nh = (vkey_head + 1) % sizeof(vkey_buffer);
  unsigned oh = vkey_head;
  unsigned tail = access_once(&vkey_tail);
  // The control dependency between `access_once(&vkey_tail)` and
  // `write_now(&vkey_buffer[vkey_head], c)`, in form of this if statement,
  // ensures that we don't overwrite a character in vkey_buffer before the
  // consumer has read it, i.e. updated the vkey_tail index.
  // This implicit barrier is paired with Mem::mp_mb() in Vkey::clear().
  if (nh != tail)
    {
      write_now(&vkey_buffer[vkey_head], c);
      // Ensure updating vkey_head is ordered after writing vkey_buffer.
      // This barrier is paired with Mem::mp_rmb() in Vkey::get().
      Mem::mp_wmb();
      vkey_head = nh;
    }

  if (oh == tail)
    hit = true;

  if (vkey_echo == Vkey::Echo_crnl && c == '\r')
    c = '\n';

  if (vkey_echo)
    putchar(c);

  return hit;
}

PRIVATE static
bool
Vkey::add(const char *seq)
{
  bool hit = false;
  for (; *seq; ++seq)
    hit |= add(*seq);
  return hit;
}

PRIVATE static
void
Vkey::trigger()
{
  Irq_base *i = access_once(vkey_irq.unwrap());
  if (i)
    i->hit(nullptr);
}

PUBLIC static
int
Vkey::check_()
{
  int  ret = 1;
  bool hit = false;

  // disable last branch recording, branch trace recording ...
  Cpu::cpus.cpu(current_cpu()).debugctl_disable();

  while (1)
    {
      int c = Kconsole::console()->getchar(false);

      if (c == -1)
	break;

      if (is_debugger_entry_key(c))
	{
	  ret = 0;  // break into kernel debugger
	  break;
	}

      switch (c)
	{
	case KEY_CURSOR_UP:    hit |= add("\033[A"); break;
	case KEY_CURSOR_DOWN:  hit |= add("\033[B"); break;
	case KEY_CURSOR_LEFT:  hit |= add("\033[D"); break;
	case KEY_CURSOR_RIGHT: hit |= add("\033[C"); break;
	case KEY_CURSOR_HOME:  hit |= add("\033[1~"); break;
	case KEY_CURSOR_END:   hit |= add("\033[4~"); break;
	case KEY_PAGE_UP:      hit |= add("\033[5~"); break;
	case KEY_PAGE_DOWN:    hit |= add("\033[6~"); break;
	case KEY_INSERT:       hit |= add("\033[2~"); break;
	case KEY_DELETE:       hit |= add("\033[3~"); break;
	case KEY_F1:           hit |= add("\033OP"); break;
	case KEY_BACKSPACE:    hit |= add(127); break;
	case KEY_TAB:          hit |= add(9); break;
	case KEY_ESC:          hit |= add(27); break;
	case KEY_SINGLE_ESC:   hit |= add(27); break;
	case KEY_RETURN:       hit |= add(13); break;
	case KEY_RETURN_2:     hit |= add(13); break;
	default:               hit |= add(c); break;
	}
    }

  if (hit)
    trigger();

  // reenable debug stuff (undo debugctl_disable)
  Cpu::cpus.cpu(current_cpu()).debugctl_enable();

  return ret;
}

PUBLIC static
int
Vkey::get()
{
  if (vkey_tail != vkey_head)
    {
      // Ensure reading vkey_buffer is ordered after reading vkey_head.
      // This barrier is paired with Mem::mp_wmb() in Vkey::add().
      Mem::mp_rmb();
      return vkey_buffer[vkey_tail];
    }

  return -1;
}

PUBLIC static
void
Vkey::clear()
{
  if (vkey_tail != vkey_head)
    {
      // Ensure reading vkey_buffer in Vkey::get() is ordered before updating
      // vkey_tail, otherwise it could happen that Vkey::add() overwrites the
      // character in vkey_buffer before Vkey::get() has read it.
      // This barrier is paired with the control dependency in Vkey::add().
      Mem::mp_mb();
      vkey_tail = (vkey_tail + 1) % sizeof(vkey_buffer);
    }
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!serial]:

#include "kernel_console.h"

PUBLIC static inline
void
Vkey::enable_receive()
{}

PUBLIC static inline
bool
Vkey::receive_enabled()
{ return false; }

PUBLIC static inline
void
Vkey::set_echo(Echo_type)
{}

PUBLIC static inline
void
Vkey::clear()
{}

PUBLIC static
int
Vkey::get()
{
  return Kconsole::console()->getchar(0);
}

PUBLIC static inline
int
Vkey::check_(int = -1)
{ return 0; }
