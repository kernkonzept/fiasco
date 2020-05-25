IMPLEMENTATION [!noncont_mem]:

PRIVATE static
bool
Kmem::cont_mapped(Address phys_beg, Address phys_end, Address virt)
{
  for (Address p = phys_beg, v = virt;
       p < phys_end && v < Mem_layout::Registers_map_end;
       p += Config::SUPERPAGE_SIZE, v += Config::SUPERPAGE_SIZE)
    {
      auto e = kdir->walk(Virt_addr(v), K_pte_ptr::Super_level);
      if (!e.is_valid() || p != e.page_addr())
        return false;
    }

  return true;
}

PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address size)
{
  static Address ndev = 0;
  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);
  Address phys_end  = Mem_layout::round_superpage(phys + size);

  for (Address a = Mem_layout::Registers_map_start;
       a < Mem_layout::Registers_map_end; a += Config::SUPERPAGE_SIZE)
    {
      if (cont_mapped(phys_page, phys_end, a))
        return (phys & ~Config::SUPERPAGE_MASK) | (a & Config::SUPERPAGE_MASK);
    }


  Address map_addr = Mem_layout::Registers_map_start + ndev;

  for (Address p = phys_page; p < phys_end; p+= Config::SUPERPAGE_SIZE)
    {
      Address dm = Mem_layout::Registers_map_start + ndev;
      assert(dm < Mem_layout::Registers_map_end);

      ndev += Config::SUPERPAGE_SIZE;

      auto m = kdir->walk(Virt_addr(dm), K_pte_ptr::Super_level);
      assert (!m.is_valid());
      assert (m.page_order() == Config::SUPERPAGE_SHIFT);
      m.set_page(m.make_page(Phys_mem_addr(p),
                             Page::Attr(Page::Rights::RW(),
                                        Page::Type::Uncached(),
                                        Page::Kern::Global())));

      m.write_back_if(true, Mem_unit::Asid_kernel);
    }

  return (phys & ~Config::SUPERPAGE_MASK) | (map_addr & Config::SUPERPAGE_MASK);
}
