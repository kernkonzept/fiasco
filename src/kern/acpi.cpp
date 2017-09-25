INTERFACE:

#include <types.h>

class Acpi_gas
{
public:
  enum Type { System_mem = 0, System_io = 1, Pci_cfg_mem = 2 };
  Unsigned8  id;
  Unsigned8  width;
  Unsigned8  offset;
  Unsigned8  access_size;
  Unsigned64 addr;
} __attribute__((packed));



class Acpi_table_head
{
public:
  char       signature[4];
  Unsigned32 len;
  Unsigned8  rev;
  Unsigned8  chk_sum;
  char       oem_id[6];
  char       oem_tid[8];
  Unsigned32 oem_rev;
  Unsigned32 creator_id;
  Unsigned32 creator_rev;

  bool checksum_ok() const;
} __attribute__((packed));


class Acpi_sdt
{
private:
  unsigned _num_tables;
  Acpi_table_head const **_tables;
};

class Acpi
{
public:
  static Acpi_sdt const *sdt() { return &_sdt; }

private:
  static Acpi_sdt _sdt;
  static bool _init_done;
};

class Acpi_madt : public Acpi_table_head
{
public:
  enum Type
  { LAPIC, IOAPIC, Irq_src_ovr, NMI, LAPIC_NMI, LAPIC_adr_ovr, IOSAPIC,
    LSAPIC, Irq_src };

  struct Apic_head
  {
    Unsigned8 type;
    Unsigned8 len;
  } __attribute__((packed));

  struct Io_apic : public Apic_head
  {
    enum { ID = IOAPIC };
    Unsigned8 id;
    Unsigned8 res;
    Unsigned32 adr;
    Unsigned32 irq_base;
  } __attribute__((packed));

  struct Irq_source : public Apic_head
  {
    enum { ID = Irq_src_ovr };
    Unsigned8  bus;
    Unsigned8  src;
    Unsigned32 irq;
    Unsigned16 flags;
  } __attribute__((packed));

public:
  Unsigned32 local_apic;
  Unsigned32 apic_flags;

private:
  char data[0];
} __attribute__((packed));

template< bool >
struct Acpi_helper_get_msb
{ template<typename P> static Address msb(P) { return 0; } };

template<>
struct Acpi_helper_get_msb<true>
{ template<typename P> static Address msb(P p) { return p >> (sizeof(Address) * 8); } };


IMPLEMENTATION:

#include "boot_alloc.h"
#include "kmem.h"
#include "warn.h"
#include <cctype>

Acpi_sdt Acpi::_sdt;
bool Acpi::_init_done;

template< typename T >
class Acpi_sdt_p : public Acpi_table_head
{
public:
  T ptrs[0];

} __attribute__((packed));

typedef Acpi_sdt_p<Unsigned32> Acpi_rsdt_p;
typedef Acpi_sdt_p<Unsigned64> Acpi_xsdt_p;

class Acpi_rsdp
{
public:
  char       signature[8];
  Unsigned8  chk_sum;
  char       oem[6];
  Unsigned8  rev;
  Unsigned32 rsdt_phys;
  Unsigned32 len;
  Unsigned64 xsdt_phys;
  Unsigned8  ext_chk_sum;
  char       reserved[3];

  Acpi_rsdt_p const *rsdt() const;
  Acpi_xsdt_p const *xsdt() const;

  bool checksum_ok() const;

  static Acpi_rsdp const *locate();
} __attribute__((packed));


static void
print_acpi_id(char const *id, int len)
{
  char ID[len];
  for (int i = 0; i < len; ++i)
    ID[i] = isalnum(id[i]) ? id[i] : '.';
  printf("%.*s", len, ID);
}

PUBLIC void
Acpi_rsdp::print_info() const
{
  printf("ACPI: RSDP[%p]\tr%02x OEM:", this, (unsigned)rev);
  print_acpi_id(oem, 6);
  printf("\n");
}

PUBLIC void
Acpi_table_head::print_info() const
{
  printf("ACPI: ");
  print_acpi_id(signature, 4);
  printf("[%p]\tr%02x OEM:", this, (unsigned)rev);
  print_acpi_id(oem_id, 6);
  printf(" OEMTID:");
  print_acpi_id(oem_tid, 8);
  printf("\n");
}

