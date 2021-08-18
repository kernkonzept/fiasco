INTERFACE [riscv]:

#include "types.h"

#define DEF_CSR(id) ~, id
#define _TO_CSR2(a, b, ...) #b
#define _TO_CSR1(name, ...) _TO_CSR2(__VA_ARGS__, name)
#define TO_CSR(name) _TO_CSR1(name, CSR_ ## name)

// CSRs defined by the hypervisor extension (gcc does not yet support them)
#define CSR_vsstatus DEF_CSR(0x200)
#define CSR_vsie DEF_CSR(0x204)
#define CSR_vstvec DEF_CSR(0x205)
#define CSR_vsscratch DEF_CSR(0x240)
#define CSR_vsepc DEF_CSR(0x241)
#define CSR_vscause DEF_CSR(0x242)
#define CSR_vstval DEF_CSR(0x243)
#define CSR_vsip DEF_CSR(0x244)
#define CSR_vsatp DEF_CSR(0x280)
#define CSR_hstatus DEF_CSR(0x600)
#define CSR_hedeleg DEF_CSR(0x602)
#define CSR_hideleg DEF_CSR(0x603)
#define CSR_hie DEF_CSR(0x604)
#define CSR_htimedelta DEF_CSR(0x605)
#define CSR_hcounteren DEF_CSR(0x606)
#define CSR_hgeie DEF_CSR(0x607)
#define CSR_henvcfg DEF_CSR(0x60A)
#define CSR_htimedeltah DEF_CSR(0x615)
#define CSR_henvcfgh DEF_CSR(0x61A)
#define CSR_htval DEF_CSR(0x643)
#define CSR_hip DEF_CSR(0x644)
#define CSR_hvip DEF_CSR(0x645)
#define CSR_htinst DEF_CSR(0x64A)
#define CSR_hgatp DEF_CSR(0x680)
#define CSR_hgeip DEF_CSR(0xE12)

#define csr_write(csr, val) \
do { \
  Mword _val = static_cast<Mword>(val); \
  __asm__ __volatile__ ("csrw " TO_CSR(csr) ", %0" : : "rK" (_val) : "memory"); \
} while (0)

#define csr_set(csr, bits) \
do { \
  Mword _bits = static_cast<Mword>(bits); \
  __asm__ __volatile__ ("csrs " TO_CSR(csr) ", %0" : : "rK" (_bits) : "memory"); \
} while (0)

#define csr_clear(csr, bits) \
do { \
  Mword _bits = static_cast<Mword>(bits); \
  __asm__ __volatile__ ("csrc " TO_CSR(csr) ", %0" : : "rK" (_bits) : "memory"); \
} while (0)

#define csr_read(csr) \
({ \
  Mword val; \
  __asm__ __volatile__ ("csrr %0, " TO_CSR(csr) : "=r" (val) : : "memory"); \
  val; \
})

#define csr_read_write(csr, val) \
({ \
  Mword _val = static_cast<Mword>(val); \
  Mword ret; \
  __asm__ __volatile__ ("csrrw %0, " TO_CSR(csr) ", %1" \
                        : "=r" (ret) : "rK" (_val) : "memory"); \
  ret; \
})

#define csr_read_set(csr, bits) \
({ \
  Mword _bits = static_cast<Mword>(bits); \
  Mword ret; \
  __asm__ __volatile__ ("csrrs %0, " TO_CSR(csr) ", %1" \
                        : "=r" (ret) : "rK" (_bits) : "memory"); \
  ret; \
})

#define csr_read_clear(csr, bits) \
({ \
  Mword _bits = static_cast<Mword>(bits); \
  Mword ret; \
  __asm__ __volatile__ ("csrrc %0, " TO_CSR(csr) ", %1" \
                        : "=r" (ret) : "rK" (_bits) : "memory"); \
  ret; \
})

// ----------------------------------------------------------------------
INTERFACE [riscv && 32bit]:

// Non-atomically read 64-bit sized CSR pair (<csr> and <csr>h).
#define csr_read64(csr) (csr_read(csr) | Unsigned64{csr_read(csr ## h)} << 32)

// Non-atomically write 64-bit sized CSR pair (<csr> and <csr>h).
#define csr_write64(csr, val) \
  do { \
    csr_write(csr, val); \
    csr_write(csr ## h, val >> 32); \
  } while (0)


// ----------------------------------------------------------------------
INTERFACE [riscv && 64bit]:

#define csr_read64(csr) csr_read(csr)
#define csr_write64(csr, val) csr_write(csr, val)
