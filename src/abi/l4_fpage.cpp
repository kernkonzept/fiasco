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
 * A L4 flex page.
 *
 * A flex page represents a naturally aligned area of mappable space,
 * such as memory, I/O-ports, and capabilities (kernel objects).
 * There is also a representation for describing a flex page that represents
 * the whole of all these address spaces. The size of a flex page is given
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
 *   - The \a type of a flex page is denotes the address space that is
 *     referenced by that flex page (see L4_fpage::Type).
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
   * Data type to represent the binary representation of a flex page.
   */
  typedef Mword Raw;

  /**
   * Address space type of a flex page.
   */
  enum Type
  {
    Special = 0, ///< Special flex pages, either invalid or all spaces.
    Memory,      ///< Memory flex page
    Io,          ///< I/O-port flex page
    Obj          ///< Object flex page (capabilities)
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

    /// Allow implicit conversion from the safe Value_enum type
    constexpr Rights(Value_enum v)
    : cxx::int_type_base<unsigned char, Rights>(v) {}

    /// Allow implicit conversion to the safe Value_enum type
    constexpr operator Value_enum () const
    { return static_cast<Value_enum>(_v); }

    constexpr Rights apply(Rights r) const { return *this & r; }

    /// Memory flex page is user accessible
    static constexpr Rights U() { return Rights(8); }

    /// Memory flex page is readable
    static constexpr Rights R() { return Rights(4); }

    /// Memory flex page is writable
    static constexpr Rights W() { return Rights(2); }

    /// Memory flex page is executable (often equal to #R)
    static constexpr Rights X() { return Rights(1); }

    static constexpr Rights UR() { return U() | R(); }
    static constexpr Rights URX() { return U() | R() | X(); }
    static constexpr Rights URWX() { return U() | R() | W() | X(); }
    static constexpr Rights URW() { return U() | R() | W(); }

    /// Memory flex page is readable and executable
    static constexpr Rights RX() { return R() | X(); }

    /// Memory flex page is readable, writeable, and executable
    static constexpr Rights RWX() { return R() | W() | X(); }

    /// Memory flex page is readable and writable
    static constexpr Rights RW() { return R() | W(); }

    ///< Memory flex page is writable and executable
    static constexpr Rights WX() { return W() | X(); }


    /// Object flex page: delete rights
    static constexpr Rights CD() { return Rights(0x8); }

    /// Object flex page: read rights (w/o this the mapping is not present)
    static constexpr Rights CR() { return Rights(0x4); }

    /** Object flex page: strong semantics (object specific, i.e. not having
     *  this right on an IPC gate demotes all capabilities transferred via this
     *  IPC gate to also suffer this right. */
    static constexpr Rights CS() { return Rights(0x2); }

    /// Object flex page: write rights (purely object specific)
    static constexpr Rights CW() { return Rights(0x1); }

    /// Object flex page: combine #CR and #CW
    static constexpr Rights CRW() { return CR() | CW(); }

    /// Object flex page: combine #CR and #CS
    static constexpr Rights CRS() { return CR() | CS(); }

    /// Object flex page: combine #CR, #CW, and #CS
    static constexpr Rights CRWS() { return CRW() | CS(); }

    /// Object flex page: combine #CS and #CW
    static constexpr Rights CWS() { return CW() | CS(); }

    /// Object flex page: combine #CS, #CW, and #CD
    static constexpr Rights CWSD() { return CW() | CS() | CD(); }

    /// Object flex page: combine #CR, #CW, #CS, and #CD
    static constexpr Rights CRWSD() { return CRWS() | CD(); }

    /// All rights shall be transferred, independent of the type
    static constexpr Rights FULL() { return Rights(0xf); }
  };

private:
  /**
   * Create a flex page with the given parameters.
   */
  L4_fpage(Type type, Mword address, unsigned char order,
           Rights rights)
  : _raw(  address | _rights_bfm_t::val_dirty(cxx::int_value<Rights>(rights))
         | order_bfm_t::val_dirty(order) | type_bfm_t::val_dirty(type))
  {}

