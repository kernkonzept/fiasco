INTERFACE[ia32,amd64]:

#include "types.h"

EXTENSION class Boot_info
{
private:
  static unsigned _checksum_ro;
  static unsigned _checksum_rw;
};

//-----------------------------------------------------------------------------
IMPLEMENTATION[ia32,amd64]:

#include <cassert>
#include <cstring>
#include <cstdlib>
#include "checksum.h"
#include "mem_layout.h"

// these members needs to be initialized with some
// data to go into the data section and not into bss
unsigned Boot_info::_checksum_ro   = 15;
unsigned Boot_info::_checksum_rw   = 16;


/// \defgroup pre init setup
/**
 * The Boot_info object must be set up with these functions
 * before Boot_info::init() is called!
 * This can be done either in __main, if booted on hardware
 * or in an initializer with a higher priority than BOOT_INFO_INIT_PRIO
 * (e.g UX_STARTUP1_INIT_PRIO) if the kernel runs on software (FIASCO-UX)
 */
//@{

PUBLIC inline static
void Boot_info::set_checksum_ro(unsigned ro_cs)
{  _checksum_ro = ro_cs; }

PUBLIC inline static
void Boot_info::set_checksum_rw(unsigned rw_cs)
{  _checksum_rw = rw_cs; }
//@}


IMPLEMENT
void
Boot_info::init()
{}

PUBLIC inline static
unsigned
Boot_info::get_checksum_ro(void)
{
  return _checksum_ro;
}

PUBLIC inline static
unsigned
Boot_info::get_checksum_rw(void)
{
  return _checksum_rw;
}

PUBLIC static
void
Boot_info::reset_checksum_ro(void)
{
  set_checksum_ro(Checksum::get_checksum_ro());
}

