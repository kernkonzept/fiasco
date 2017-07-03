INTERFACE[mips]:

#include "l4_types.h"
#include "types.h"
#include "cp0_status.h"
#include "mem.h"
#include <cxx/bitfield>

class Syscall_frame
{
private:
  Mword r[5];

public:
  void from(Mword id)
  { r[4] = id; }

  Mword from_spec() const
  { return r[4]; }

  L4_obj_ref ref() const
  { return L4_obj_ref::from_raw(r[1]); }

  void ref(L4_obj_ref const &ref)
  { r[1] = ref.raw(); }

  L4_timeout_pair timeout() const
  { return L4_timeout_pair(r[2]); }

  void timeout(L4_timeout_pair const &to)
  { r[2] = to.raw(); }

  Utcb *utcb() const
  { return reinterpret_cast<Utcb*>(r[0]); }

  L4_msg_tag tag() const
  { return L4_msg_tag(r[3]); }

  void tag(L4_msg_tag const &tag)
  { r[3] = tag.raw(); }
};

class Entry_frame;

class Return_frame
{
public:
  Mword bad_instr_p;
  Mword bad_instr;
  Mword r[32];
  Mword hi, lo;
  Mword bad_v_addr;
  Mword cause;
  Mword status;
  Mword epc;

  enum Regs
  {
    R_a0 =  4,  R_a1 =  5,  R_a2 =  6,  R_a3 =  7,
    R_t0 =  8,  R_t1 =  9,  R_t2 = 10,  R_t3 = 11,
    R_t4 = 12,  R_t5 = 13,
    R_s0 = 16,  R_s1 = 17,  R_s2 = 18,  R_s3 = 19,
    R_s4 = 20,  R_s5 = 21,  R_s6 = 22,  R_s7 = 23,
    R_sp = 29,
  };

  enum Cause_bits
  {
    C_type_mask = 0x00000300, // use the IP0 and IP1 bits for L4 specifics
    C_type_mips = 0x00000000, ///< 0 means HW cause value
    C_type_l4   = 0x00000100, ///< L4 specific cause code in bits 0..7
    C_l4_ipc_upcall     = 0x00000100,
    C_l4_sw_exception   = 0x00000101,
    C_l4_syscall_upcall = 0x00000102,

    C_src_context_mask = 0x03, // use bits 0..1 to encode the source context
    C_src_kern         = 0x00, ///< exception caused by user kernel mode
    C_src_user         = 0x01, ///< exception caused by user mode
    C_src_guest        = 0x02, ///< exception caused by guest mode
  };

  enum Status_bits
  {
    S_ksu = 0x18
  };

  struct Cause
  {
    Mword raw;
    Cause() = default;
    Cause(Mword c) : raw(c) {}
    /// L4-extension: source context: kern=0, user=1, guest=2
    CXX_BITFIELD_MEMBER( 0,  1, src_context, raw);
    /// Exception code
    CXX_BITFIELD_MEMBER( 2,  6, exc_code, raw);
    /// L4-extension: break point spec: 0 normal, 1 JSB sequence, 2 JDB IPI
    CXX_BITFIELD_MEMBER(16, 17, bp_spec, raw);
  };

  Mword eret_work() const
  { return r[0]; /* unused zero register */ }

  void eret_work(Mword v)
  { r[0] = v; /* unused zero register */ }

  Mword sp() const
  { return r[R_sp]; }

  void sp(Mword sp)
  { r[R_sp] = sp; }

  Mword ip() const
  { return epc; }

  void ip(Mword pc)
  { epc = pc; }

  bool user_mode() const
  { return status & Cp0_status::ST_KSU_USER; }

  void copy_and_sanitize(Return_frame const *f)
  {
    Mem::memcpy_mwords(&r[1], &f->r[1], 31u + 2u);
    status &= Cp0_status::ST_USER_MASK;
    status |= Cp0_status::ST_USER_MUST_SET;

    epc = access_once(&f->epc);
  }

  void set_ipc_upcall()
  { cause = C_l4_ipc_upcall; }

  Address ip_syscall_page_user() const
  { return epc; }

  Syscall_frame *syscall_frame()
  { return reinterpret_cast<Syscall_frame *>(&r[R_s0]); }

  Syscall_frame const *syscall_frame() const
  { return reinterpret_cast<Syscall_frame const *>(&r[R_s0]); }

  static Entry_frame *to_entry_frame(Syscall_frame *sf)
  {
    // assume Entry_frame == Return_frame
    return reinterpret_cast<Entry_frame *>
      (  reinterpret_cast<char *>(sf)
       - reinterpret_cast<intptr_t>(&reinterpret_cast<Return_frame *>(0)->r[R_s0]));
  }
};

class Entry_frame : public Return_frame {};

//------------------------------------------------------------------------------
IMPLEMENTATION [mips]:

#include <cstdio>
#include "cp0_status.h"

PUBLIC
void
Return_frame::dump() const
{
  char const *const regs[] =
  {
    "00", "at", "v0", "v1",
    "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3",
    "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3",
    "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1",
    "gp", "sp", "fp", "ra"
  };

  printf("00[ 0]: 00000000 ");
  for (unsigned i = 1; i < 32; ++i)
    printf("%s[%2d]: %08lx%s", regs[i], i, r[i], (i & 3) == 3 ? "\n" : " ");

  printf("HI: %08lx LO: %08lx\n", hi, lo);
  printf("Status %08lx Cause %08lx EPC %08lx\n", status, cause, epc);
  //printf("Cause  %08lx BadVaddr %08lx\n", cause, badvaddr);
}

