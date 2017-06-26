/*
 * arch. independent L4 Types
 */

INTERFACE:

#include "types.h"
#include "l4_fpage.h"
#include "l4_buf_desc.h"
#include "l4_error.h"

typedef Address Local_id;

class Utcb;

/**
 * A reference to a kernel object (capability selector),
 * as passed from user level.
 *
 * A capability selector contains an index into the capability table/object
 * space of a task.  The index is usually stored in the most significant bits
 * of the binary representation. The twelve least significant bits are used to
 * to denote the type of operation that shall be invoked and also so flags for
 * special capabilities, such as the invalid cap, the reply capability, or the
 * self capability.
 *
 * Generally all operations on kernel objects are modelled as message passing
 * primitives that consist of two phases, the send phase and the receive phase.
 * However, both phases are optional and come in slightly different flavors.
 * \see L4_obj_ref::Operation.
 * The terms send and receive are from the invokers point of view.  This means,
 * a client doing RPC needs a send for sending the requested operation an
 * parameters and a receive to receive the return code of the RPC.
 */
class L4_obj_ref
{
public:
  /**
   * Operation codes, stored in the four least significant bits of a capability
   * selector.
   */
  enum Operation
  {
    /// A no-op on the capability (undefined from user level).
    None = 0,

    /**
     * \deprecated Use #Ipc_call_ipc.
     * Deprecated call code, do not use this operation code.
     */
    Ipc_call = 0,

    /**
     * Set this bit to include a send phase.
     *
     * In the case of a send phase, the message is send to the object
     * denoted by either the capability selector (cap()), the reply capability
     * (if #Ipc_reply is also set), or to the thread itself (if the cap is the
     * special self capability).
     */
    Ipc_send = 1,

    /**
     * Set this bit to include a receive phase.
     *
     * During the receive phase the caller waits for a message from either a
     * specific sender (closed wait) or from any possible sender
     * (#Ipc_open_wait) that has a capability to send messages to the invoker.
     */
    Ipc_recv = 2,

    /**
     * Set this bit to denote an open-wait receive phase.
     *
     * An open wait means that the invoker shall wait for a message from any
     * sender that has a capability to send messages to the invoker.  In this
     * case the index (cap()) in the capability selector are ignored for the
     * receive phase.
     */
    Ipc_open_wait = 4,

    /**
     * Set this bit to make the send phase a reply.
     *
     * A reply operation uses the implicit reply capability that is stored
     * in per thread storage and can be used only once. The reply capability
     * also vanishes in the case of an abort due to the caller or a newly
     * received call operation by the same thread.
     * \see #Ipc_send.
     */
    Ipc_reply = 8,

    /**
     * Denotes a wait operation. (#Ipc_recv | #Ipc_open_wait).
     *
     * The wait operation is usually used by servers to implement remote
     * objects.
     */
    Ipc_wait           = Ipc_open_wait | Ipc_recv,

    /**
     * Denotes a combination of a send and a wait operation (#Ipc_send |
     * #Ipc_recv | #Ipc_open_wait).
     *
     * \note this is not used for usual RPC, see #Ipc_reply_and_wait for that.
     */
    Ipc_send_and_wait  = Ipc_open_wait | Ipc_send | Ipc_recv,

    /**
     * Denotes a reply and wait operation (#Ipc_send | #Ipc_reply | #Ipc_recv |
     * #Ipc_open_wait).
     *
     * This operation is usually used to send replies to RPC requests.
     */
    Ipc_reply_and_wait = Ipc_open_wait | Ipc_send | Ipc_recv | Ipc_reply,

    /**
     * Denotes a call operation (#Ipc_send | #Ipc_recv).
     *
     * A call is usually used by a client to invoke an operation on a remote
     * object and wait for a result. The call operation establishes the
     * implicit reply capability for the partner thread (see #Ipc_reply)
     * and enables the implementation of an object to respond to an invocation
     * without knowledge of the invoker thread.
     */
    Ipc_call_ipc       = Ipc_send | Ipc_recv,
  };

  /**
   * Special capability selectors (e.g., Invalid cap).
   */
  enum Special
  {
    /**
     * Invalid capability selector.
     */
    Invalid      = 1UL << 11UL,

    /**
     * Bit that flags a capability selector as special.
     */
    Special_bit  = 1UL << 11UL,

