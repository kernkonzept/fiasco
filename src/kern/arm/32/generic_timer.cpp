INTERFACE [arm && arm_generic_timer]:

namespace Generic_timer {

  enum Timer_type { Physical, Virtual, Hyp };

  template<unsigned Type>  struct T;

  template<> struct T<Virtual>
  {
    enum { Type = Virtual, R1 = Physical, R2 = Hyp  };
    /* In non-HYP mode we use always the virtual counter and the
     * virtual timer
     */
    static Unsigned64 counter() // use the virtual counter
    { Unsigned64 v; asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r" (v)); return v; }

    static Unsigned64 compare()
    { Unsigned64 v; asm volatile("mrrc p15, 3, %Q0, %R0, c14" : "=r" (v)); return v; }

    static void compare(Unsigned64 v)
    { asm volatile("mcrr p15, 3, %Q0, %R0, c14" : : "r" (v)); }

    static Unsigned32 control()
    { Unsigned32 v; asm volatile("mrc p15, 0, %0, c14, c3, 1" : "=r" (v)); return v; }

    static void control(Unsigned32 v)
    { asm volatile("mcr p15, 0, %0, c14, c3, 1" : : "r" (v)); }

    static void setup_timer_access()
    {
      // CNTKCTL: allow access to virtual counter from PL0
      asm volatile("mcr p15, 0, %0, c14, c1, 0" : : "r"(0x2));
    }

    static Unsigned32 frequency()
    { Unsigned32 v; asm volatile ("mrc p15, 0, %0, c14, c0, 0": "=r" (v)); return v; }

    static void frequency(Unsigned32 v)
    { asm volatile ("mcr p15, 0, %0, c14, c0, 0": :"r" (v)); }
  };

  template<> struct T<Physical>
  {
    enum { Type = Physical, R1 = Hyp, R2 = Virtual  };
    /* In non-HYP mode we use always the virtual counter and the
     * virtual timer
     */
    static Unsigned64 counter() // use the virtual counter
    { Unsigned64 v; asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (v)); return v; }

    static Unsigned64 compare()
    { Unsigned64 v; asm volatile("mrrc p15, 2, %Q0, %R0, c14" : "=r" (v)); return v; }

    static void compare(Unsigned64 v)
    { asm volatile("mcrr p15, 2, %Q0, %R0, c14" : : "r" (v)); }

    static Unsigned32 control()
    { Unsigned32 v; asm volatile("mrc p15, 0, %0, c14, c2, 1" : "=r" (v)); return v; }

    static void control(Unsigned32 v)
    { asm volatile("mcr p15, 0, %0, c14, c2, 1" : : "r" (v)); }

    static void setup_timer_access()
    {
       // CNTKCTL: allow access to virtual and physical counter from PL0
      asm volatile("mcr p15, 0, %0, c14, c1, 0" : : "r"(0x3));
    }

    static Unsigned32 frequency()
    { Unsigned32 v; asm volatile ("mrc p15, 0, %0, c14, c0, 0": "=r" (v)); return v; }

    static void frequency(Unsigned32 v)
    { asm volatile ("mcr p15, 0, %0, c14, c0, 0": :"r" (v)); }
  };

  template<> struct T<Hyp>
  {
    enum { Type = Hyp, R1 = Physical, R2 = Virtual };
    /* In HYP mode we use the physical counter and the
     * HYP mode timer
     */
    static Unsigned64 counter() // use the physical counter
    { Unsigned64 v; asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (v)); return v; }

    static Unsigned64 compare()
    { Unsigned64 v; asm volatile("mrrc p15, 6, %Q0, %R0, c14" : "=r" (v)); return v; }

    static void compare(Unsigned64 v)
    { asm volatile("mcrr p15, 6, %Q0, %R0, c14" : : "r" (v)); }

    static Unsigned32 control()
    { Unsigned32 v; asm volatile("mrc p15, 4, %0, c14, c2, 1" : "=r" (v)); return v; }

    static void control(Unsigned32 v)
    { asm volatile("mcr p15, 4, %0, c14, c2, 1" : : "r" (v)); }

    static void setup_timer_access()
    {
      // CNTKCTL: allow access to virtual and physical counter from PL0
      asm volatile("mcr p15, 0, %0, c14, c1, 0" : : "r"(0x3));
      // CNTHCTL: forbid access to physical timer from PL0 and PL1
      asm volatile("mcr p15, 4, %0, c14, c1, 0" : : "r"(0x0));
    }

    static Unsigned32 frequency()
    { Unsigned32 v; asm volatile ("mrc p15, 0, %0, c14, c0, 0": "=r" (v)); return v; }

    static void frequency(Unsigned32 v)
    { asm volatile ("mcr p15, 0, %0, c14, c0, 0": :"r" (v)); }
  };
}

// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer && cpu_virt]:

namespace Generic_timer { typedef Generic_timer::T<Generic_timer::Hyp> Gtimer; }

// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer && arm_em_tz]:

namespace Generic_timer { typedef Generic_timer::T<Generic_timer::Physical> Gtimer; }

// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer && (!cpu_virt && (arm_em_ns || arm_em_std))]:

namespace Generic_timer { typedef Generic_timer::T<Generic_timer::Virtual> Gtimer; }

