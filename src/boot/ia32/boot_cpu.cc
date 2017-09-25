#include <cassert>
#include <cstring>
#include <cstdio>

#include "types.h"
#include "boot_cpu.h"
#include "boot_paging.h"
#include "mem_layout.h"
#include "panic.h"
#include "regdefs.h"

enum
{
  PDESHIFT		= 22,
  PDEMASK		= 0x3ff,
  PTESHIFT		= 12,
  PTEMASK		= 0x3ff,

  INTEL_PTE_VALID	= 0x00000001,
  INTEL_PTE_WRITE	= 0x00000002,
  INTEL_PTE_USER	= 0x00000004,
  INTEL_PTE_WTHRU	= 0x00000008,
  INTEL_PTE_NCACHE 	= 0x00000010,
  INTEL_PTE_REF		= 0x00000020,
  INTEL_PTE_MOD		= 0x00000040,
  INTEL_PTE_GLOBAL	= 0x00000100,
  INTEL_PTE_AVAIL	= 0x00000e00,
  INTEL_PTE_PFN		= 0xfffff000,

  INTEL_PDE_VALID	= 0x00000001,
  INTEL_PDE_WRITE	= 0x00000002,
  INTEL_PDE_USER	= 0x00000004,
  INTEL_PDE_WTHRU	= 0x00000008,
  INTEL_PDE_NCACHE 	= 0x00000010,
  INTEL_PDE_REF		= 0x00000020,
  INTEL_PDE_MOD		= 0x00000040,
  INTEL_PDE_SUPERPAGE	= 0x00000080,
  INTEL_PDE_GLOBAL	= 0x00000100,
  INTEL_PDE_AVAIL	= 0x00000e00,
  INTEL_PDE_PFN		= 0xfffff000,
  CPUF_4MB_PAGES	= 0x00000008,

  BASE_TSS		= 0x08,
  KERNEL_CS		= 0x10,
  KERNEL_DS		= 0x18,
  DBF_TSS		= 0x20,

  ACC_TSS		= 0x09,
  ACC_TSS_BUSY		= 0x02,
  ACC_CODE_R		= 0x1a,
  ACC_DATA_W		= 0x12,
  ACC_PL_K		= 0x00,
  ACC_P			= 0x80,
  SZ_32			= 0x4,
  SZ_16			= 0x0,
  SZ_G			= 0x8,

  GDTSZ			= (0x28/8),
  IDTSZ			= 0x14,
};

struct pseudo_descriptor
{
  Unsigned16 pad;
  Unsigned16 limit;
  Unsigned32 linear_base;
};

struct x86_desc 
{
  Unsigned16 limit_low;		/* limit 0..15 */
  Unsigned16 base_low;		/* base  0..15 */
  Unsigned8  base_med;		/* base  16..23 */
  Unsigned8  access;		/* access byte */
  Unsigned8  limit_high:4;	/* limit 16..19 */
  Unsigned8  granularity:4;	/* granularity */
  Unsigned8  base_high;		/* base 24..31 */
} __attribute__((packed));

struct x86_gate
{
  Unsigned16 offset_low;	/* offset 0..15 */
  Unsigned16 selector;
  Unsigned8  word_count;
  Unsigned8  access;
  Unsigned16 offset_high;	/* offset 16..31 */
} __attribute__((packed));

struct x86_tss
{
  Unsigned32 back_link;
  Unsigned32 esp0, ss0;
  Unsigned32 esp1, ss1;
  Unsigned32 esp2, ss2;
  Unsigned32 cr3;
  Unsigned32 eip, eflags;
  Unsigned32 eax, ecx, edx, ebx, esp, ebp, esi, edi;
  Unsigned32 es, cs, ss, ds, fs, gs;
  Unsigned32 ldt;
  Unsigned16 trace_trap;
  Unsigned16 io_bit_map_offset;
};

struct gate_init_entry
{
  Unsigned32 entrypoint;
  Unsigned16 vector;
  Unsigned16 type;
};

struct trap_state
{
  Unsigned32 gs, fs, es, ds;
  Unsigned32 edi, esi, ebp, cr2, ebx, edx, ecx, eax;
  Unsigned32 trapno, err;
  Unsigned32 eip, cs, eflags, esp, ss;
};

