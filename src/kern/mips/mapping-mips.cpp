/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Sanjay Lal <sanjayl@kymasys.com>
 */

INTERFACE [mips32]:

#include "types.h"

class Treemap;

class Mapping_entry
{
public:
  enum { Alignment = 4 };
  union 
  {
    struct 
    {
      unsigned long _space:32;	///< Address-space number
//      unsigned long _pad:1;
      unsigned long address:20;	///< Virtual address in address space
    } __attribute__((packed)) data;
    Treemap *_submap;
  };
  Unsigned8 _depth;
  void set_space(Space *s) { data._space = (unsigned long)s; }
  Space *space() const { return (Space *)data._space; }
};


INTERFACE [mips64]:

#include "mem_layout.h"
#include "types.h"

class Treemap;

class Mapping_entry
{
public:
  enum { Alignment = 8 };
  union 
  {
    struct 
    {
      unsigned long address; ///< Virtual address in address space
      unsigned _space;       ///< Address-space number
    } __attribute__((packed)) data;
    Treemap *_submap;
  };
  Unsigned8 _depth;

  void set_space(Space *s)
  { data._space = (unsigned long)s & 0xffffffff; }

  Space *space() const
  { return (Space *)((unsigned long)data._space | Mem_layout::KSEG0); }
};