    /**
     * Value for the self capability selector. This means, the invoking thread
     * references itself.
     */
    Self         = (~0UL) << 11UL,

    /**
     * Mask for getting all bits of special capabilities.
     */
    Special_mask = (~0UL) << 11UL,
  };

  enum
  {
    Cap_shift = 12UL
  };

  /**
   * Create a special capability selector from \a s.
   * \param s which special cap selector shall be created
   *          (see L4_obj_ref::Special).
   *
   * Special capability selectors are the invalid capability and the self
   * Capability.  All special capability selectors must have the #Special_bit
   * set.
   */
  L4_obj_ref(Special s = Invalid) : _raw(s) {}

  /**
   * Create a capability selector from it's binary representation.
   * \param raw the raw binary representation of a capability selector. As
   *            passed from user land.
   */
  static L4_obj_ref from_raw(Mword raw) { return L4_obj_ref(true, raw); }

  /**
   * Is the capability selector a valid capability (no special capability).
   * \return true if the capability selector is a valid index into the
   *         capability table, or false if the selector is a special
   *         capability.
   */
  bool valid() const { return !(_raw & Special_bit); }

  /**
   * Is the capability selector a special capability (i.e., not an index
   * into the capability table).
   * \return true if the capability selector denotes a special capability
   *         (see L4_obj_ref::Special), or false if this capability is a
   *         valid index into a capability table.
   *
   */
  bool special() const { return _raw & Special_bit; }

  /**
   * Is this capability selector the special \a self capability.
   * \return true if this capability is the special self capability for the
   *         invoking thread.
   */
  bool self() const { return special(); }

  /**
   * Get the value of a special capability.
   * \pre special() == true
   * \return the value of a special capability selector, see
   *         L4_obj_ref::Special.
   */
  Special special_cap() const { return Special(_raw & Special_mask); }
  //bool self() const { return (_raw & Invalid_mask) == Self; }

  /**
   * Does the operation contain a receive phase?
   * \return true if the operation encoded in the capability selector
   *              comprises a receive phase, see #L4_obj_ref::Ipc_recv.
   */
  unsigned have_recv() const { return _raw & Ipc_recv; }

  /**
   * Get the index into the capability table.
   * \pre valid() == true
   * \return The index into the capability table stored in the capability
   *         selector (i.e., the most significant bits of the selector).
   */
  Cap_index cap() const { return Cap_index(_raw >> 12); }

  /**
   * Get the operation stored in this selector (see L4_obj_ref::Operation).
   * \return The operation encoded in the lower 4 bits of the capability
   *         selector, see L4_obj_ref::Operation.
   */
  Operation op() const { return Operation(_raw & 0xf); }

  /**
   * Get the raw binary representation of this capability selector.
   * \return the binary representation of this cap selector.
   */
  Mword raw() const { return _raw; }

  /**
   * Create a valid capability selector for the shifted cap-table index
   * and the operation.
   * \param cap the shifted (<< #Cap_shift) capability-table index.
   * \param op the operation to be encoded in bits 0..3.
   */
  explicit L4_obj_ref(Mword cap, Operation op = None) : _raw(cap | op) {}
  explicit L4_obj_ref(Cap_index cap, Operation op = None)
  : _raw((cxx::int_value<Cap_index>(cap) << L4_obj_ref::Cap_shift) | op) {}

  /**
   * Create a capability selector (index 0) with the given operation.
   * \param op the operation to be encoded into the capability selector,
   *        see L4_obj_ref::Operation.
   */
  L4_obj_ref(Operation op) : _raw(op) {}

  /**
   * Compare two capability selectors for equality.
   * \param o the right hand side for te comparison.
   * \note Capability selectors are compared by their binary representation.
   */
  bool operator == (L4_obj_ref const &o) const { return _raw == o._raw; }

private:
  L4_obj_ref(bool, Mword raw) : _raw(raw) {}
  Mword _raw;
};


/**
 * Flags for unmap operations.
 */
class L4_map_mask
{
public:

  /**
   * Create a from binary representation.
   * \param raw the binary representation, as passed from user level.
   */
  explicit L4_map_mask(Mword raw = 0) : _raw(raw) {}

  /**
   * Get the flags for a full unmap.
   * \return A L4_map_mask for doing a full unmap operation.
   */
  static L4_map_mask full() { return L4_map_mask(0xc0000002); }

