#include <cassert>
#include <cstring>
#include <cstdio>

#include "types.h"
#include "boot_cpu.h"
#include "boot_paging.h"
#include "mem_layout.h"
#include "processor.h"
#include "regdefs.h"

enum
{
  PML4ESHIFT		= 38,
  PML4EMASK		= 0x1ff,
  PDPESHIFT		= 30,
  PDPEMASK		= 0x1ff,
  PDESHIFT		= 21,
  PDEMASK		= 0x1ff,
  PTESHIFT		= 12,
  PTEMASK		= 0x1ff,

  INTEL_PTE_VALID	= 0x0000000000000001LL,
  INTEL_PTE_WRITE	= 0x0000000000000002LL,
  INTEL_PTE_USER	= 0x0000000000000004LL,
  INTEL_PTE_WTHRU	= 0x00000008,
  INTEL_PTE_NCACHE 	= 0x00000010,
  INTEL_PTE_REF		= 0x00000020,
  INTEL_PTE_MOD		= 0x00000040,
  INTEL_PTE_GLOBAL	= 0x00000100,
  INTEL_PTE_AVAIL	= 0x00000e00,
  INTEL_PTE_PFN		= 0x000ffffffffff000LL,

  INTEL_PDE_VALID	= 0x0000000000000001LL,
  INTEL_PDE_WRITE	= 0x0000000000000002LL,
  INTEL_PDE_USER	= 0x0000000000000004LL,
  INTEL_PDE_WTHRU	= 0x00000008,
  INTEL_PDE_NCACHE 	= 0x00000010,
  INTEL_PDE_REF		= 0x00000020,
  INTEL_PDE_MOD		= 0x00000040,
  INTEL_PDE_SUPERPAGE	= 0x0000000000000080LL,
  INTEL_PDE_GLOBAL	= 0x00000100,
  INTEL_PDE_AVAIL	= 0x00000e00,
  INTEL_PDE_PFN		= 0x000ffffffffff000LL,

  INTEL_PDPE_VALID	= 0x0000000000000001LL,
  INTEL_PDPE_WRITE	= 0x0000000000000002LL,
  INTEL_PDPE_USER	= 0x0000000000000004LL,
  INTEL_PDPE_PFN	= 0x000ffffffffff000LL,

  INTEL_PML4E_VALID	= 0x0000000000000001LL,
  INTEL_PML4E_WRITE	= 0x0000000000000002LL,
  INTEL_PML4E_USER	= 0x0000000000000004LL,
  INTEL_PML4E_PFN	= 0x000ffffffffff000LL,
 
  CPUF_4MB_PAGES	= 0x00000008,

  BASE_TSS		= 0x08,
  KERNEL_DS		= 0x18,
  KERNEL_CS_64		= 0x20, // XXX

  DBF_TSS		= 0x28, // XXX check this value

  ACC_TSS		= 0x09,
  ACC_TSS_BUSY		= 0x02,
  ACC_CODE_R		= 0x1a,
  ACC_DATA_W		= 0x12,
  ACC_PL_K		= 0x00,
  ACC_P			= 0x80,
  SZ_32			= 0x4,
  SZ_16			= 0x0,
  SZ_G			= 0x8,
  SZ_CODE_64		= 0x2, // XXX 64 Bit Code Segment 

  GDTSZ			= (0x30/8), // XXX check this value
  IDTSZ			= 256,
};


struct pseudo_descriptor
{
  Unsigned16 pad;
  Unsigned16 limit;
  Unsigned64 linear_base;
} __attribute__((packed));

struct x86_desc 
{
  Unsigned16 limit_low;
  Unsigned16 base_low;
  Unsigned8  base_med;
  Unsigned8  access;
  Unsigned8  limit_high:4;
  Unsigned8  granularity:4;
  Unsigned8  base_high;
} __attribute__((packed));

