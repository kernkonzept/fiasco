IMPLEMENTATION [!noncont_mem]:

PUBLIC static
Address
Kmem::mmio_remap(Address phys)
{
  auto m = kdir->walk(Virt_addr(phys), K_pte_ptr::Super_level);
  if (m.is_valid())
    {
      assert (m.page_order() >= Config::SUPERPAGE_SHIFT);
      assert (m.page_addr() == cxx::mask_lsb(phys, m.page_order()));
      assert (m.attribs().type == Page::Type::Uncached());
      return phys;
    }

  assert (m.page_order() == Config::SUPERPAGE_SHIFT);
  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);
  m.set_page(m.make_page(Phys_mem_addr(phys_page),
                         Page::Attr(Page::Rights::RW(),
                                    Page::Type::Uncached(),
                                    Page::Kern::Global())));

  m.write_back_if(true, Mem_unit::Asid_kernel);

  return phys;
}

