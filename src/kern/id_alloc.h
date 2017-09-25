#pragma once

#include "boot_alloc.h"
#include "types.h"
#include "atomic.h"
#include "processor.h"

template<typename ID_TYPE, typename OWNER_TYPE, typename ID_OPS>
class Id_alloc
{
public:
  typedef ID_TYPE Id_type;
  typedef OWNER_TYPE Owner_type;
  typedef ID_OPS Id_ops;

  Id_alloc(unsigned nr_ids) : _nr_ids(nr_ids)
  {
    _active = (Owner_type **)Boot_alloced::alloc(sizeof(Owner_type *) * _nr_ids);
    for (unsigned i = 0; i < _nr_ids; ++i)
      _active[i] = 0;
  }

  template<typename ARG>
  Id_type alloc(Owner_type *owner, ARG &&arg)
  {
    if (EXPECT_TRUE(Id_ops::valid(owner, arg)))
      return Id_ops::get_id(owner, arg);

    Id_type new_id = next_id();
    Owner_type **bad_guy = &_active[new_id];
    while (Owner_type *victim = access_once(bad_guy))
      {
        if (victim == reinterpret_cast<Owner_type *>(~0UL))
          break;

        if (!Id_ops::can_replace(victim, arg))
          {
            new_id = next_id();
            bad_guy = &_active[new_id];
            continue;
          }

        // If the victim is valid and we get a 1 written to the ID array
        // then we have to reset the ID of our victim, else the
        // reset function is currently resetting the IDs of the
        // victim from a different CPU.
        if (mp_cas(bad_guy, victim, reinterpret_cast<Owner_type *>(1)))
          Id_ops::reset_id(victim, arg);
        break;
      }

    Id_ops::set_id(owner, arg, new_id + Id_ops::Id_offset);
    write_now(bad_guy, owner);
    return new_id + Id_ops::Id_offset;
  }

  template<typename ARG>
  void free(Owner_type *owner, ARG &&arg)
  {
    if (!Id_ops::valid(owner, arg))
      return;

    Id_type id = Id_ops::get_id(owner, arg) - Id_ops::Id_offset;
    Owner_type **o = &_active[id];
    if (!mp_cas(o, owner, reinterpret_cast<Owner_type *>(~0UL)))
      while (access_once(o) == reinterpret_cast<Owner_type *>(1))
        Proc::pause();
  }

private:
  unsigned _nr_ids;
  Id_type _next_free = 0;
  Owner_type **_active;

  Id_type next_id()
  {
    Id_type n = _next_free;
    ++_next_free;
    if (_next_free >= _nr_ids)
      _next_free = 0;
    return n;
  }
};