struct idt_desc
{
  Unsigned16 offset_low0;
  Unsigned16 selector;
  Unsigned8  zero_and_ist;
  Unsigned8  access;
  Unsigned16 offset_low1;
  Unsigned32 offset_high;
  Unsigned32 ignored;
} __attribute__((packed));

struct x86_tss
{
  Unsigned32 ign0;
  Unsigned64 rsp0;
  Unsigned64 rsp1;
  Unsigned64 rsp2;
  Unsigned32 ign1;
  Unsigned32 ign2;
  Unsigned64 ist1;
  Unsigned64 ist2;
  Unsigned64 ist3;
  Unsigned64 ist4;
  Unsigned64 ist5;
  Unsigned64 ist6;
  Unsigned64 ist7;
  Unsigned32 ign3;
  Unsigned32 ign4;
  Unsigned16 ign5;
  Unsigned16 io_bit_map_offset;
} __attribute__((packed));

struct idt_init_entry
{
  Unsigned64 entrypoint;
  Unsigned16 vector;
  Unsigned16 type;
} __attribute__((packed));

struct trap_state
{
  Unsigned64 rax, rbx, rcx, rdx;
  Unsigned64 rdi, rsi, rbp, cr2;
  Unsigned64 r8,  r9,  r10, r11;
  Unsigned64 r12, r13, r14, r15;
  Unsigned64 trapno, err;
  Unsigned64 rip, cs, rflags, rsp, ss;
};

static Unsigned64       cpu_feature_flags;
static Address          base_pml4_pa;
static struct x86_tss   base_tss;
static struct x86_desc  base_gdt[GDTSZ];
static struct idt_desc  base_idt[IDTSZ];

static char dbf_stack[2048];

extern "C" void _exit(int code) __attribute__((noreturn));

static inline Unsigned64* find_pml4e(Address pml4_pa, Address la)
{ return (&((Unsigned64*)pml4_pa)[(la >> PML4ESHIFT) & PML4EMASK]); }

static inline Unsigned64* find_pdpe(Address pdp_pa, Address la)
{ return (&((Unsigned64*)pdp_pa)[(la >> PDPESHIFT) & PDPEMASK]); }

static inline Unsigned64* find_pde(Address pdir_pa, Address la)
{ return (&((Unsigned64*)pdir_pa)[(la >> PDESHIFT) & PDEMASK]); }

static inline Unsigned64* find_pte(Address ptab_pa, Address la)
{ return (&((Unsigned64*)ptab_pa)[(la >> PTESHIFT) & PTEMASK]); }

extern inline Unsigned32 get_eflags()
{ Mword efl; asm volatile("pushf \n\t pop %0" : "=r" (efl)); return efl; }

extern inline void set_eflags(Mword efl)
{ asm volatile("push %0 ; popf" : : "r" (efl) : "memory"); }

extern inline void set_ds(Unsigned16 ds)
{ asm volatile("mov %w0,%%ds" : : "r" (ds)); }

extern inline void set_es(Unsigned16 es)
{ asm volatile("mov %w0,%%es" : : "r" (es)); }

extern inline void set_fs(Unsigned16 fs)
{ asm volatile("mov %w0,%%fs" : : "r" (fs)); }

extern inline void set_gs(Unsigned16 gs)
{ asm volatile("mov %w0,%%gs" : : "r" (gs)); }

extern inline void set_ss(Unsigned16 ss)
{ asm volatile("mov %w0,%%ss" : : "r" (ss)); }

extern inline Unsigned16 get_ss()
{ Unsigned16 ss; asm volatile("mov %%ss,%w0" : "=r" (ss)); return ss; }

#define set_idt(pseudo_desc) \
 asm volatile("lidt %0" : : "m"((pseudo_desc)->limit), "m"(*pseudo_desc))

#define set_gdt(pseudo_desc) \
 asm volatile("lgdt %0" : : "m"((pseudo_desc)->limit), "m"(*pseudo_desc))

#define	set_tr(seg) \
 asm volatile("ltr %0" : : "rm" ((Unsigned16)(seg)))

