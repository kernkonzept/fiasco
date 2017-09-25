INTERFACE [debug]:

#include "types.h"
#include "lock_guard.h"
#include "spin_lock.h"
#include "slab_cache.h"
#include <cxx/slist>

class Dbg_page_info_table;

class Dbg_page_info : public cxx::S_list_item
{
  friend class Dbg_page_info_table;

private:
  Page_number const _pfn;
  typedef unsigned long Buf[5];
  Buf _buf;

  char *b() { return reinterpret_cast<char*>(_buf); }
  char const *b() const { return reinterpret_cast<char const*>(_buf); }

  typedef Slab_cache Allocator;

public:
  void *operator new (size_t) throw() { return alloc()->alloc(); }
  void operator delete (void *p, size_t) { alloc()->free(p); }

  enum { Buffer_size = sizeof(Buf) };

  Dbg_page_info(Page_number pfn) : _pfn(pfn) {}

  bool match(Page_number p) { return _pfn == p; }

  template<typename T>
  T *info()
  { return reinterpret_cast<T*>(b()); }

  template<typename T>
  T const *info() const
  { return reinterpret_cast<T const*>(b()); }
};

class Dbg_page_info_table
{
private:
  typedef cxx::S_list_bss<Dbg_page_info> List;

public:
  struct Entry
  {
    List h;
    Spin_lock<> l;
  };
  enum { Hash_tab_size = 1024 };

private:
  Entry _tab[Hash_tab_size];
  static unsigned hash(Page_number p)
  { return cxx::int_value<Page_number>(p) % Hash_tab_size; }
};



IMPLEMENTATION [debug]:

#include "kmem_slab.h"


static Dbg_page_info_table _t;

PUBLIC static
Dbg_page_info_table &
Dbg_page_info::table()
{
  return _t;
}

static Kmem_slab_t<Dbg_page_info> _dbg_page_info_allocator("Dbg_page_info");

PRIVATE static
Dbg_page_info::Allocator *
Dbg_page_info::alloc()
{ return _dbg_page_info_allocator.slab(); }


PUBLIC template<typename B, typename E> static inline
B
Dbg_page_info_table::find(B const &b, E const &e, Page_number p)
{
  for (B i = b; i != e; ++i)
    if (i->match(p))
      return i;
  return e;
}

PUBLIC
Dbg_page_info *
Dbg_page_info_table::operator [] (Page_number pfn) const
{
  Entry &e = const_cast<Dbg_page_info_table*>(this)->_tab[hash(pfn)];
  auto g = lock_guard(e.l);
  // we know that *end() is NULL
  return *find(e.h.begin(), e.h.end(), pfn);
}

PUBLIC
void
Dbg_page_info_table::insert(Dbg_page_info *i)
{
  Entry *e = &_tab[hash(i->_pfn)];
  auto g = lock_guard(e->l);
  e->h.add(i);
}

PUBLIC
Dbg_page_info *
Dbg_page_info_table::remove(Page_number pfn)
{
  Entry *e = &_tab[hash(pfn)];
  auto g = lock_guard(e->l);

  List::Iterator i = find(e->h.begin(), e->h.end(), pfn);
  if (i == e->h.end())
    return 0;

  Dbg_page_info *r = *i;
  e->h.erase(i);
  return r;
}
