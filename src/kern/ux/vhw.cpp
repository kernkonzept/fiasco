/*
 * Description for driver memory areas, used under UX.
 */

INTERFACE[ux,ia32]:

#include "types.h"

class Vhw_entry
{
public:

  enum Vhw_entry_type
  {
    TYPE_NONE,
    TYPE_FRAMEBUFFER,
    TYPE_INPUT,
    TYPE_NET,
  };

private:
  Vhw_entry_type _type;	    // type of the descriptor
  Unsigned32 _provider_pid; // Host PID of the program providing this area

  Address    _mem_start;    // start of a memory area in physical memory
  Address    _mem_size;     // size of the memory area

  Unsigned32 _irq_no;       // IRQ number for notification events
  Unsigned32 _fd;           // Communication file descriptor

public:

  void type(enum Vhw_entry_type t) { _type = t; }
  void provider_pid(Unsigned32 p)  { _provider_pid = p; }
  void mem_start(Address a)        { _mem_start = a; }
  void mem_size(Address a)         { _mem_size = a; }
  void irq_no(Unsigned32 i)        { _irq_no = i; }
  void fd(Unsigned32 i)            { _fd = i; }
};

// -------

class Vhw_descriptor
{
public:
  Unsigned32 magic() const       { return _magic; }
  Unsigned8 version() const      { return _version; }
  Unsigned8 count() const        { return _count; }

private:

  static Unsigned8 const count_const = 3;  // nr of descriptors

  Unsigned32 _magic;                       // magic value
  Unsigned8  _version;                     // version of this description format
  Unsigned8  _count;                       // nr of descriptors
  Unsigned8  _pad1;
  Unsigned8  _pad2;

  Vhw_entry descs[count_const];
};

// -----------------------------------------------------------------
IMPLEMENTATION[ux,ia32]:

PUBLIC
void
Vhw_entry::reset()
{
  type(TYPE_NONE);
}

PUBLIC
void
Vhw_entry::set(enum Vhw_entry_type t,
               Address start, Address size,
               Unsigned32 irqno, Unsigned32 provpid, Unsigned32 fdp)
{
  type(t);
  mem_start(start);
  mem_size(size);
  irq_no(irqno);
  provider_pid(provpid);
  fd(fdp);
}

// -------

PUBLIC
void
Vhw_descriptor::init()
{
  _magic   = 0x56687765;
  _version = 1;
  _count   = count_const;

  for (int i = 0; i < count_const; i++)
    descs[i].reset();
}

PUBLIC
void
Vhw_descriptor::set_desc(Vhw_entry::Vhw_entry_type type,
                         Address mem_start, Address mem_size,
                         Unsigned32 irq_no, Unsigned32 provider_pid,
                         Unsigned32 fdp)
{
  if (type != Vhw_entry::TYPE_NONE)
    descs[type - 1].set(type, mem_start, mem_size, irq_no, provider_pid, fdp);
}