#define get_esp() \
 ({ Unsigned64 _temp__; \
    asm("mov %%rsp, %0" : "=r" (_temp__)); _temp__; })

#define	get_cr0() \
 ({ Unsigned64 _temp__; \
    asm volatile("mov %%cr0, %0" : "=r" (_temp__)); _temp__; })

#define	set_cr3(value) \
 ({ Unsigned64 _temp__ = (value); \
    asm volatile("mov %0, %%cr3" : : "r" (_temp__)); })

#define get_cr4() \
 ({ Unsigned64 _temp__; \
    asm volatile("mov %%cr4, %0" : "=r" (_temp__)); _temp__; })

#define set_cr4(value) \
 ({ Unsigned64 _temp__ = (value); \
    asm volatile("mov %0, %%cr4" : : "r" (_temp__)); })


extern inline void enable_longmode()
{ Proc::efer(Proc::efer() | Proc::Efer_lme_flag | Proc::Efer_nxe_flag); }

static inline void
fill_descriptor(struct x86_desc *desc, Unsigned32 base, Unsigned32 limit,
		Unsigned8 access, Unsigned8 sizebits)
{
  if (limit > 0xfffff)
    {
      limit >>= 12;
      sizebits |= SZ_G;
    }
  desc->limit_low = limit & 0xffff;
  desc->base_low = base & 0xffff;
  desc->base_med = (base >> 16) & 0xff;
  desc->access = access | ACC_P;
  desc->limit_high = limit >> 16;
  desc->granularity = sizebits;
  desc->base_high = base >> 24;
}

static inline void
fill_idt_desc(struct idt_desc *desc, Unsigned64 offset,
	  Unsigned16 selector, Unsigned8 access, Unsigned8 ist_entry)
{
  desc->offset_low0  = offset & 0x000000000000ffffLL;
  desc->selector     = selector;
  desc->zero_and_ist = ist_entry;
  desc->access       = access | ACC_P;
  desc->offset_low1  = (offset & 0x00000000ffff0000LL) >> 16;
  desc->offset_high  = (offset & 0xffffffff00000000LL) >> 32;
  desc->ignored	     = 0;
}

inline void ALWAYS_INLINE
paging_enable(Address pml4)
{
  /* Enable Physical Address Extension (PAE). */
  set_cr4(get_cr4() | CR4_PAE);

  /* Load the page map level 4.  */
  set_cr3(pml4);

  /* Enable long mode. */
  enable_longmode();

  /* Turn on paging and switch to long mode. */
  asm volatile("mov  %0,%%cr0 ; jmp  1f ; 1:" : : "r" (get_cr0() | CR0_PG));
}