static Unsigned32       cpu_feature_flags;
static Address          base_pdir_pa;
static struct x86_tss   base_tss;
static struct x86_desc  base_gdt[GDTSZ];
static struct x86_gate  base_idt[IDTSZ];

static void handle_dbf(void);
static char           dbf_stack[2048];
static struct x86_tss dbf_tss =
  {
    0/*back_link*/, 
    0/*esp0*/, 0/*ss0*/, 0/*esp1*/, 0/*ss1*/, 0/*esp2*/, 0/*ss2*/, 
    0/*cr3*/,
    (Unsigned32)handle_dbf/*eip*/, 0x00000082/*eflags*/,
    0/*eax*/, 0/*ecx*/, 0/*edx*/, 0/*ebx*/,
    (Unsigned32)dbf_stack + sizeof(dbf_stack)/*esp*/,
    0/*ebp*/, 0/*esi*/, 0/*edi*/,
    KERNEL_DS/*es*/, KERNEL_CS/*cs*/, KERNEL_DS/*ss*/,
    KERNEL_DS/*ds*/, KERNEL_DS/*fs*/, KERNEL_DS/*gs*/,
    0/*ldt*/, 0/*trace_trap*/, 0x8000/*io_bit_map_offset*/
  };

extern "C" void _exit(int code) __attribute__((noreturn));

static inline Address* pdir_find_pde(Address pdir_pa, Address la)
{ return (&((Address*)pdir_pa)[(la >> PDESHIFT) & PDEMASK]); }

static inline Address* ptab_find_pte(Address ptab_pa, Address la)
{ return (&((Address*)ptab_pa)[(la >> PTESHIFT) & PTEMASK]); }

extern inline Unsigned32 get_eflags()
{ Unsigned32 efl; asm volatile("pushf ; popl %0" : "=r" (efl)); return efl; }

extern inline void set_eflags(Unsigned32 efl)
{ asm volatile("pushl %0 ; popf" : : "r" (efl) : "memory"); }

extern inline void set_ds(Unsigned16 ds)
{ asm volatile("movw %w0,%%ds" : : "r" (ds)); }

extern inline void set_es(Unsigned16 es)
{ asm volatile("movw %w0,%%es" : : "r" (es)); }

extern inline void set_fs(Unsigned16 fs)
{ asm volatile("movw %w0,%%fs" : : "r" (fs)); }

extern inline void set_gs(Unsigned16 gs)
{ asm volatile("movw %w0,%%gs" : : "r" (gs)); }

extern inline void set_ss(Unsigned16 ss)
{ asm volatile("movw %w0,%%ss" : : "r" (ss)); }

extern inline Unsigned16 get_ss()
{ Unsigned16 ss; asm volatile("movw %%ss,%w0" : "=r" (ss)); return ss; }

#define set_idt(pseudo_desc) \
 asm volatile("lidt %0" : : "m"((pseudo_desc)->limit), "m"(*pseudo_desc))

#define set_gdt(pseudo_desc) \
 asm volatile("lgdt %0" : : "m"((pseudo_desc)->limit), "m"(*pseudo_desc))

#define	set_tr(seg) \
 asm volatile("ltr %0" : : "rm"((Unsigned16)(seg)))

#define get_esp() \
 ({ Unsigned32 _temp__; \
    asm("movl %%esp, %0" : "=r" (_temp__)); _temp__; })

#define	get_cr0() \
 ({ Unsigned32 _temp__; \
    asm volatile("mov %%cr0, %0" : "=r" (_temp__)); _temp__; })

#define	get_cr2() \
 ({ Unsigned32 _temp__; \
    asm volatile("mov %%cr2, %0" : "=r" (_temp__)); _temp__; })

#define	set_cr3(value) \
 ({ Unsigned32 _temp__ = (value); \
    asm volatile("mov %0, %%cr3" : : "r" (_temp__)); })

#define get_cr4() \
 ({ Unsigned32 _temp__; \
    asm volatile("mov %%cr4, %0" : "=r" (_temp__)); _temp__; })

#define set_cr4(value) \
 ({ Unsigned32 _temp__ = (value); \
    asm volatile("mov %0, %%cr4" : : "r" (_temp__)); })