PUBLIC
void
Acpi_sdt::print_summary() const
{
  for (unsigned i = 0; i < _num_tables; ++i)
    if (_tables[i])
      _tables[i]->print_info();
}

PUBLIC template< typename T >
unsigned
Acpi_sdt_p<T>::entries() const
{ return (len - sizeof(Acpi_table_head)) / sizeof(ptrs[0]); }

PUBLIC template< typename SDT >
void
Acpi_sdt::init(SDT *sdt)
{
  unsigned entries = sdt->entries();
  _tables = (Acpi_table_head const **)Boot_alloced::alloc(sizeof(*_tables) * entries);
  if (_tables)
    {
      _num_tables = entries;
      for (unsigned i = 0; i < entries; ++i)
        if (sdt->ptrs[i])
          _tables[i] = this->map_entry(i, sdt->ptrs[i]);
        else
          _tables[i] = 0;
    }
}

PRIVATE static
void *
Acpi::_map_table_head(Unsigned64 phys)
{
  // is the acpi address bigger that our handled physical addresses
  if (Acpi_helper_get_msb<(sizeof(phys) > sizeof(Address))>::msb(phys))
    {
      printf("ACPI: cannot map phys address %llx, out of range (%ubit)\n",
             (unsigned long long)phys, (unsigned)sizeof(Address) * 8);
      return 0;
    }

  void *t = Kmem::dev_map.map((void*)phys);
  if (t == (void *)~0UL)
    {
      printf("ACPI: cannot map phys address %llx, map failed\n",
             (unsigned long long)phys);
      return 0;
    }

  return t;
}

PUBLIC static
bool
Acpi::check_signature(char const *sig, char const *reference)
{
  for (; *reference; ++sig, ++reference)
    if (*reference != *sig)
      return false;

  return true;
}

PUBLIC static template<typename TAB>
TAB *
Acpi::map_table_head(Unsigned64 phys)
{ return reinterpret_cast<TAB *>(_map_table_head(phys)); }


PRIVATE template< typename T >
Acpi_table_head const *
Acpi_sdt::map_entry(unsigned idx, T phys)
{
  if (idx >= _num_tables)
    {
      printf("ACPI: table index out of range (%u >= %u)\n", idx, _num_tables);
      return 0;
    }

  return Acpi::map_table_head<Acpi_table_head>((Unsigned64)phys);
}



PUBLIC static
void
Acpi::init_virt()
{
  enum { Print_info = 0 };

  if (_init_done)
    return;
  _init_done = 1;

  if (Print_info)
    printf("ACPI-Init\n");

  Acpi_rsdp const *rsdp = Acpi_rsdp::locate();
  if (!rsdp)
    {
      WARN("ACPI: Could not find RSDP, skip init\n");
      return;
    }

  rsdp->print_info();

  if (rsdp->rev && rsdp->xsdt_phys)
    {
      Acpi_xsdt_p const *x = Kmem::dev_map.map((const Acpi_xsdt_p *)rsdp->xsdt_phys);
      if (x == (Acpi_xsdt_p const *)~0UL)
        WARN("ACPI: Could not map XSDT\n");
      else if (!x->checksum_ok())
        WARN("ACPI: Checksum mismatch in XSDT\n");
      else
        {
          _sdt.init(x);
          if (Print_info)
            {
              x->print_info();
              _sdt.print_summary();
            }
          return;
        }
    }

  if (rsdp->rsdt_phys)
    {
      Acpi_rsdt_p const *r = Kmem::dev_map.map((const Acpi_rsdt_p *)(unsigned long)rsdp->rsdt_phys);
      if (r == (Acpi_rsdt_p const *)~0UL)
        WARN("ACPI: Could not map RSDT\n");
      else if (!r->checksum_ok())
        WARN("ACPI: Checksum mismatch in RSDT\n");
      else
        {
          _sdt.init(r);
          if (Print_info)
            {
              r->print_info();
              _sdt.print_summary();
            }
          return;
        }
    }
}

PUBLIC static
template< typename T >
T
Acpi::find(const char *s)
{
  init_virt();
  return static_cast<T>(sdt()->find(s));
}

IMPLEMENT
Acpi_rsdt_p const *
Acpi_rsdp::rsdt() const
{
  return (Acpi_rsdt_p const*)(unsigned long)rsdt_phys;
}

