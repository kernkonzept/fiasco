INTERFACE:

#include "types.h"

class L4_error
{
public:
  enum Error_code
  {
    None            = 0,
    Timeout         = 2,
    R_timeout       = 3,
    Not_existent    = 4,
    Canceled        = 6,
    R_canceled      = 7,
    Overflow        = 8,
    Snd_xfertimeout = 10,
    Rcv_xfertimeout = 12,
    Aborted         = 14,
    R_aborted       = 15,
    Map_failed      = 16,
  };

  enum Phase
  {
    Snd = 0,
    Rcv = 1
  };

  L4_error(Error_code ec = None, Phase p = Snd) : _raw(ec | p) {}
  L4_error(L4_error const &e, Phase p = Snd) : _raw(e._raw | p) {}

  bool ok() const { return _raw == 0; }

  Error_code error() const { return Error_code(_raw & 0x1e); }
  Mword raw() const { return _raw; }
  bool snd_phase() const { return !(_raw & Rcv); }

  static L4_error from_raw(Mword raw) { return L4_error(true, raw); }

private:
  L4_error(bool, Mword raw) : _raw(raw) {}
  Mword _raw;
};


//----------------------------------------------------------------------------
IMPLEMENTATION [debug]:

static char const *__errors[] =
{ "OK", "timeout", "not existent", "canceled", "overflow",
  "xfer snd", "xfer rcv", "aborted", "map failed" };



PUBLIC
char const *
L4_error::str_error()
{
  return __errors[(_raw >> 1) & 0xf];
}