public:

  enum
  {
    Whole_space = 63 ///< Value to use as \a order for a whole address space.
  };

  /**
   * Create an I/O flex page.
   *
   * IO flex pages do not support access rights other than RW or nothing.
   * \param port base I/O-port number (0..65535), must be aligned to
   *             2^\a order. The value is shifted by #Addr_shift bits to the
   *             left.
   * \param order the order of the I/O flex page, size is 2^\a order ports.
   */
  static L4_fpage io(Mword port, unsigned char order)
  { return L4_fpage(Io, adr_bfm_t::val_dirty(port), order, Rights(0)); }

  /**
   * Create an object flex page.
   *
   * \param idx capability index, note capability indexes are multiples of
   *            0x1000. (hence \a idx is not shifted)
   * \param order The size of the flex page is 2^\a order. The value in \a idx
   *              must be aligned to 2^(\a order + #Addr_shift.
   */
  static L4_fpage obj(Mword idx, unsigned char order, Rights rights = Rights(0))
  { return L4_fpage(Obj, idx & adr_bfm_t::Mask, order, rights); }

  /**
   * Create a memory flex page.
   *
   * \param addr The virtual address. Only the most significant bits are
   *             considered, bits from 0 to \a order-1 are dropped.
   * \param order The size of the flex page is 2^\a order in bytes.
   */
  static L4_fpage mem(Mword addr, unsigned char order, Rights rights = Rights(0))
  { return L4_fpage(Memory, addr & adr_bfm_t::Mask, order, rights); }

  /**
   * Create a nil (invalid) flex page.
   */
  static L4_fpage nil()
  { return L4_fpage(0); }

  /**
   * Create a special receive flex page representing
   * all available address spaces at once. This is used
   * for page-fault and exception IPC.
   */
  static L4_fpage all_spaces(Rights rights = Rights(0))
  { return L4_fpage(Special, 0, Whole_space, rights); }

  /**
   * Create a flex page from the raw value.
   */
  explicit L4_fpage(Raw raw) : _raw(raw) {}

  /**
   * The robust type for carrying virtual memory addresses.
   */
  typedef Virt_addr Mem_addr;

  /**
   * Get the virtual address of a memory flex page.
   *
   * \pre type() must return #Memory to return a valid value.
   * \return The virtual memory base address of the flex page.
   */
  Virt_addr mem_address() const
  { return Virt_addr(adr_bfm_t::get_unshifted(_raw)); }

  /**
   * Get the capability address of an object flex page.
   *
   * \pre type() must return #Obj to return a valid value.
   * \return The capability value (index) of this flex page.
   *         This value is not shifted, so it is a multiple of 0x1000.
   *         See obj_index() for reference.
   */
  Mword obj_address() const { return adr_bfm_t::get_unshifted(_raw); }

  /**
   * Get the I/O-port number of an I/O flex page.
   * \pre type() must return #Io to return a valid value.
   * \return The I/O-port index of this flex page.
   */
  Port_number io_address() const { return (Port_number)(unsigned)adr(); }

  /**
   * Get the capability index of an object flex page.
   *
   * \pre type() must return #Obj to return a valid value.
   * \return The index into the capability table provided by this flex page.
   *         This value is shifted #Addr_shift to be a real index
   *         (opposed to obj_address()).
   */
  Cap_index obj_index() const { return Cap_index((Mword)adr()); }

  /**
   * Test for memory flex page (if type() is #Memory).
   * \return true if type() is #Memory.
   */
  bool is_mempage() const { return type() == Memory; }

  /**
   * Test for I/O flex page (if type() is #Io).
   * \return true if type() is #Io.
   */
  bool is_iopage() const { return type() == Io; }

  /**
   * Test for object flex page (if type() is #Obj).
   * \return true if type() is #Obj.
   */
  bool is_objpage() const { return type() == Obj; }


  /**
   * Is the flex page the whole address space?
   * @return not zero, if the flex page covers the
   *   whole address space.
   */
  Mword is_all_spaces() const
  { return (_raw & (type_bfm_t::Mask | order_bfm_t::Mask)) == order_bfm_t::val(Whole_space); }

  /**
   * Is the flex page valid?
   * \return not zero if the flex page
   *    contains a value other than 0.
   */
  Mword is_valid() const { return _raw; }

  /**
   * Get the binary representation of the flex page.
   * \return this flex page in binary representation.
   */
  Raw raw() const { return _raw; }

private:
  Raw _raw;

  /** \name Rights of the flex page */
  CXX_BITFIELD_MEMBER( 0,  3, _rights, _raw);

public:
  /** \name Type of the flex page */
  CXX_BITFIELD_MEMBER( 4,  5, type, _raw);
  /** \name Size (as power of 2) of the flex page */
  CXX_BITFIELD_MEMBER( 6, 11, order, _raw);
private:
  /** \name Address encoded in the flex page */
  CXX_BITFIELD_MEMBER(12, MWORD_BITS-1, adr, _raw);

public:

  Rights rights() const { return Rights(_rights()); }

  /**
   * Rights bits for flex pages.
   *
   * The particular semantics of the rights bits in a flex page differ depending on the
   * type of the flex page. For memory there are #R, #W, and #X rights. For
   * I/O-ports there must be #R and #W, to get access. For object (capabilities)
   * there are #CD, #CR, #CS, and #CW rights on the object and additional
   * rights in the map control value of the map operation (see L4_fpage::Obj_map_ctl).
   */


  /**
   * Remove the given rights from this flex page.
   * \param r the rights to remove. The semantics depend on the
   *          type (type()) of the flex page.
   */
  void mask_rights(Rights r) { _raw &= (Mword(cxx::int_value<Rights>(r)) | ~_rights_bfm_t::Mask); }
};

