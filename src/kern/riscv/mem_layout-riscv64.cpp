INTERFACE [riscv && 64bit]:

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return _User_max; }

  enum Virt_layout_kern : Address {
    Jdb_map_base          = 0xffffffc000000000,
    Jdb_tmp_map_area      = Jdb_map_base,
    Tbuf_status_page      = Jdb_map_base + 0x200000,
    Tbuf_buffer_area      = Jdb_map_base + 0x400000,
    Tbuf_buffer_size      = 0x2000000,

    Mmio_map_start        = 0xffffffd000000000,
    Mmio_map_end          = 0xffffffd040000000,

    Map_base              = 0xfffffff000000000,
    Pmem_start            = 0xfffffff040000000,
    Pmem_end              = 0xfffffff140000000,

    Max_kernel_image_size = Map_base - Pmem_start,
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && 64bit && riscv_sv39]:

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address {
    _User_max             = 0x0000003fffffffffUL,
    Utcb_addr             = _User_max + 1 - 0x10000,
  };

};

//----------------------------------------------------------------------------
INTERFACE [riscv && 64bit && riscv_sv48]:

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address {
    _User_max             = 0x00007fffffffffffUL,
    Utcb_addr             = _User_max + 1 - 0x10000,
  };
};
