#pragma once

#include "globalconfig.h"

#define L4_PTR_FMT             "%016lx"
#define L4_MWORD_FMT           "%016lx"
#define L4_X64_FMT             "%016llx"
#define L4_ADDR_INPUT_FMT      "%16lx"
#define L4_FRAME_INPUT_FMT     "%10lx"

/// standard fixed-width types
typedef unsigned char          Unsigned8;
typedef signed char            Signed8;
typedef unsigned short         Unsigned16;
typedef signed short           Signed16;
typedef unsigned int           Unsigned32;
typedef signed int             Signed32;
typedef unsigned long long int Unsigned64;
typedef signed long long int   Signed64;

/// machine word
typedef signed long int        Smword;
typedef unsigned long int      Mword;

static constexpr unsigned MWORD_BITS = 64;
#if defined CONFIG_MIPS_PAGE_SIZE_16K
static constexpr unsigned ARCH_PAGE_SHIFT = 14;
#elif defined CONFIG_MIPS_PAGE_SIZE_4K
static constexpr unsigned ARCH_PAGE_SHIFT = 12;
#else
#  error "MIPS page size must be 16 KiB or 4 KiB"
#endif

typedef Signed32 Small_atomic_int;

/// (virtual or physical address) should be addr_t or something
typedef unsigned long int     Address;
enum Address_vals : Address
{
  Invalid_address = ~0UL
};

typedef Unsigned64 Cpu_time;

#include <cxx/cxx_int>

typedef cxx::int_type<unsigned short, struct Cpu_phys_id_t> Cpu_phys_id;