static void
panic(const char *str)
{
  printf("\n%s\n", str);
  _exit(-1);
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

extern "C" struct idt_init_entry boot_idt_inittab[];
static void
base_idt_init(void)
{
  struct idt_desc *dst = base_idt;
  const struct idt_init_entry *src = boot_idt_inittab;

  while (src->entrypoint)
    {
      fill_idt_desc(&dst[src->vector], src->entrypoint, KERNEL_CS_64, 
	            src->type,(src->vector==8)?1:0);
      src++;
    }
}

static void
base_gdt_init(void)
{
  printf("base_tss @%p\n", &base_tss);
  /* Initialize the base TSS descriptor.  */
  fill_descriptor(&base_gdt[BASE_TSS / 8],
                  (Address)&base_tss, sizeof(base_tss) - 1,
                  ACC_PL_K | ACC_TSS, 0);

  memset(&base_gdt[(BASE_TSS / 8) + 1], 0, 8);

  /* Initialize the 64-bit kernel code and data segment descriptors
     to point to the base of the kernel linear space region.  */
  fill_descriptor(&base_gdt[KERNEL_CS_64 / 8], 0, 0xffffffff,
                  ACC_PL_K | ACC_CODE_R, SZ_CODE_64);

  fill_descriptor(&base_gdt[KERNEL_DS / 8], 0, 0xffffffff,
                  ACC_PL_K | ACC_DATA_W, SZ_32);

}

static void
base_tss_init(void)
{
  base_tss.rsp0 = get_esp(); /* only temporary */
  base_tss.ist1 = (Unsigned64)(dbf_stack + sizeof(dbf_stack));
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

  // XXX must be fixed 
  /* Reload all the segment registers from the new GDT. */
  asm volatile (
  "movabsq	$1f, %%rax	\n"
  "pushq	%%rbx		\n"
  "pushq	%%rax		\n"
  "lretq 			\n"
  "1: 				\n"
    :
    : "b" (KERNEL_CS_64)
    : "rax", "memory");
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
  asm volatile ("" : : : "memory");
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
  static char pool[69<<12] __attribute__((aligned(4096)));
  static Address pdirs;
  static int initialized;

  if (! initialized)
    {
      initialized = 1;
      memset(pool, 0, sizeof(pool));
      pdirs = ((Address)pool + PAGE_SIZE - 1) & ~PAGE_MASK;
    }

  if (pdirs > (Address)pool + sizeof(pool))
    panic("Cannot allocate page table -- increase ptab_alloc::pool");

  *out_ptab_pa = pdirs;
  pdirs += PAGE_SIZE;
}

static void
pdir_map_range(Address pml4_pa, Address la, Address pa,
	       Unsigned32 size, Unsigned32 mapping_bits)
{
  assert(la+size-1 > la); // avoid 4GB wrap around

  while (size > 0)
    {
      Unsigned64 *pml4e = find_pml4e(pml4_pa, la);

      /* Create new pml4e with corresponding pdp (page directory pointer)
       * if no valid entry exists. */
      if (!(*pml4e & INTEL_PML4E_VALID))
	{
	  Address pdp_pa;

	  /* Allocate new page for pdp. */
	  ptab_alloc(&pdp_pa);

	  /* Set the pml4 to point to it. */
	  *pml4e = (pdp_pa & INTEL_PML4E_PFN)
	    | INTEL_PML4E_VALID | INTEL_PML4E_USER | INTEL_PML4E_WRITE;
	}

      do
	{
	  Unsigned64 *pdpe = find_pdpe(*pml4e & INTEL_PML4E_PFN, la);

	  /* Create new pdpe with corresponding pd (page directory)
	   * if no valid entry exists. */
	  if (!(*pdpe & INTEL_PDPE_VALID))
	    {
	      Address pd_pa;

	      /* Allocate new page for pd. */
	      ptab_alloc(&pd_pa);

	      /* Set the pdpe to point to it. */
	      *pdpe = (pd_pa & INTEL_PDPE_PFN)
		| INTEL_PDPE_VALID | INTEL_PDPE_USER | INTEL_PDPE_WRITE;
	    }

	  do
	    {
	      Unsigned64 *pde = find_pde(*pdpe & INTEL_PDPE_PFN, la);

	      /* Use a 2MB page if we can.  */
	      if (superpage_aligned(la) && superpage_aligned(pa)
		  && (size >= SUPERPAGE_SIZE))
		  //&& (cpu_feature_flags & CPUF_4MB_PAGES)) XXX
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


		  /* Use normal 4KB page mappings.  */
		  do
		    {
		      Unsigned64 *pte = find_pte(*pde & INTEL_PDE_PFN, la);
		      assert(!(*pte & INTEL_PTE_VALID));

		      /* Insert the mapping.  */
		      *pte = pa | mapping_bits;

		      /* Advance to the next page.  */
		      //pte++; 
		      la += PAGE_SIZE;
		      pa += PAGE_SIZE;
		      size -= PAGE_SIZE;
		    }
		  while ((size > 0) && !superpage_aligned(la));
		}
	    }
	  while ((size > 0) && !pd_aligned(la));
	}
      while ((size > 0) && !pdp_aligned(la));
    }
}

