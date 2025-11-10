INTERFACE:

#include "cpu_mask.h"
#include "member_offs.h"
#include "global_data.h"

class Cpu
{
  MEMBER_OFFSET();

public:
  struct By_phys_id
  {
    Cpu_phys_id _p;
    By_phys_id(Cpu_phys_id p) : _p(p) {}
    template<typename CPU>
    bool operator () (CPU const &c) const { return _p == c.phys_id(); }
  };
  // we actually use a mask that has one CPU more that we can physically,
  // have, to avoid lots of special cases for an invalid CPU number
  typedef Cpu_mask_t<Config::Max_num_cpus + 1> Online_cpu_mask;

  enum { Invalid = Config::Max_num_cpus };
  static Cpu_number invalid() { return Cpu_number(Invalid); }

  /** Get the logical ID of this CPU */
  Cpu_number id() const;

  /**
   * Set CPU online.
   *
   * This does not activate an inactive CPU, just sets the online state.
   */
  void set_online();

  /**
   * Set CPU offline (i.e. not online).
   *
   * This does not deactivate an active CPU, just sets the offline state.
   */
  void set_offline();

  /**
   * Set CPU present.
   */
  void set_present();

  /**
   * Set CPU absent (i.e. not present).
   */
  void set_absent();

  /** Convenience for Cpu::cpus.cpu(cpu).online() */
  static bool online(Cpu_number cpu);
  /** Is this CPU online ? */
  bool online() const;

  static Online_cpu_mask const &online_mask();
  static Online_cpu_mask const &present_mask();

  static bool is_canonical_address(Address addr);

  /**
   * Show useful information when a CPU is booted but only if the information is
   * different from the boot CPU.
   */
  void print_infos() const;

private:

  static Global_data<Online_cpu_mask> _online_mask;
  static Global_data<Online_cpu_mask> _present_mask;
};


//--------------------------------------------------------------------------
INTERFACE[mp]:

EXTENSION class Cpu
{

private:
  void set_id(Cpu_number id) { _id = id; }
  Cpu_number _id;
};

//--------------------------------------------------------------------------
INTERFACE[!mp]:

EXTENSION class Cpu
{
private:
  void set_id(Cpu_number) {}
};


// --------------------------------------------------------------------------
IMPLEMENTATION:

DEFINE_GLOBAL
Global_data<Cpu::Online_cpu_mask> Cpu::_online_mask(Online_cpu_mask::Init::Bss);

DEFINE_GLOBAL
Global_data<Cpu::Online_cpu_mask> Cpu::_present_mask(Online_cpu_mask::Init::Bss);

IMPLEMENT inline
Cpu::Online_cpu_mask const &
Cpu::online_mask()
{ return _online_mask; }

IMPLEMENT inline
Cpu::Online_cpu_mask const &
Cpu::present_mask()
{ return _present_mask; }

IMPLEMENT inline
void
Cpu::set_online()
{ _online_mask->atomic_set(id()); }

IMPLEMENT inline
void
Cpu::set_offline()
{ _online_mask->atomic_clear(id()); }

IMPLEMENT inline
void
Cpu::set_present()
{ _present_mask->atomic_set(id()); }

IMPLEMENT inline
void
Cpu::set_absent()
{ _present_mask->atomic_clear(id()); }

IMPLEMENT_DEFAULT inline
bool
Cpu::is_canonical_address(Address)
{ return true; }

IMPLEMENT_DEFAULT inline
void
Cpu::print_infos() const
{}

// --------------------------------------------------------------------------
IMPLEMENTATION [mp]:

IMPLEMENT inline
Cpu_number
Cpu::id() const
{ return _id; }

IMPLEMENT inline
bool
Cpu::online() const
{ return _online_mask->get(_id); }

IMPLEMENT static inline
bool
Cpu::online(Cpu_number _cpu)
{ return _online_mask->get(_cpu); }

// --------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

IMPLEMENT inline
Cpu_number
Cpu::id() const
{ return Cpu_number::boot_cpu(); }

IMPLEMENT inline
bool
Cpu::online() const
{ return true; }

IMPLEMENT static inline
bool
Cpu::online(Cpu_number _cpu)
{ return _cpu == Cpu_number::boot_cpu(); }