static inline void
fill_descriptor(struct x86_desc *desc, Unsigned32 base, Unsigned32 limit,
		Unsigned8 access, Unsigned8 sizebits)
{
  if (limit > 0xfffff)
    {
      limit >>= 12;
      sizebits |= SZ_G;
    }
  desc->limit_low   = limit & 0xffff;
  desc->base_low    = base & 0xffff;
  desc->base_med    = (base >> 16) & 0xff;
  desc->access      = access | ACC_P;
  desc->limit_high  = limit >> 16;
  desc->granularity = sizebits;
  desc->base_high   = base >> 24;
}

static inline void
fill_gate(unsigned vector, Unsigned32 offset,
	  Unsigned16 selector, Unsigned8 access)
{
  base_idt[vector].offset_low  = offset & 0xffff;
  base_idt[vector].selector    = selector;
  base_idt[vector].word_count  = 0;
  base_idt[vector].access      = access | ACC_P;
  base_idt[vector].offset_high = (offset >> 16) & 0xffff;
}

inline void ALWAYS_INLINE
paging_enable(Address pdir)
{
  /* Load the page directory.  */
  set_cr3(pdir);

  /* Turn on paging.  */
  asm volatile("movl  %0,%%cr0 ; jmp  1f ; 1:" : : "r" ((get_cr0() & ~0xf0000000) | CR0_PG));
}

static void
cpuid()
{
  int orig_eflags = get_eflags();

  /* Check for a dumb old 386 by trying to toggle the AC flag.  */
  set_eflags(orig_eflags ^ EFLAGS_AC);
  if ((get_eflags() ^ orig_eflags) & EFLAGS_AC)
    {
      /* It's a 486 or better.  Now try toggling the ID flag.  */
      set_eflags(orig_eflags ^ EFLAGS_ID);
      if ((get_eflags() ^ orig_eflags) & EFLAGS_ID)
	{
	  int highest_val, dummy;
	  asm volatile("cpuid" 
			: "=a" (highest_val)
			: "a" (0) : "ebx", "ecx", "edx");

	  if (highest_val >= 1)
	    {
	      asm volatile("cpuid"
		           : "=a" (dummy),
      			     "=d" (cpu_feature_flags)
			     : "a" (1)
			     : "ebx", "ecx");
	    }
	}
    }

  set_eflags(orig_eflags);
}

extern "C" struct gate_init_entry boot_idt_inittab[];
static void
base_idt_init(void)
{
  const struct gate_init_entry *src = boot_idt_inittab;

  while (src->entrypoint)
    {
      if ((src->type & 0x1f) == 0x05)
	// task gate
	fill_gate(src->vector, 0, src->entrypoint, src->type);
      else
	// interrupt gate
	fill_gate(src->vector, src->entrypoint, KERNEL_CS, src->type);
      src++;
    }
}

static void
base_gdt_init(void)
{
  /* Initialize the base TSS descriptor.  */
  fill_descriptor(&base_gdt[BASE_TSS / 8],
		  (Address)&base_tss, sizeof(base_tss) - 1,
	  	  ACC_PL_K | ACC_TSS, 0);
  /* Initialize the TSS descriptor for the double fault handler */
  fill_descriptor(&base_gdt[DBF_TSS / 8],
		  (Address)&dbf_tss, sizeof(dbf_tss) - 1,
		  ACC_PL_K | ACC_TSS, 0);
  /* Initialize the 32-bit kernel code and data segment descriptors
     to point to the base of the kernel linear space region.  */
  fill_descriptor(&base_gdt[KERNEL_CS / 8], 0, 0xffffffff,
	  	  ACC_PL_K | ACC_CODE_R, SZ_32);
  fill_descriptor(&base_gdt[KERNEL_DS / 8], 0, 0xffffffff,
	  	  ACC_PL_K | ACC_DATA_W, SZ_32);
}

static void
base_tss_init(void)
{
  base_tss.ss0 = KERNEL_DS;
  base_tss.esp0 = get_esp(); /* only temporary */
  base_tss.io_bit_map_offset = sizeof(base_tss);
}