  /**
   * Get the raw binary representation for the map mask.
   * \return the binary value of the flags.
   */
  Mword raw() const { return _raw; }

  /**
   * Unmap from the calling Task too.
   * \return true if the caller wishes to unmap from its own address space too.
   */
  Mword self_unmap() const { return _raw & 0x80000000; }

  /**
   * Shall the unmap delete the object if allowed?
   * \return true if the unmap operation shall also delete the kernel
   * object if permitted to the caller.
   */
  Mword do_delete() const { return _raw & 0x40000000; }

private:
  Mword _raw;
};


/**
 * Description of a message to the kernel or other thread.
 *
 * A message tag determines the number of untyped message words (words()), the
 * number of typed message items (items(), L4_msg_item), some flags, and a
 * protocol ID.  The number of typed and untyped items in the UTCB's message
 * registers, as well as the flags, control the kernels message passing
 * mechanism.  The protocol ID is not interpreted by the message passing
 * itself, however is interpreted by the receiving object itself.  In thread to
 * thread IPC the all contents besides the flags are copied from the sender to
 * the receiver. The flags on the receiver side contain some information about
 * the operation itself.
 *
 * The untyped message words are copied to the receiving object/thread
 * uninterpreted. The typed items directly following the untyped words in
 * the message registers are interpreted by the message passing and contain,
 * for example, map items for memory or kernel objects (see L4_msg_item,
 * L4_fpage).
 */
class L4_msg_tag
{
public:
  /**
   * Flags in the message tag.
   *
   * The input flags control the send phase of an IPC operation. Flags might
   * have a different semantics in the returned message tag, the result of an
   * IPC operation, see L4_msg_tag::Output_flags. However, the #Transfer_fpu
   * and #Schedule flags are passed to the receiver.
   */
  enum Flags
  {
    /**
     * The sender is transferring the state of the floating-point unit (FPU)
     * as part of the message.
     * \note The receiver needs to agree with that by setting
     *       L4_buf_desc::Inherit_fpu in its buffer descriptor register (BDR).
     * \note This flag is passed through to the receiver.
     */
    Transfer_fpu = 0x1000,

    /**
     * The sender does not want to donate its remaining time-slice to the
     * receiver (partner) thread.
     * \note This flag is passed on to the receiver.
     */
    Schedule     = 0x2000,

    /**
     * The sender wants to propagate an incoming call operation to a different
     * thread.
     * \note Not implemented in Fiasco.OC.
     *
     * Propagation means that the reply capability shall be passed on to the
     * receiver of this message to enable a direct reply.
     */
    Propagate    = 0x4000, // snd only flag
  };

  /**
   * Result flags for IPC operations.
   *
   * These flags are dedicated return values for an IPC operation.
   */
  enum Output_flags
  {
    /**
     * The IPC operation did not succeed, the detailed error code
     * is in the error register in the UTCB.
     */
    Error        = 0x8000,

    /**
     * Combination of flags that are not pass through.
     */
    Rcv_flags    = Error,
  };

  /**
   * Protocol IDs that are defined by the kernel ABI.
   *
   * These protocol IDs are used for either kernel implemented
   * objects, or used for kernel-synthesized requests to user
   * objects.
   */
  enum Protocol
  {
    Label_none          = 0, ///< No protocol, the default
    /**
     * Value to allow the current system call for an alien thread.
     *
     * This value is used in the reply to an alien pre-syscall exception IPC.
     */
    Label_allow_syscall = 1,

    Label_irq = -1L,           ///< IRQ object protocol.
    Label_page_fault = -2L,    ///< Page fault messages use this protocol.
    Label_preemption = -3L,    ///< Preemption IPC protocol. \note unused.
    Label_sys_exception = -4L, ///< Sys exception protocol. \note unused.
    Label_exception  = -5L,    ///< Exception IPC protocol.
    Label_sigma0 = -6L,        ///< Protocol for sigma0 objects.
    Label_io_page_fault = -8L, ///< Protocol for I/O-port page faults.
    Label_kobject = -10L,      ///< Control protocol iD for IPC gates (server
                               ///  side).
    Label_task = -11L,         ///< Protocol ID for task and VM objects.
    Label_thread = -12L,       ///< Protocol ID for thread objects.
    Label_log = -13L,          ///< Protocol ID for log / vcon objects.
    Label_scheduler = -14L,    ///< Protocol ID for scheduler objects.
    Label_factory = -15L,      ///< Protocol ID for factory objects.
    Label_vm = -16L,           ///< Factory ID for VM objects (used for create
                               ///  operations on a factory).
    Label_dma_space = -17L,    ///< Factory ID for an DMA address space
    Label_irq_sender = -18L,   ///< Protocol for IRQ sender objects.
    Label_irq_mux = -19L,      ///< Protocol for IRQ multiplexer objects.
    Label_semaphore = -20L,    ///< Protocol ID for semaphore objects.
    Label_iommu = -22L,        ///< Protocol ID for IOMMUs
    Label_debugger = -23L,     ///< Protocol ID for the debugger
    Max_factory_label = Label_iommu,
  };
private:
  Mword _tag;
};

