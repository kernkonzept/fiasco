INTERFACE [riscv && 32bit]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout : Address {
    Sdram_phys_base       = RAM_PHYS_BASE
  };

  enum Virt_layout : Address {
    User_max              = 0xbfffffff,
    Utcb_addr             = User_max + 1 - 0x10000,
  };

  enum Virt_layout_kern : Address {
    Jdb_map_base          = 0xeac00000,
    Jdb_tmp_map_area      = Jdb_map_base,
    Tbuf_status_page      = Jdb_map_base + 0x200000,
    Tbuf_buffer_area      = Jdb_map_base + 0x400000,
    Tbuf_buffer_size      = 0x2000000,

    Mmio_map_start        = 0xee000000,
    Mmio_map_end          = 0xef000000,

    Map_base              = 0xf0000000,
    Pmem_start            = 0xf0400000,
    Pmem_end              = 0xf5000000,

    Caps_start            = 0xf5000000,
    Caps_end              = 0xfd000000,

    Max_kernel_image_size = Map_base - Pmem_start,
  };
};
