IMPLEMENTATION [!noncont_mem]:

PUBLIC static
Address
Kmem::mmio_remap(Address phys)
{
  static Address ndev = 0;
  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);

  for (Address a = Mem_layout::Registers_map_start;
       a < Mem_layout::Registers_map_end;
       a += Config::SUPERPAGE_SIZE)
    {
      auto e = kdir->walk(Virt_addr(a), K_pte_ptr::Super_level);
      if (e.is_valid() && phys_page == e.page_addr())
        return (phys & ~Config::SUPERPAGE_MASK) | (a & Config::SUPERPAGE_MASK);
    }


  Address dm = Mem_layout::Registers_map_start + ndev;
  assert(dm < Mem_layout::Registers_map_end);

  ndev += Config::SUPERPAGE_SIZE;

  auto m = kdir->walk(Virt_addr(dm), K_pte_ptr::Super_level);
  assert (!m.is_valid());
  assert (m.page_order() == Config::SUPERPAGE_SHIFT);
  m.set_page(m.make_page(Phys_mem_addr(phys_page),
                         Page::Attr(Page::Rights::RW(),
                                    Page::Type::Uncached(),
                                    Page::Kern::Global())));

  m.write_back_if(true, Mem_unit::Asid_kernel);

  return (phys & ~Config::SUPERPAGE_MASK) | (dm & Config::SUPERPAGE_MASK);
}
