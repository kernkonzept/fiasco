INTERFACE:

class Mem
{
public:
  /**
   * Memory barriers.
   */
  static void barrier() { __asm__ __volatile__ ("" : : : "memory"); }

  static void mb();
  static void rmb();
  static void wmb();

  static void mp_mb();
  static void mp_rmb();
  static void mp_wmb();
  static void mp_acquire();
  static void mp_release();

  /**
   * Memory operations.
   */
  static void memcpy_mwords(void *dst, void const *src, unsigned long nr_of_mwords);
  static void memcpy_bytes (void *dst, void const *src, unsigned long nr_of_bytes);
  static void memset_mwords(void *dst, const unsigned long val, unsigned long nr_of_mwords);

  template<typename T> static T read64_consistent(T const *t);
  template<typename T> static void write64_consistent(T *t, T val);
};

//---------------------------------------------------------------------------
IMPLEMENTATION[!mp]:

IMPLEMENT inline static void Mem::mb() { barrier(); }
IMPLEMENT inline static void Mem::rmb() { barrier(); }
IMPLEMENT inline static void Mem::wmb() { barrier(); }

IMPLEMENT inline static void Mem::mp_mb() { barrier(); }
IMPLEMENT inline static void Mem::mp_rmb() { barrier(); }
IMPLEMENT inline static void Mem::mp_wmb() { barrier(); }
IMPLEMENT inline static void Mem::mp_acquire() { barrier(); }
IMPLEMENT inline static void Mem::mp_release() { barrier(); }

//---------------------------------------------------------------------------
IMPLEMENTATION [64bit]:

#include "types.h"

IMPLEMENT_DEFAULT static inline NEEDS["types.h"]
template<typename T>
T
Mem::read64_consistent(T const *t)
{
  return access_once(t);
}

IMPLEMENT_DEFAULT static inline NEEDS["types.h"]
template<typename T>
void
Mem::write64_consistent(T *t, T val)
{
  write_now(t, val);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [32bit]:

#include "types.h"

IMPLEMENT_DEFAULT static inline NEEDS[Mem::mp_rmb, "types.h"]
template<typename T>
T
Mem::read64_consistent(T const *t)
{
  static_assert(sizeof(T) == 2* sizeof(Unsigned32), "value has invalid size");
  union U
  {
    T v64;
    struct S32
    {
#ifdef CONFIG_BIG_ENDIAN
      Unsigned32 hi, lo;
#else
      Unsigned32 lo, hi;
#endif
    } v32;
  };

  U const *u = reinterpret_cast<U const *>(t);
  U res;
  do
    {
      res.v32.hi = access_once(&(u->v32.hi));
      mp_rmb();
      res.v32.lo = access_once(&(u->v32.lo));
      mp_rmb();
    }
  while (res.v32.hi != access_once(&(u->v32.hi)));
  return res.v64;
}

IMPLEMENT_DEFAULT static inline NEEDS[Mem::mp_wmb, "types.h"]
template<typename T>
void
Mem::write64_consistent(T *t, T val)
{
  static_assert(sizeof(T) == 2* sizeof(Unsigned32), "value has invalid size");
  union U
  {
    T v64;
    struct S32
    {
#ifdef CONFIG_BIG_ENDIAN
      Unsigned32 hi, lo;
#else
      Unsigned32 lo, hi;
#endif
    } v32;
  };

  U *u = reinterpret_cast<U *>(t);
  U const &v = reinterpret_cast<U const &>(val);
  write_now(&(u->v32.lo), v.v32.lo);
  mp_wmb();
  write_now(&(u->v32.hi), v.v32.hi);
}

