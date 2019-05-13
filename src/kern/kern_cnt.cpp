INTERFACE:

#include "types.h"

class Kern_cnt
{
public:
  enum {
    Valid_ctrs = 7,
  };

private:
  enum {
    Max_slot = 2,
  };

  static Unsigned32 *kcnt[Max_slot];
  static Mword (*read_kcnt_fn[Max_slot])();
  static Unsigned8 valid_ctrs[Valid_ctrs];
};


IMPLEMENTATION:

#include "jdb_ktrace.h"
#include "mem_layout.h"
#include "tb_entry.h"
#include "jdb_tbuf.h"

Unsigned32 *Kern_cnt::kcnt[Max_slot];
Mword (*Kern_cnt::read_kcnt_fn[Max_slot])() = { read_kcnt1, read_kcnt2 };

// Only the following counters have to be used. The remaining Kern_cnt_XXX
// enums are deprecated but we don't want to change the trace buffer status
// page.
Unsigned8 Kern_cnt::valid_ctrs[Kern_cnt::Valid_ctrs] =
{
  Kern_cnt_context_switch,
  Kern_cnt_addr_space_switch,
  Kern_cnt_irq,
  Kern_cnt_page_fault,
  Kern_cnt_io_fault,
  Kern_cnt_schedule,
  Kern_cnt_exc_ipc,
};

static Mword Kern_cnt::read_kcnt1() { return (Mword)*kcnt[0]; }
static Mword Kern_cnt::read_kcnt2() { return (Mword)*kcnt[1]; }

PUBLIC static
int
Kern_cnt::valid_2_ctr(unsigned num)
{
  return num >= Valid_ctrs ? -1 : valid_ctrs[num];
}

PUBLIC static
int
Kern_cnt::ctr_2_valid(unsigned num)
{
  for (unsigned i = 0; i < Valid_ctrs; ++i)
    if (valid_ctrs[i] == num)
      return i;

  return -1;
}

PUBLIC static
Unsigned32*
Kern_cnt::get_ctr(int num)
{
  if (num >= Kern_cnt_max)
    return nullptr;

  return Jdb_tbuf::status()->kerncnts + num;
}

PUBLIC static
Unsigned32*
Kern_cnt::get_vld_ctr(int num)
{
  if (num >= Valid_ctrs)
    return nullptr;

  return get_ctr(valid_2_ctr(num));
}

PUBLIC static
const char *
Kern_cnt::get_str(unsigned num)
{
  switch (num)
    {
    case Kern_cnt_context_switch:    return "Context switches";
    case Kern_cnt_addr_space_switch: return "Address space switches";
    case Kern_cnt_shortcut_failed:   return "IPC shortcut failed";
    case Kern_cnt_shortcut_success:  return "IPC shortcut success";
    case Kern_cnt_irq:               return "Hardware interrupts";
    case Kern_cnt_ipc_long:          return "Long IPCs";
    case Kern_cnt_page_fault:        return "Page faults";
    case Kern_cnt_io_fault:          return "IO bitmap faults";
    case Kern_cnt_task_create:       return "Tasks created";
    case Kern_cnt_schedule:          return "Scheduler calls";
    case Kern_cnt_iobmap_tlb_flush:  return "IO bitmap TLB flushs";
    case Kern_cnt_exc_ipc:           return "Exception IPCs";
    default:                         return nullptr;
    }
}

PUBLIC static
const char *
Kern_cnt::get_vld_str(unsigned num)
{
  if (num >= Valid_ctrs)
    return nullptr;

  return get_str(valid_2_ctr(num));
}

PUBLIC static
int
Kern_cnt::mode(Mword slot, const char **mode, const char **name, Mword *event)
{
  Unsigned32 *c = 0;

  switch (slot)
    {
    case 0: c = kcnt[0]; break;
    case 1: c = kcnt[1]; break;
    }

  if (c)
    {
      Mword num = c - get_ctr(0);

      *mode  = "on";
      *name  = get_str(num);
      *event = 0x80000000 | num;
      return 1;
    }

  *mode = "off";
  *name = "";
  return 0;
}

PUBLIC static
int
Kern_cnt::setup_pmc(Mword slot, Mword event)
{
  if (slot>=Max_slot)
    return 0;

  if (!(event & 0x80000000))
    {
      kcnt[slot] = nullptr;
      Tb_entry::set_rdcnt(slot, 0);
      return 0;
    }

  event &= 0xff;
  if (event >= Kern_cnt_max)
    return 0;

  kcnt[slot] = get_ctr(event);
  Tb_entry::set_rdcnt(slot, read_kcnt_fn[slot]);
  return 1;
}