static void
base_gdt_load(void)
{
  struct pseudo_descriptor pdesc;

  /* Create a pseudo-descriptor describing the GDT.  */
  pdesc.limit = sizeof(base_gdt) - 1;
  pdesc.linear_base = (Address)&base_gdt;

  /* Load it into the CPU.  */
  set_gdt(&pdesc);

  /* Reload all the segment registers from the new GDT. */
  asm volatile("ljmp  %0,$1f ;  1:" : : "i" (KERNEL_CS));
  set_ds(KERNEL_DS);
  set_es(KERNEL_DS);
  set_ss(KERNEL_DS);
  set_fs(0);
  set_gs(0);
}

static void
base_idt_load(void)
{
  struct pseudo_descriptor pdesc;

  /* Create a pseudo-descriptor describing the GDT.  */
  pdesc.limit = sizeof(base_idt) - 1;
  pdesc.linear_base = (Address)&base_idt;
  set_idt(&pdesc);
}

static void
base_tss_load(void)
{
  /* Make sure the TSS isn't marked busy.  */
  base_gdt[BASE_TSS / 8].access &= ~ACC_TSS_BUSY;
  set_tr(BASE_TSS);
}

void
base_cpu_setup(void)
{
  cpuid();
  base_idt_init();
  base_gdt_init();
  base_tss_init();
  // force tables to memory before loading segment registers
  asm volatile ("" : : : "memory");
  base_gdt_load();
  base_idt_load();
  base_tss_load();
}

static void
ptab_alloc(Address *out_ptab_pa)
{
  static char pool[140<<12] __attribute__((aligned(4096)));
  static Address pdirs;
  static int initialized;

  if (! initialized)
    {
      initialized = 1;
      memset(pool, 0, sizeof(pool));
      pdirs = round_page((Address)pool);
    }

  if (pdirs >= (Address)pool + sizeof(pool))
    panic("Cannot allocate page table -- increase ptab_alloc::pool");

  *out_ptab_pa = pdirs;
  pdirs += PAGE_SIZE;
}

static void
pdir_map_range(Address pdir_pa, Address la, Address pa,
	       Unsigned32 size, Unsigned32 mapping_bits)
{
  while (size > 0)
    {
      Address *pde = pdir_find_pde(pdir_pa, la);

      /* Use a 4MB page if we can.  */
      if (superpage_aligned(la) && superpage_aligned(pa)
	  && (size >= SUPERPAGE_SIZE)
	  && (cpu_feature_flags & CPUF_4MB_PAGES))
	{
	  /* a failed assertion here may indicate a memory wrap
	     around problem */
	  assert(!(*pde & INTEL_PDE_VALID));
	  /* XXX what if an empty page table exists
	     from previous finer-granularity mappings? */
	  *pde = pa | mapping_bits | INTEL_PDE_SUPERPAGE;
	  la += SUPERPAGE_SIZE;
	  pa += SUPERPAGE_SIZE;
	  size -= SUPERPAGE_SIZE;
	}
      else
	{
	  Address *pte;

	  /* Find the page table, creating one if necessary.  */
	  if (!(*pde & INTEL_PDE_VALID))
	    {
	      Address ptab_pa;

	      /* Allocate a new page table.  */
	      ptab_alloc(&ptab_pa);

	      /* Set the pde to point to it.  */
	      *pde = (ptab_pa & INTEL_PTE_PFN)
		   | INTEL_PDE_VALID | INTEL_PDE_USER | INTEL_PDE_WRITE;
	    }
	  assert(!(*pde & INTEL_PDE_SUPERPAGE));
	  pte = ptab_find_pte(*pde & INTEL_PDE_PFN, la);

	  /* Use normal 4KB page mappings.  */
	  do
	    {
	      assert(!(*pte & INTEL_PTE_VALID));

	      /* Insert the mapping.  */
	      *pte = pa | mapping_bits;

	      /* Advance to the next page.  */
	      pte++;
	      la += PAGE_SIZE;
	      pa += PAGE_SIZE;
	      size -= PAGE_SIZE;
	    }
	  while ((size > 0) && !superpage_aligned(la));
	}
    }
}