/**
 * L4 timeouts data type.
 */
class L4_timeout
{
public:
  /// Typical timeout constants.
  enum {
    Never = 0, ///< Never timeout.
    Zero  = 0x400, ///< Zero timeout.
  };

  /**
   * Create the specified timeout.
   * @param man mantissa of the send timeout.
   * @param exp exponent of the send timeout
   *        (exp=0: infinite timeout,
   *        exp>0: t=2^(exp)*man,
   *        man=0 & exp!=0: t=0).
   */
  L4_timeout(Mword man, Mword exp);
  L4_timeout(Mword man, Mword exp, bool clock);

  /**
   * Create a timeout from it's binary representation.
   * @param t the binary timeout value.
   */
  L4_timeout(unsigned short t = 0);

  /**
   * Get the binary representation of the timeout.
   * @return The timeout as binary representation.
   */
  unsigned short raw() const;

  /**
   * Get the receive exponent.
   * @return The exponent of the receive timeout.
   * @see rcv_man()
   */
  Mword exp() const;

  /**
   * Set the exponent of the receive timeout.
   * @param er the exponent for the receive timeout (see L4_timeout()).
   * @see rcv_man()
   */
  void exp(Mword er);

  /**
   * Get the receive timout's mantissa.
   * @return The mantissa of the receive timeout (see L4_timeout()).
   * @see rcv_exp()
   */
  Mword man() const;

  /**
   * Set the mantissa of the receive timeout.
   * @param mr the mantissa of the recieve timeout (see L4_timeout()).
   * @see rcv_exp()
   */
  void man(Mword mr);

  /**
   * Get the relative receive timeout in microseconds.
   * @param clock Current value of kernel clock
   * @return The receive timeout in micro seconds.
   */
  Unsigned64 microsecs_rel(Unsigned64 clock) const;

  /**
   * Get the absolute receive timeout in microseconds.
   * @param clock Current value of kernel clock
   * @return The receive timeout in micro seconds.
   */
  Unsigned64 microsecs_abs(Utcb const *u) const;

private:
  enum
    {
      Clock_mask   = 0x0400,
      Abs_mask     = 0x8000,

      Exp_mask     = 0x7c00,
      Exp_shift    = 10,

      Man_mask     = 0x3ff,
      Man_shift    = 0,
    };

  unsigned short _t;
} __attribute__((packed));

struct L4_timeout_pair
{
  L4_timeout rcv;
  L4_timeout snd;

  L4_timeout_pair(L4_timeout const &rcv, L4_timeout const &snd)
    : rcv(rcv), snd(snd) {}

  L4_timeout_pair(unsigned long v) : rcv(v), snd(v >> 16) {}

  Mword raw() const { return (Mword)rcv.raw() | (Mword)snd.raw() << 16; }
};

/**
 * This class contains constants for the message size for exception IPC.
 *
 * This information is architecture dependent, see #Msg_size.
 */
class L4_exception_ipc
{};

/**
 * Constants for error codes returned by kernel objects.
 */
class L4_err
{
public:
  enum Err
  {
    EPerm         =  1, ///< Permission denied.
    ENoent        =  2, ///< Some object was not found.
    ENomem        = 12, ///< Out of memory.
    EFault        = 14, ///< There was an unresolved page fault
    EBusy         = 16, ///< The object is busy, try again.
    EExists       = 17, ///< Some object does already exist.
    ENodev        = 19, ///< Objects of the specified type cannot be created.
    EInval        = 22, ///< Invalid parameters passed.
    ERange        = 34, ///< Parameter out of range
    ENosys        = 38, ///< No such operation.
    EBadproto     = 39, ///< Protocol not supported by object.