IMPLEMENT
Acpi_xsdt_p const *
Acpi_rsdp::xsdt() const
{
  if (rev == 0)
    return 0;
  return (Acpi_xsdt_p const*)xsdt_phys;
}

IMPLEMENT
bool
Acpi_rsdp::checksum_ok() const
{
  // ACPI 1.0 checksum
  Unsigned8 sum = 0;
  for (unsigned i = 0; i < 20; i++)
    sum += *((Unsigned8 *)this + i);

  if (sum)
    return false;

  if (rev == 0)
    return true;

  // Extended Checksum
  for (unsigned i = 0; i < len && i < 4096; ++i)
    sum += *((Unsigned8 *)this + i);

  return !sum;
}

IMPLEMENT
bool
Acpi_table_head::checksum_ok() const
{
  Unsigned8 sum = 0;
  for (unsigned i = 0; i < len && i < 4096; ++i)
    sum += *((Unsigned8 *)this + i);

  return !sum;
}

PUBLIC
Acpi_table_head const *
Acpi_sdt::find(char const *sig) const
{
  for (unsigned i = 0; i < _num_tables; ++i)
    {
      Acpi_table_head const *t = _tables[i];
      if (!t)
	continue;

      if (Acpi::check_signature(t->signature, sig)
          && t->checksum_ok())
	return t;
    }

  return 0;
}

PUBLIC
template< typename T >
Acpi_table_head const *
Acpi_sdt_p<T>::find(char const *sig) const
{
  for (unsigned i = 0; i < ((len-sizeof(Acpi_table_head))/sizeof(ptrs[0])); ++i)
    {
      Acpi_table_head const *t = Kmem::dev_map.map((Acpi_table_head const*)ptrs[i]);
      if (t == (Acpi_table_head const *)~0UL)
	continue;

      if (Acpi::check_signature(t->signature, sig)
          && t->checksum_ok())
	return t;
    }

  return 0;
}

PUBLIC
Acpi_madt::Apic_head const *
Acpi_madt::find(Unsigned8 type, int idx) const
{
  for (unsigned i = 0; i < len-sizeof(Acpi_madt);)
    {
      Apic_head const *a = (Apic_head const *)(data + i);
      //printf("a=%p, a->type=%u, a->len=%u\n", a, a->type, a->len);
      if (a->type == type)
	{
	  if (!idx)
	    return a;
	  --idx;
	}
      i += a->len;
    }

  return 0;
}

PUBLIC template<typename T> inline
T const *
Acpi_madt::find(int idx) const
{
  return static_cast<T const *>(find(T::ID, idx));
}


// ------------------------------------------------------------------------
IMPLEMENTATION [ia32,amd64]:

PRIVATE static
Acpi_rsdp const *
Acpi_rsdp::locate_in_region(Address start, Address end)
{
  for (Address p = start; p < end; p += 16)
    {
      Acpi_rsdp const* r = (Acpi_rsdp const *)p;
      if (Acpi::check_signature(r->signature, "RSD PTR ")
          && r->checksum_ok())
        return r;
    }

  return 0;
}

IMPLEMENT
Acpi_rsdp const *
Acpi_rsdp::locate()
{
  enum
  {
    ACPI20_PC99_RSDP_START = 0x0e0000,
    ACPI20_PC99_RSDP_END   = 0x100000,

    BDA_EBDA_SEGMENT       = 0x00040E,
  };

  // If we are booted from UEFI, bootstrap reads the RDSP pointer from
  // UEFI and creates a memory descriptor with sub type 5 for it
  for (auto const &md: Kip::k()->mem_descs_a())
    if (   md.type() == Mem_desc::Info
        && md.ext_type() == Mem_desc::Info_acpi_rsdp)
      {
        Acpi_rsdp const *r = Acpi::map_table_head<Acpi_rsdp>(md.start());
        if (   Acpi::check_signature(r->signature, "RSD PTR ")
            && r->checksum_ok())
          return r;
        else
          panic("RSDP memory descriptor from bootstrap invalid");
      }

  if (Acpi_rsdp const *r = locate_in_region(ACPI20_PC99_RSDP_START,
                                            ACPI20_PC99_RSDP_END))
    return r;

  Address ebda = *(Unsigned16 *)BDA_EBDA_SEGMENT << 4;
  if (Acpi_rsdp const *r = locate_in_region(ebda, ebda + 1024))
    return r;

  return 0;
}
