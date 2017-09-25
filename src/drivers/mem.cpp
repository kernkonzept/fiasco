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
