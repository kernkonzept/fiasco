IMPLEMENTATION [mips32]:

IMPLEMENT inline
bool
Kmem::is_kmem_page_fault(Mword pfa, Mword /*cause*/)
{ return pfa >= 0x80000000; }

IMPLEMENTATION [mips64]:

IMPLEMENT inline
bool
Kmem::is_kmem_page_fault(Mword pfa, Mword /*cause*/)
{ return pfa >= 0x8000000000000000; }

IMPLEMENTATION [mips]:

// currently a dummy, the kernel runs unpaged
DEFINE_GLOBAL_CONSTINIT Global_data<Kpdir *> Kmem::kdir;
