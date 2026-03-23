/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Valentin Gehrke <valentin.gehrke@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

INTERFACE [arm]:

#include <std_macros.h>
#include <types.h>

class Semihosting
{
private:
  template <typename T>
  static inline ALWAYS_INLINE Mword as_mword(T v)
  { return static_cast<Mword>(v); }

  template <typename T>
  static inline ALWAYS_INLINE Mword as_mword(T* v)
  { return reinterpret_cast<Mword>(v); }

public:
  enum
  {
    Op_open = 0x01,
    Op_write = 0x05,

    Mode_read = 0,
    Mode_write = 4,
  };

  typedef Mword Op;

  // Various parameter types
  typedef Mword Open_mode;
  typedef Smword File_handle;
  typedef Mword Size;

  // For use with SYS_OPEN to open stdout/stdin of simulator
  static constexpr char const * Magic_stdio_path = ":tt";

  static constexpr File_handle Invalid_file_handle = -1;

  /* Call with parameter in register
   * r0 = op
   * r1 = single parameter
   * r0 = return value
   */
  template <typename Ret, typename Arg>
  static inline ALWAYS_INLINE Ret
  call(Op op, Arg arg)
  {
    register Mword r0 asm("r0") = op;
    register Mword r1 asm("r1") = as_mword(arg);
    register Ret ret asm("r0");
    if constexpr (TAG_ENABLED(thumb2))
      asm volatile("hlt #0x3c" : "=r" (ret) : "r" (r0), "r" (r1) : "memory");
    else
      asm volatile("hlt #0xF000" : "=r" (ret) : "r" (r0), "r" (r1) : "memory");
    return ret;
  }

  /* Call with parameters in memory
   * r0 = op
   * r1 = pointer to array of parameters,
   * r0 = return value
   */
  template <typename Ret, typename... Args>
  static inline ALWAYS_INLINE Ret
  call_block(Op op, Args... args)
  {
    Mword params[] = { as_mword(args) ... };

    return call<Ret, Mword*>(op, params);
  }

  // Basic call without any parameter (often want r1 to be 0)
  template <typename Ret>
  static inline ALWAYS_INLINE Ret
  call_zero(Op op)
  {
    return call<Ret, void*>(op, nullptr);
  }

  // SYS_OPEN wrapper
  static inline ALWAYS_INLINE File_handle
  sys_open(char const *path, Open_mode mode)
  {
    return call_block<File_handle, char const *, Mword, Mword>
      (Op_open, path, mode, __builtin_strlen(path));
  }

  // SYS_WRITE wrapper
  static inline ALWAYS_INLINE Size
  sys_write(File_handle fh, char const *buf, Size buflen)
  {
    return call_block<Size, File_handle, char const *, Size>
      (Op_write, fh, buf, buflen);
  }
};
