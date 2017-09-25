INTERFACE:

#include "initcalls.h"

class Pit
{
  enum
  {
    Clock_tick_rate = 1193180,
  };
};


//----------------------------------------------------------------------------
IMPLEMENTATION[i8254]:

#include "io.h"

PUBLIC static inline NEEDS ["io.h"]
void
Pit::done()
{
  // set counter channel 0 to one-shot mode
  Io::out8_p(0x30, 0x43);
}

PUBLIC static FIASCO_INIT_CPU_AND_PM
void
Pit::setup_channel2_to_20hz()
{
  // set gate high, disable speaker
  Io::out8((Io::in8(0x61) & ~0x02) | 0x01, 0x61);

  // set counter channel 2 to binary, mode0, lsb/msb
  Io::out8(0xb0, 0x43);

  // set counter frequency
  const unsigned latch = Clock_tick_rate / 20; // 50ms
  Io::out8(latch & 0xff, 0x42);
  Io::out8(latch >> 8,   0x42);
}


//----------------------------------------------------------------------------
IMPLEMENTATION[i8254 & pit_timer]:

PRIVATE static
void
Pit::set_freq(unsigned freq)
{
  // set counter channel 0 to binary, mode2, lsb/msb
  Io::out8_p(0x34, 0x43);

  // set counter frequency
  const unsigned latch = Clock_tick_rate / freq;
  Io::out8_p(latch & 0xff, 0x40);
  Io::out8_p(latch >> 8,   0x40);
}

// set up timer interrupt (~ 1ms)
PUBLIC static inline NEEDS ["io.h", Pit::set_freq]
void
Pit::init()
{
  // set counter frequency to ~1000 Hz (1000.151 Hz)
  set_freq(1000);
}

PUBLIC static inline NEEDS ["io.h", Pit::set_freq]
void
Pit::init(unsigned freq)
{
  set_freq(freq);
}

PUBLIC static inline NEEDS ["io.h", Pit::set_freq]
void
Pit::set_freq_slow()
{
  set_freq(32);
}