void
base_paging_init(void)
{
  // We assume that we only have to map the first 4MB page. This has
  // to be checked before base_paging_init was called.
  ptab_alloc(&base_pdir_pa);

  // Establish one-to-one mapping for the first 4MB of physical memory
  pdir_map_range(base_pdir_pa, /*virt*/0, /*phys*/0, /*size*/4 << 20,
		 INTEL_PDE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);

  // Enable superpage support if we have it
  if (cpu_feature_flags & CPUF_4MB_PAGES)
    set_cr4(get_cr4() | CR4_PSE);

  dbf_tss.cr3 = base_pdir_pa;

  // map in the Kernel image (superpage) of physical memory to 0xf0000000
  pdir_map_range(base_pdir_pa, /*virt*/Mem_layout::Kernel_image,
                 Mem_layout::Kernel_image_phys,
		 /*size*/Mem_layout::Kernel_image_end -
			 Mem_layout::Kernel_image,
		 INTEL_PDE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);

  // Adapter memory is already contrained in the kernel-image mapping
  if (!Mem_layout::Adap_in_kernel_image)
    {
      // map in the adapter memory (superpage) of physical memory to ...
      pdir_map_range(base_pdir_pa, /*virt*/Mem_layout::Adap_image,
                     /*phys*/Mem_layout::Adap_image_phys,
                     /*size*/Config::SUPERPAGE_SIZE,
                     INTEL_PDE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);
    }

  // Turn on paging
  paging_enable(base_pdir_pa);
}

void
base_map_physical_memory_for_kernel()
{
  unsigned long sz = Mem_layout::pmem_size;
  // map in the last 60MB of physical memory to 0xfc400000
  pdir_map_range(base_pdir_pa,
                 /*virt*/Mem_layout::Physmem,
                 /*phys*/Mem_layout::pmem_to_phys(Mem_layout::Physmem),
		 /*size*/sz,
		 INTEL_PDE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);
}

static char const * const base_regs =
  "\n"
  "EAX %08x EBX %08x ECX %08x EDX %08x\n"
  "ESI %08x EDI %08x EBP %08x ESP %08x\n"
  "EIP %08x EFLAGS %08x\n"
  "CS %04x SS %04x DS %04x ES %04x FS %04x GS %04x\n";

extern "C" FIASCO_FASTCALL
void
trap_dump_panic(const struct trap_state *st)
{
  int from_user = (st->cs & 3);
  int i;

  printf(base_regs,
         st->eax, st->ebx, st->ecx, st->edx,
       	 st->esi, st->edi, st->ebp, from_user ? st->esp : (Unsigned32)&st->esp,
	 st->eip, st->eflags,
	 st->cs & 0xffff, from_user ? st->ss & 0xffff : get_ss(),
	 st->ds & 0xffff, st->es & 0xffff, st->fs & 0xffff, st->gs & 0xffff);
  printf("trapno %u, error %08x, from %s mode\n",
         st->trapno, st->err, from_user ? "user" : "kernel");
  if (st->trapno == 0x0d)
    {
      printf("(%sternal event regarding ", st->err & 1 ? "ex" : "in");
      if (st->err & 2)
	printf("IDT gate descriptor");
      else
	printf("%cDT entry", st->err & 4 ? 'L' : 'G');
      printf(" no. 0x%02x)\n", st->err >> 3);
    }
  else if (st->trapno == 0x0e)
    printf("page fault linear address %08x\n", get_cr2());

  if (!from_user)
    {
      for (i = 0; i < 32; i++)
	printf("%08x%c", (&st->esp)[i], ((i & 7) == 7) ? '\n' : ' ');
    }
  panic("Unexpected trap while booting Fiasco!");
}

static void
handle_dbf()
{
  printf(base_regs,
	 base_tss.eax, base_tss.ebx, base_tss.ecx, base_tss.edx,
	 base_tss.esi, base_tss.edi, base_tss.ebp, base_tss.esp,
	 base_tss.eip, base_tss.eflags,
	 base_tss.cs & 0xffff, base_tss.ss & 0xffff, base_tss.ds & 0xffff,
	 base_tss.es & 0xffff, base_tss.fs & 0xffff, base_tss.gs & 0xffff);

  panic("Unexpected DOUBLE FAULT while booting Fiasco!");
}