    EAddrnotavail = 99, ///< The given address is not available.
    EMsgtooshort  = 1001, ///< Incoming IPC message too short
  };
};


class L4_cpu_set_descr
{
private:
  Mword _w;

public:
  Order granularity() const
  {
    Mword g = (_w >> 24) & 0xff;
    if (g > 24) g = 24; // limit granularity to 2**24
    return Order(g);
  }

  Cpu_number offset() const
  { return cxx::mask_lsb(Cpu_number(_w & 0x00ffffff), granularity()); }
};

class L4_cpu_set : public L4_cpu_set_descr
{
private:
  Mword _map;

public:
  bool contains(Cpu_number cpu) const
  {
    if (offset() > cpu)
      return false;

    cpu -= offset();
    cpu >>= granularity();
    if (cpu >= Cpu_number(MWORD_BITS))
      return false;

    return (_map >> cxx::int_value<Cpu_number>(cpu)) & 1;
  }

  template<typename MAP>
  Cpu_number first(MAP const &bm, Cpu_number max) const
  {
    Cpu_number cpu = offset();

    for (;;)
      {
        Cpu_number b = (cpu - offset()) >> granularity();
        if (cpu >= max || b >= Cpu_number(MWORD_BITS))
          return max;

        if (!((_map >> cxx::int_value<Cpu_number>(b)) & 1))
          {
            cpu += Cpu_number(1) << granularity();
            continue;
          }

        if (bm.get(cpu))
          return cpu;

        ++cpu;
      }
  }
};

struct L4_sched_param
{
  L4_cpu_set cpus;
  Smword sched_class; // legacy prio when positive
  Mword length;       // sizeof (...)
};

struct L4_sched_param_legacy
{
  L4_cpu_set cpus;
  Smword prio;        // must be positive, overlays with sched_class
  Mword quantum;
};


//----------------------------------------------------------------------------
INTERFACE [ia32 || ux]:

EXTENSION class L4_exception_ipc
{
public:
  enum { Msg_size = 19 };
};

//----------------------------------------------------------------------------
INTERFACE [arm && 32bit]:

EXTENSION class L4_exception_ipc
{
public:
  enum { Msg_size = 21 };
};

//----------------------------------------------------------------------------
INTERFACE [arm && 64bit]:

EXTENSION class L4_exception_ipc
{
public:
  enum { Msg_size = 39 };
};

//----------------------------------------------------------------------------
INTERFACE [amd64]:

EXTENSION class L4_exception_ipc
{
public:
  enum { Msg_size = 26 };
};

//----------------------------------------------------------------------------
INTERFACE [ppc32]:

EXTENSION class L4_exception_ipc
{
public:
  enum { Msg_size = 39 };
};

INTERFACE [sparc]:
EXTENSION class L4_exception_ipc
{
public:
  enum { Msg_size = 12 }; // XXX whatever?
};

//----------------------------------------------------------------------------
INTERFACE:

/**
 * User-level Thread Control Block (UTCB).
 *
 * The UTCB is a virtual extension of the registers of a thread. A UTCB
 * comprises three sets of registers: the message registers (MRs), the buffer
 * registers (BRs and BDR), and the control registers (TCRs).
 *
 * The message registers (MRs) contain the contents of the messages that are
 * sent to objects or received from objects.  The message contents consist of
 * untyped data and typed message items (see L4_msg_tag).  The untyped must be
 * stored in the first \a n message registers (\a n = L4_msg_tag::words()) and
 * are transferred / copied uninterpreted to the receiving object (MRs of
 * receiver thread or kernel object).  The typed items follow starting at MR[\a
 * n+1].  Each typed item is stored in two MRs and is interpreted by the kernel
 * (see L4_msg_item, L4_fpage). The number of items is denoted by
 * L4_msg_tag::items().  On the receiver side the typed items are translated
 * into a format that is useful for the receiver and stored at into the same
 * MRs in the receivers UTCB.
 *
 * The buffer registers (BRs and BDR) contain information that describe receive
 * buffers for incoming typed items.  The contents of these registers are not
 * altered by the kernel. The buffer descriptor register (BDR, Utcb::buf_desc)
 * contains information about the items in the buffer registers (BRs) and
 * flags to enable FPU state transfer.  The BRs contain a set of receive
 * message items (L4_msg_item) that describe receive buffers, such as, virtual
 * memory regions for incoming memory mappings or buffers for capabilities.
 * The BRs are also used to store absolute 64bit timeout values for operations,
 * The value of the timeout pair encodes the number of the BR if an absolute
 * timeout is used.
 *
 * The thread control registers (TCRs) comprise an error code for errors during
 * message passing and a set of user-level registers. The user-level registers
 * are not used by the kernel and provide and anchor for thread-local storage.
 */
