INTERFACE:

#include "types.h"
#include <cxx/bitfield>
#include <cxx/cxx_int>

/// Difference between two capability indexes
typedef cxx::int_type_order<Mword, struct Cap_diff_t, Order> Cap_diff;

/// A capability index
typedef cxx::int_type_full<Mword, struct Cap_index_t, Cap_diff, Order> Cap_index;

/// IA32 I/O port numbers
typedef cxx::int_type_order<unsigned long, struct Port_number_t, Order> Port_number;

/**
 * An L4 flexpage.
 *
 * A flexpage represents a naturally aligned area of mappable space,
 * such as memory, I/O-ports, and capabilities (kernel objects).
 * There is also a representation for describing a flexpage that represents
 * the whole of all these address spaces. The size of a flexpage is given
 * as a power of two.
 *
 *
 * The internal representation is a single machine word with the following
 * layout:
 *   \verbatim
 *   +- bitsize-12 +- 11-6 -+ 5-4 -+-- 3-0 -+
 *   | page number | order  | type | rights |
 *   +-------------+--------+------+--------+
 *   \endverbatim
 *
 *   - The rights bits (0-3) denote the access rights to an object, see
 *     L4_fpage::Rights.
 *   - The \a type of a flexpage is denotes the address space that is
 *     referenced by that flexpage (see L4_fpage::Type).
 *   - The order is the exponent for the size calculation (size = 2^order).
 *   - The page number denotes the page address within the address space
 *     denoted by \a type. For example when \a type is #Memory, the \a page
 *     \a number must contain the most significant bits of a virtual address
 *     that must be aligned to \a order bits.  In the case that \a type
 *     equals either #Io or #Obj, the \a page \a number contains all bits of
 *     the I/O-port number or the capability index, respectively (note, the
 *     values must also be aligned according to the value of \a order).
 *
 */
class L4_fpage
{
public:
  /**
   * Data type to represent the binary representation of a flexpage.
   */
  typedef Mword Raw;

  /**
   * Address space type of a flexpage.
   */
  enum Type
  {
    Special = 0, ///< Special flexpages, either invalid or all spaces.
    Memory,      ///< Memory flexpage
    Io,          ///< I/O-port flexpage
    Obj          ///< Object flexpage (capabilities)
  };

  enum { Addr_shift = 12 };

