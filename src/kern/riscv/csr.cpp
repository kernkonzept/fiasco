INTERFACE [riscv]:

#include "types.h"

#define csr_write(csr, val) \
do { \
  Mword _val = static_cast<Mword>(val); \
  __asm__ __volatile__ ("csrw " #csr ", %0" : : "rK" (_val) : "memory"); \
} while (0)

#define csr_set(csr, bits) \
do { \
  Mword _bits = static_cast<Mword>(bits); \
  __asm__ __volatile__ ("csrs " #csr ", %0" : : "rK" (_bits) : "memory"); \
} while (0)

#define csr_clear(csr, bits) \
do { \
  Mword _bits = static_cast<Mword>(bits); \
  __asm__ __volatile__ ("csrc " #csr ", %0" : : "rK" (_bits) : "memory"); \
} while (0)

#define csr_read(csr) \
({ \
  Mword val; \
  __asm__ __volatile__ ("csrr %0, " #csr : "=r" (val) : : "memory"); \
  val; \
})

#define csr_read_write(csr, val) \
({ \
  Mword _val = static_cast<Mword>(val); \
  Mword ret; \
  __asm__ __volatile__ ("csrrw %0, " #csr ", %1" \
                        : "=r" (ret) : "rK" (_val) : "memory"); \
  ret; \
})

#define csr_read_set(csr, bits) \
({ \
  Mword _bits = static_cast<Mword>(bits); \
  Mword ret; \
  __asm__ __volatile__ ("csrrs %0, " #csr ", %1" \
                        : "=r" (ret) : "rK" (_bits) : "memory"); \
  ret; \
})

#define csr_read_clear(csr, bits) \
({ \
  Mword _bits = static_cast<Mword>(bits); \
  Mword ret; \
  __asm__ __volatile__ ("csrrc %0, " #csr ", %1" \
                        : "=r" (ret) : "rK" (_bits) : "memory"); \
  ret; \
})