class Utcb
{
  /* must be 2^n bytes */
public:

  /**
   * Type for time values in the UTCB (size is fix 64bit).
   *
   * On 32bit architectures this type uses two MRs on 64bit one Mr is used.
   * This type is used for conversion of time values stored in MRs or BRs.
   */
  union Time_val
  {
    enum { Words = sizeof(Cpu_time)/sizeof(Mword) /**< Number of MRs used. */ };
    Mword b[Words]; ///< The array of MRs to use.
    Cpu_time t;     ///< The time value itself.
  };

  enum
  {
    Max_words = 63,  ///< Number of MRs.
    Max_buffers = 58 ///< Number of BRs.
  };

  /// The message registers (MRs).
  union
  {
    Mword         values[Max_words];
    Unsigned64    val64[Max_words / (sizeof(Unsigned64) / sizeof(Mword))];
  } __attribute__((packed));

  static unsigned val64_idx(unsigned validx)
  { return validx / (sizeof(Unsigned64) / sizeof(Mword)); }

  static unsigned val_idx(unsigned val64_idx)
  { return val64_idx * (sizeof(Unsigned64) / sizeof(Mword)); }

  Mword           utcb_addr;

  /// The buffer descriptor register (BDR).
  L4_buf_desc     buf_desc;
  /// The buffer registers (BRs).
  Mword           buffers[Max_buffers];