  struct Rights
  : cxx::int_type_base<unsigned char, Rights>,
    cxx::int_bit_ops<Rights>,
    cxx::int_null_chk<Rights>
  {
    Rights() = default;

    explicit constexpr Rights(unsigned char v)
    : cxx::int_type_base<unsigned char, Rights>(v) {}

    /// Explicit conversion from the Value_enum type
    explicit constexpr Rights(Value_enum v)
    : cxx::int_type_base<unsigned char, Rights>(v) {}

    /// Explicit conversion to the Value_enum type
    explicit constexpr operator Value_enum () const
    { return static_cast<Value_enum>(_v); }

    constexpr bool empty() const { return _v == 0; }

    constexpr Rights apply(Rights r) const { return *this & r; }

    /// Memory flexpage is user accessible
    static constexpr Rights U() { return Rights(8); }

    /// Memory flexpage is readable
    static constexpr Rights R() { return Rights(4); }

    /// Memory flexpage is writable
    static constexpr Rights W() { return Rights(2); }

    /// Memory flexpage is executable (often equal to #R)
    static constexpr Rights X() { return Rights(1); }

    static constexpr Rights UR() { return U() | R(); }
    static constexpr Rights URX() { return U() | R() | X(); }
    static constexpr Rights URWX() { return U() | R() | W() | X(); }
    static constexpr Rights URW() { return U() | R() | W(); }

    /// Memory flexpage is readable and executable
    static constexpr Rights RX() { return R() | X(); }

    /// Memory flexpage is readable, writeable, and executable
    static constexpr Rights RWX() { return R() | W() | X(); }

    /// Memory flexpage is readable and writable
    static constexpr Rights RW() { return R() | W(); }

    ///< Memory flexpage is writable and executable
    static constexpr Rights WX() { return W() | X(); }


    /// Object flexpage: delete rights
    static constexpr Rights CD() { return Rights(0x8); }

    /// Object flexpage: read rights (w/o this the mapping is not present)
    static constexpr Rights CR() { return Rights(0x4); }

    /** Object flexpage: strong semantics (object specific, i.e. not having
     *  this right on an IPC gate demotes all capabilities transferred via this
     *  IPC gate to also suffer this right. */
    static constexpr Rights CS() { return Rights(0x2); }

    /// Object flexpage: write rights (purely object specific)
    static constexpr Rights CW() { return Rights(0x1); }

    /// Object flexpage: combine #CR and #CW
    static constexpr Rights CRW() { return CR() | CW(); }

    /// Object flexpage: combine #CR and #CS
    static constexpr Rights CRS() { return CR() | CS(); }

    /// Object flexpage: combine #CR, #CW, and #CS
    static constexpr Rights CRWS() { return CRW() | CS(); }

    /// Object flexpage: combine #CS and #CW
    static constexpr Rights CWS() { return CW() | CS(); }

    /// Object flexpage: combine #CS, #CW, and #CD
    static constexpr Rights CWSD() { return CW() | CS() | CD(); }

    /// Object flexpage: combine #CR, #CW, #CS, and #CD
    static constexpr Rights CRWSD() { return CRWS() | CD(); }

    /// All rights shall be transferred, independent of the type
    static constexpr Rights FULL() { return Rights(0xf); }
  };

private:
  /**
   * Create a flexpage with the given parameters.
   */
  constexpr L4_fpage(Type type, Mword address,
                     unsigned char order, Rights rights)
  : _raw(  address | _rights_bfm_t::val_dirty(cxx::int_value<Rights>(rights))
         | order_bfm_t::val_dirty(order) | type_bfm_t::val_dirty(type))
  {}

public:

  enum
  {
    Whole_space = 63 ///< Value to use as \a order for a whole address space.
  };

  /**
   * Create an I/O flexpage.
   *
   * IO flexpages do not support access rights other than RW or nothing.
   * \param port base I/O-port number (0..65535), must be aligned to
   *             2^\a order. The value is shifted by #Addr_shift bits to the
   *             left.
   * \param order the order of the I/O flexpage, size is 2^\a order ports.
   */
  static constexpr L4_fpage io(Mword port, unsigned char order, Rights rights = Rights(0))
  { return L4_fpage(Io, adr_bfm_t::val_dirty(port), order, rights); }

  /**
   * Create an object flexpage.
   *
   * \param idx capability index, note capability indexes are multiples of
   *            0x1000. (hence \a idx is not shifted)
   * \param order The size of the flexpage is 2^\a order. The value in \a idx
   *              must be aligned to 2^(\a order + #Addr_shift.
   */
  static constexpr L4_fpage obj(Mword idx, unsigned char order, Rights rights = Rights(0))
  { return L4_fpage(Obj, idx & adr_bfm_t::Mask, order, rights); }

  /**
   * Create a memory flexpage.
   *
   * \param addr The virtual address. Only the most significant bits are
   *             considered, bits from 0 to \a order-1 are dropped.
   * \param order The size of the flexpage is 2^\a order in bytes.
   */
  static constexpr L4_fpage mem(Mword addr, unsigned char order, Rights rights = Rights(0))
  { return L4_fpage(Memory, addr & adr_bfm_t::Mask, order, rights); }

  /**
   * Create a nil (invalid) flexpage.
   */
  static constexpr L4_fpage nil()
  { return L4_fpage(0); }

  /**
   * Create a special receive flexpage representing
   * all available address spaces at once. This is used
   * for page-fault and exception IPC.
   */
  static constexpr L4_fpage all_spaces(Rights rights = Rights(0))
  { return L4_fpage(Special, 0, Whole_space, rights); }

  /**
   * Create a flexpage from the raw value.
   */
  explicit constexpr L4_fpage(Raw raw) : _raw(raw) {}