void
base_paging_init(void)
{
  ptab_alloc(&base_pml4_pa);

  // Establish one-to-one mappings for the first 4MB of physical memory
  pdir_map_range(base_pml4_pa, /*virt*/0, /*phys*/0, /*size*/4 << 20,
		 INTEL_PDE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);

  // map in the first 4MB of physical memory to 0xfffffffff0000000
  pdir_map_range(base_pml4_pa, Mem_layout::Kernel_image,
                 Mem_layout::Kernel_image_phys,
                 Mem_layout::Kernel_image_end - Mem_layout::Kernel_image,
                 INTEL_PDE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);

  // Adapter memory needs a seperate mapping
  if (!Mem_layout::Adap_in_kernel_image)
    // map in the adapter memory (superpage) of physical memory to ...
    pdir_map_range(base_pml4_pa, /*virt*/Mem_layout::Adap_image,
                   /*phys*/Mem_layout::Adap_image_phys,
                   /*size*/Config::SUPERPAGE_SIZE,
                   INTEL_PDE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);

  // XXX Turn on paging and activate 64Bit mode
  paging_enable(base_pml4_pa);
}

void
base_map_physical_memory_for_kernel()
{
  unsigned long sz = Mem_layout::pmem_size;
  printf("map pmem %14lx-%14lx to %14lx-%14lx\n",
         Mem_layout::pmem_to_phys(Mem_layout::Physmem),
         Mem_layout::pmem_to_phys(Mem_layout::Physmem) + sz,
         Mem_layout::Physmem,
         Mem_layout::Physmem + sz);
  // map in the last 60MB of physical memory to 0xfc400000
  pdir_map_range(base_pml4_pa,
                 /*virt*/Mem_layout::Physmem,
                 /*phys*/Mem_layout::pmem_to_phys(Mem_layout::Physmem),
		 /*size*/sz,
		 INTEL_PDE_VALID | INTEL_PDE_WRITE | INTEL_PDE_USER);
}


extern "C" void
trap_dump_panic(const struct trap_state *st)
{
  int from_user = (st->cs & 3);
  int i;

  printf("RAX %016llx RBX %016llx\n", st->rax, st->rbx);
  printf("RCX %016llx RDX %016llx\n", st->rcx, st->rdx);
  printf("RSI %016llx RDI %016llx\n", st->rsi, st->rdi);
  printf("RBP %016llx RSP %016llx\n",
      st->rbp, from_user ? st->rsp : (Address)&st->rsp);
  printf("R8  %016llx R9  %016llx\n", st->r8, st->r9);
  printf("R10 %016llx R11 %016llx\n", st->r10, st->r11);
  printf("R12 %016llx R13 %016llx\n", st->r12, st->r13);
  printf("R14 %016llx R15 %016llx\n", st->r14, st->r15);
  printf("RIP %016llx RFLAGS %016llx\n", st->rip, st->rflags);
  printf("CS %04llx SS %04llx\n",
      st->cs & 0xffff, from_user ? st->ss & 0xffff : get_ss());
  printf("trapno %llu, error %08llx, from %s mode\n",
      st->trapno, st->err, from_user ? "user" : "kernel");
  if (st->trapno == 0x0d)
    {
      if (st->err & 1)
	printf("(external event");
      else
	printf("(internal event");
      if (st->err & 2)
	{
	  printf(" regarding IDT gate descriptor no. 0x%02llx)\n",
	      st->err >> 3);
	}
      else
	{
	  printf(" regarding %s entry no. 0x%02llx)\n",
	      st->err & 4 ? "LDT" : "GDT", st->err >> 3);
	}
    }
  else if (st->trapno == 0x0e)
    printf("page fault linear address %016llx\n", st->cr2);

  if (!from_user)
    {
      for (i = 0; i < 32; i++)
	printf("%016llx%c", (&st->rsp)[i], ((i & 7) == 7) ? '\n' : ' ');
    }
  panic("Unexpected trap while booting Fiasco!");
}