  /// The error code for IPC (TCR).
  L4_error        error;
  /// \deprecated transfer timeout is not used currently (TCR).
  L4_timeout_pair xfer;
  /// The user-level registers for TLS (TCR).
  Mword           user[3];
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

#include <minmax.h>

/**
 * Receiver is ready to receive FPU contents?
 * \return true if the receiver is ready to receive the state of the FPU as
 *         part of a message.
 * \see L4_buf_desc::Inherit_fpu, L4_buf_desc.
 */
PUBLIC inline
bool Utcb::inherit_fpu() const
{ return buf_desc.flags() & L4_buf_desc::Inherit_fpu; }


/**
 * Create a message tag from its parts.
 * \param words the number of untyped message words to transfer.
 * \param items the number of typed message items, following the untyped words
 *        in the message registers. See L4_msg_item.
 * \param flags the flags, see L4_msg_tag::Flags and L4_msg_tag::Output_flags.
 * \param proto the protocol ID to use.
 */
PUBLIC inline
L4_msg_tag::L4_msg_tag(unsigned words, unsigned items, unsigned long flags,
    unsigned long proto)
  : _tag((words & 0x3f) | ((items & 0x3f) << 6) | flags | (proto << 16))
{}

/**
 * Create an uninitialized message tag.
 * \note the value of the tag is unpredictable.
 */
PUBLIC inline
L4_msg_tag::L4_msg_tag()
{}

/**
 * Create a message tag from another message tag, replacing
 * the L4_msg_tag::Output_flags.
 * \param o the message tag to copy.
 * \param flags the output flags to set in the new tag.
 * \pre (flags & ~Rcv_flags) == 0
 */
PUBLIC inline
L4_msg_tag::L4_msg_tag(L4_msg_tag const &o, Mword flags)
  : _tag((o.raw() & ~Mword(Rcv_flags)) | flags)
{}

/**
 * Create msg tag from the binary representation.
 * \param raw the raw binary representation, as passed from user level.
 */
PUBLIC explicit inline
L4_msg_tag::L4_msg_tag(Mword raw)
  : _tag(raw)
{}

/**
 * Get the protocol ID.
 * \return the protocol ID.
 */
PUBLIC inline
long
L4_msg_tag::proto() const
{ return long(_tag) >> 16; }

/**
 * Get the binary representation.
 * \return the binary value of the tag.
 */
PUBLIC inline
unsigned long
L4_msg_tag::raw() const
{ return _tag; }

/**
 * Get the number of untyped words to deliver.
 * \return number message registers that shall be transferred
 *         uninterpreted to the receiving object.
 */
PUBLIC inline
unsigned L4_msg_tag::words() const
{ return _tag & 63; }

/**
 * Get the number of typed message items in the message.
 * \return the number of typed items, directly following the
 *         untyped words in the message registers.
 * \see L4_msg_item.
 */
PUBLIC inline
unsigned L4_msg_tag::items() const
{ return (_tag >> 6) & 0x3f; }

/**
 * Get the flags of the tag.
 * \return the flags of the message tag, note reserved bits might be
 *         set in the result.
 */
PUBLIC inline
Mword L4_msg_tag::flags() const
{ return _tag; }

/**
 * Transfer the FPU?
 * \return true if the sender wishes to transfer FPU contents.
 * \see #Transfer_fpu.
 */
PUBLIC inline
bool L4_msg_tag::transfer_fpu() const
{ return _tag & Transfer_fpu; }

/**
 * Do time-slice donation?
 * \return true if the sender is willing to donate its remaining time-
 *         slice to the receiver.
 * \see #Schedule.
 */
PUBLIC inline
bool L4_msg_tag::do_switch() const
{ return !(_tag & Schedule); }

/**
 * Set the error flag to \a e.
 * \param e the value of the error flag to be set.
 */
PUBLIC inline
void L4_msg_tag::set_error(bool e = true)
{ if (e) _tag |= Error; else _tag &= ~Mword(Error); }

/**
 * Is there an error flagged?
 * \return true if the error flag of the message tag is set.
 */
PUBLIC inline
bool L4_msg_tag::has_error() const
{ return _tag & Error; }
//
// L4_timeout implementation
//

IMPLEMENT inline L4_timeout::L4_timeout(unsigned short t)
  : _t(t)
{}

IMPLEMENT inline unsigned short L4_timeout::raw() const
{ return _t; }

PUBLIC inline
Mword L4_timeout::abs_exp() const
{ return (_t >> 11) & 0xf; }

PUBLIC inline
bool L4_timeout::abs_clock() const
{ return _t & Clock_mask; }

IMPLEMENT inline
Unsigned64
L4_timeout::microsecs_rel(Unsigned64 clock) const
{
  if (man() == 0)
    return 0;
  else
   return clock + ((Unsigned64)man() << exp());
}

IMPLEMENT inline NEEDS[<minmax.h>]
Unsigned64
L4_timeout::microsecs_abs(Utcb const *u) const
{
  int idx = min<int>(_t & 0x3f, Utcb::Max_buffers);
  Utcb::Time_val const *top
    = reinterpret_cast<Utcb::Time_val const *>(&u->buffers[idx]);
  return top->t;
}

PUBLIC inline
bool
L4_timeout::is_absolute() const
{ return _t & Abs_mask; }

PUBLIC inline
Unsigned64
L4_timeout::microsecs(Unsigned64 clock, Utcb const *u) const
{ 
  if (is_absolute())
    return microsecs_abs(u);
  else
    return microsecs_rel(clock);
}

PUBLIC inline
bool L4_timeout::is_never() const
{ return !_t; }

PUBLIC inline
bool L4_timeout::is_zero() const
{ return _t == Zero; }

PUBLIC inline
unsigned short L4_timeout::is_finite() const
{ return _t; }


//
// L4_timeout implementation
//

IMPLEMENT inline
L4_timeout::L4_timeout(Mword man, Mword exp)
: _t (((man & Man_mask) | ((exp << Exp_shift) & Exp_mask)))
{}

IMPLEMENT inline
L4_timeout::L4_timeout(Mword man, Mword exp, bool clock)
: _t (((man & Man_mask) | ((exp << (Exp_shift+1)) & Exp_mask)
      | (clock ? Clock_mask : 0) | Abs_mask))
{}

IMPLEMENT inline Mword L4_timeout::exp() const
{ return (_t & Exp_mask) >> Exp_shift; }

IMPLEMENT inline void L4_timeout::exp(Mword w)
{ _t = (_t & ~Exp_mask) | ((w << Exp_shift) & Exp_mask); }

IMPLEMENT inline Mword L4_timeout::man() const
{ return (_t & Man_mask) >> Man_shift; }

IMPLEMENT inline void L4_timeout::man (Mword w)
{ _t = (_t & ~Man_mask) | ((w << Man_shift) & Man_mask); }


