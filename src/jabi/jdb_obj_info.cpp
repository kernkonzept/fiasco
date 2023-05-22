INTERFACE:

#include "types.h"

struct Jobj_info
{
  // See Jdb_mapdb::info_obj_mapping().
  struct Mapping
  {
    enum { Type = 0 };
    Unsigned64 mapping_ptr;
    char space_name[16];
    Unsigned32 cap_idx;
    Unsigned16 entry_rights;
    Unsigned16 entry_flags;
    Unsigned64 entry_ptr;
  }; // 40 bytes

  // See Jdb_tcb::info_kobject().
  struct Thread
  {
    enum { Type = 1 };
    bool is_kernel;
    bool is_current;
    bool in_ready_list;
    bool is_kernel_task;
    Unsigned32 home_cpu;
    Unsigned32 current_cpu;
    Signed64 ref_cnt;
    Unsigned64 space_id;
  }; // 32 bytes

  // See Jdb_space::info_kobject().
  struct Space
  {
    enum { Type = 2 };
    bool is_kernel;
    Signed64 ref_cnt;
  }; // 16 bytes

  // See Jdb_vm::info_kobject().
  struct Vm
  {
    enum { Type = 3 };
    Unsigned64 utcb;
    Unsigned64 pc;
  }; // 16 bytes

  // See Jdb_ipc_gate::info_kobject().
  struct Ipc_gate
  {
    enum { Type = 4 };
    Unsigned64 label;
    Unsigned64 thread_id;
  }; // 16 bytes

  // See Jdb_kobject_irq::info_kobject().
  struct Irq_sender
  {
    enum { Type = 5 };
    char chip_type[10];
    Unsigned16 flags;
    Unsigned32 pin;
    Unsigned64 label;
    Unsigned64 target_id;
    Signed64 queued;
  }; // 40 bytes

  // See Jdb_kobject_irq::info_kobject().
  struct Irq_semaphore
  {
    enum { Type = 6 };
    char chip_type[10];
    Unsigned16 flags;
    Unsigned32 pin;
    Signed64 queued;
  }; // 24 bytes

  // See Jdb_factory::info_kobject().
  struct Factory
  {
    enum { Type = 7 };
    Unsigned64 current;
    Unsigned64 limit;
  }; // 16 bytes

  // See Jdb_mapdb::obj_info().
  struct Jdb         { enum { Type =  8 }; };
  struct Scheduler   { enum { Type =  9 }; };
  struct Vlog        { enum { Type = 10 }; };
  struct Pfc         { enum { Type = 11 }; };
  struct Dmar_space  { enum { Type = 12 }; };
  struct Iommu       { enum { Type = 13 }; };
  struct Smmu        { enum { Type = 14 }; };

  Unsigned64 type:5;      // See struct*/Type
  Unsigned64 id:59;       // 59 bits shall be sufficient
  Unsigned64 mapping_ptr;
  Unsigned64 ref_cnt;     // Kobject_mappable::_cnt
  union
  {
    Mapping mapping;
    Thread thread;
    Space space;
    Vm vm;
    Ipc_gate ipc_gate;
    Irq_sender irq_sender;
    Irq_semaphore irq_semaphore;
    Factory factory;
    Unsigned64 raw[5];
  };
};

static_assert(sizeof(Jobj_info) == 64, "Size of Jobj_info");