  /**
   * The robust type for carrying virtual memory addresses.
   */
  typedef Virt_addr Mem_addr;

  /**
   * Get the virtual address of a memory flexpage.
   *
   * \pre type() must return #Memory to return a valid value.
   * \return The virtual memory base address of the flexpage.
   */
  constexpr Virt_addr mem_address() const
  { return Virt_addr(adr_bfm_t::get_unshifted(_raw)); }

  /**
   * Get the capability address of an object flexpage.
   *
   * \pre type() must return #Obj to return a valid value.
   * \return The capability value (index) of this flexpage.
   *         This value is not shifted, so it is a multiple of 0x1000.
   *         See obj_index() for reference.
   */
  constexpr Mword obj_address() const { return adr_bfm_t::get_unshifted(_raw); }

  /**
   * Get the I/O-port number of an I/O flexpage.
   * \pre type() must return #Io to return a valid value.
   * \return The I/O-port index of this flexpage.
   */
  constexpr Port_number io_address() const { return Port_number{adr()}; }

  /**
   * Get the capability index of an object flexpage.
   *
   * \pre type() must return #Obj to return a valid value.
   * \return The index into the capability table provided by this flexpage.
   *         This value is shifted #Addr_shift to be a real index
   *         (opposed to obj_address()).
   */
  constexpr Cap_index obj_index() const { return Cap_index{adr()}; }

  /**
   * Test for memory flexpage (if type() is #Memory).
   * \return true if type() is #Memory.
   */
  constexpr bool is_mempage() const { return type() == Memory; }

  /**
   * Test for I/O flexpage (if type() is #Io).
   * \return true if type() is #Io.
   */
  constexpr bool is_iopage() const { return type() == Io; }

  /**
   * Test for object flexpage (if type() is #Obj).
   * \return true if type() is #Obj.
   */
  constexpr bool is_objpage() const { return type() == Obj; }


  /**
   * Is the flexpage the whole address space?
   * @return not zero, if the flexpage covers the
   *   whole address space.
   */
  constexpr Mword is_all_spaces() const
  {
    Mword mask = Mword{type_bfm_t::Mask} | Mword{order_bfm_t::Mask};
    return (_raw & mask) == order_bfm_t::val(Whole_space);
  }

  /**
   * Is the flexpage valid?
   * \return not zero if the flexpage
   *    contains a value other than 0.
   */
  constexpr Mword is_valid() const { return _raw; }

  /**
   * Get the binary representation of the flexpage.
   * \return this flexpage in binary representation.
   */
  constexpr Raw raw() const { return _raw; }

private:
  Raw _raw;

  /** \name Rights of the flexpage */
  CXX_BITFIELD_MEMBER( 0,  3, _rights, _raw);

public:
  /** \name Type of the flexpage */
  CXX_BITFIELD_MEMBER( 4,  5, type, _raw);
  /** \name Size (as power of 2) of the flexpage */
  CXX_BITFIELD_MEMBER( 6, 11, order, _raw);
private:
  /** \name Address encoded in the flexpage */
  CXX_BITFIELD_MEMBER(12, MWORD_BITS-1, adr, _raw);

public:

  constexpr Rights rights() const { return Rights(_rights()); }

  /**
   * Rights bits for flexpages.
   *
   * The particular semantics of the rights bits in a flexpage differ depending
   * on the type of the flexpage. For memory there are #R, #W, and #X rights.
   * For I/O-ports there must be #R and #W, to get access. For object
   * (capabilities) there are #CD, #CR, #CS, and #CW rights on the object and
   * additional rights in the map control value of the map operation (see
   * L4_fpage::Obj_map_ctl).
   */


  /**
   * Mask the flexpage with the given rights.
   *
   * \param r Rights mask. All rights missing in the mask are removed.
   *          The semantics depend on the type (type()) of the flexpage.
   */
  void mask_rights(Rights r) { _raw &= (Mword{cxx::int_value<Rights>(r)} | ~_rights_bfm_t::Mask); }
};

