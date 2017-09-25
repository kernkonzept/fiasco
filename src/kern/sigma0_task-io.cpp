IMPLEMENTATION [io]:

PUBLIC
bool
Sigma0_task::v_fabricate(Io_space::V_pfn address, Io_space::Phys_addr *phys,
                         Io_space::Page_order *size, Io_space::Attr *attribs = 0)
{
  // special-cased because we don't do lookup for sigma0
  *size = Io_space::Page_order(Io_space::Map_superpage_shift);
  *phys = cxx::mask_lsb(address, *size);
  if (attribs)
    *attribs = L4_fpage::Rights::FULL();
  return true;
}
