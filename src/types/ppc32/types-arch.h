#pragma once

#define L4_PTR_FMT             "%08lx"
#define L4_MWORD_FMT           "%08lx"
#define L4_X64_FMT             "%016llx"
#define L4_ADDR_INPUT_FMT      "%8x"
#define L4_FRAME_INPUT_FMT     "%5x"

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

static constexpr unsigned MWORD_BITS = 32;
static constexpr unsigned ARCH_PAGE_SHIFT = 12;

typedef signed char Small_atomic_int;

/// (virtual or physical address) should be addr_t or something
typedef unsigned long int      Address;
enum Address_vals : Address
{
  Invalid_address = ~0UL
};

typedef Unsigned64 Cpu_time;

#include <cxx/cxx_int>

typedef cxx::int_type<unsigned char, struct Cpu_phys_id_t> Cpu_phys_id;
