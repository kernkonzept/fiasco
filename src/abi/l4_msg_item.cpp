INTERFACE:

#include "types.h"
#include "l4_fpage.h"
#include <cxx/bitfield>

/**
 * The first word of a message item, either a send item or a receive buffer.
 *
 * L4_msg_item is the first word of a typed message item in system calls (incl.
 * IPC) The L4_msg_item is usually followed by a second word. The
 * interpretation of the second word depends on the contents of the
 * L4_msg_item.
 *
 * A generic message item has the following binary layout.
 * \verbatim
 * +----------------------------+ 3 + 2 .. 0 +
 * |                            | t |        |
 * +----------------------------+---+--------+ \endverbatim
 *
 * Bit 3 (\a t) is the type bit, if t is set the item is a map
 * item. \note Fiasco.OC currently has no support for other types
 * than map items.
 *
 * A map item has a more specific layout:
 * \verbatim
 * +-- x .. 12 --+- 11 .. 8 -+- 7 .. 4 -+ 3 + 2 +- 1 -+ 0 +
 * | hot_spot    |     SBZ   |   attr   | 1 | i | g/s | c |
 * +-------------+-----------+----------+---+---+-----+---+ \endverbatim
 *
 * Bit 0 (\a c), the compound bit: if this bit is set on a send map item
 * the next message item of the same type shall be mapped using the same
 * receive buffer as this send item. The caller should properly use the
 * \a hot_spot to avoid overlapping mappings. Useful to implement
 * scatter-gather behaviour.
 *
 * If the compound bit is set on a receive buffer, the destination task is
 * selected explicitly. In this case, the capability slot of the task is passed
 * explicitly as a third (or second, in case of a small object buffer) word. If
 * unset, the task of the receiving thread is implicitly the destination task.
 *
 * Bit 1 (\a g/s): On a send map item a set \a g bit flags a grant operation.
 * This means, the sender delegates access to the receiver and
 * removes the own rights (basically a move operation). On a receive buffer
 * a set \a s bit flags a small object buffer. This means, the whole buffer
 * item is just a single buffer register in size and provides a receive buffer
 * for a single object mapping, the address of the buffer is stored in the
 * \a hot_spot.
 *
 * Bit 2 (\a i): This bit is defined for small receive buffers only, a set \a i
 * bit denotes that he receiver wants to avoid a full mapping.  Instead, if all
 * preconditions are met, the receiver will get either the label of an Ipc_gate
 * or a capability selector in its message registers.  A label of an Ipc_gate
 * is sent if and only if the receiving thread is in the same task as the
 * thread that is attached to the Ipc_gate.  A capability selector is received
 * if the sending and the receiving thread are in the same task.
 *
 * Bits 7..4 (\a attr): This bits contain extra attributes that influence the
 * mapping itself. For memory mapping these bits contain cacheability
 * information. For object mappings these bits contain extra rights on the
 * object.
 *
 * Bits x..12 (\a hot_spot): These bits are the so called hot spot and are used
 * to disambiguate the cases where either the send flex page or the receive flex
 * page is larger that the other.
 */
class L4_msg_item
{
public:
  enum Type
  {
    Map    = 8,
  };

  /**
   * Create a message item from its binary representation.
   * \param raw is the binary representation of the message item.
   */
  explicit constexpr L4_msg_item(Mword raw) : _raw(raw) {}

  /**
   * Is the item a \a void item?
   * \return true if the item is \a void, false if it is valid.
   */
  bool constexpr is_void() const { return _raw == 0; }

  /**
   * Get the binary representation of the item.
   * \return the binary representation of this item.
   */
  Mword constexpr raw() const { return _raw; }

protected:
  /**
   * The binary representation.
   */
  Mword _raw;

public:
  /** \name Type of the message item
   *
   *  Currently the type must indicate a map item (#L4_msg_item::Map).
   *  \see L4_msg_item::Type
   */
  CXX_BITFIELD_MEMBER_UNSHIFTED( 3, 3, type, _raw);
};


class L4_snd_item : public L4_msg_item
{
private:
  enum
  {
    Addr_shift = 12, ///< number of bits an index must be shifted, or
                     ///  an address must be aligned to, in the control word
  };

public:
  explicit constexpr L4_snd_item(Mword raw) : L4_msg_item(raw) {}

  /**
   * Additional rights for objects (capabilities) apply to the control word of L4_snd_item.
   */
  enum Obj_attribs
  {
    C_weak_ref            = 0x10, ///< Map a weak reference (not counted in the kernel)
    C_ref                 = 0x00, ///< Map a normal reference (counted, if not derived
                                  ///  from a weak reference)

    C_obj_right_1         = 0x20, ///< Some kernel internal, object-type specific right
    C_obj_right_2         = 0x40, ///< Some kernel internal, object-type specific right
    C_obj_right_3         = 0x80, ///< Some kernel internal, object-type specific right

    C_obj_specific_rights = C_obj_right_1 | C_obj_right_2 | C_obj_right_3,
    C_ctl_rights          = C_obj_specific_rights | C_weak_ref,
  };

  /**
   * Additional flags for memory send items.
   *
   * These flags are to control the caching attributes of memory mappings.
   */
  struct Memory_type
  : cxx::int_type_base<unsigned char, Memory_type>,
    cxx::int_bit_ops<Memory_type>
  {
    Memory_type() = default;
    constexpr explicit Memory_type(unsigned char v)
    : cxx::int_type_base<unsigned char, Memory_type>(v) {}

    static constexpr Memory_type Set() { return Memory_type(0x10); }
    static constexpr Memory_type Normal() { return Memory_type(0x20); }
    static constexpr Memory_type Buffered() { return Memory_type(0x40); }
    static constexpr Memory_type Uncached() { return Memory_type(0x00); }
  };

  Memory_type constexpr mem_type() const { return Memory_type(attr() & 0x70); }

  /**
   * Use the same receive buffer for the next send item.
   * \pre The item must be a send item.
   * \return true if the next send item shall be handled with the
   *         same receive buffer as this one.
   */
  Mword constexpr compound() const { return _raw & 1; }

  /**
   * Is the map item actually a grant item?
   * \pre type() == #L4_msg_item::Map
   * \pre The item is a send item.
   * \return true if the sender does a grant operation.
   */
  Mword constexpr is_grant() const { return _raw & 2; }

  /**
   * Create a map item.
   * \param base the hot spot address of the map item.
   */
  static constexpr L4_snd_item map(Mword base) { return L4_snd_item(base | Map); }

  /** \name Attribute bits of the message item
   *  \pre The item is a send item.
   *  \pre type() == #L4_msg_item::Map.
   *
   * The semantics of the extra attributes depends on
   * the type of the second word, the L4_fpage, of the
   * complete send item.
   * \see L4_snd_item::Memory_attribs, L4_snd_item::Obj_attribs
   */
  CXX_BITFIELD_MEMBER_UNSHIFTED( 4, 7, attr, _raw);

  /** \name the hot-spot address encoded in the message item
   *  \note Useful for memory message items. */
  CXX_BITFIELD_MEMBER_UNSHIFTED(Addr_shift, sizeof(_raw)*8-1, address, _raw);

  /** \name the hot-spot index encoded in the message item
   *  \note In particular useful for IO-port receive items. */
  CXX_BITFIELD_MEMBER          (Addr_shift, sizeof(_raw)*8-1, index, _raw);
};


class L4_buf_item : public L4_msg_item
{
public:
  explicit constexpr L4_buf_item(Mword raw) : L4_msg_item(raw) {}

  bool constexpr forward_mappings() const { return _raw & 1; }

  /**
   * Is the buffer item a small object buffer?
   * \pre The item must be a receive buffer.
   * \pre type() == #L4_msg_item::Map
   * \return true if the buffer is a single-word single-object
   *         receive buffer, false else.
   */
  Mword constexpr is_small_obj() const { return _raw & 2; }

  /**
   * Receiver tries to receive an object ID or a
   * capability selector?
   * \pre The item must be a receive buffer.
   * \pre type() == #L4_msg_item::Map
   * \pre is_small_obj() == true
   * \return true if the receiver is willing to receive an object ID
   *         or a capability selector, if possible.
   */
  Mword constexpr is_rcv_id() const { return _raw & 4; }

  /**
   * Get the L4_fpage that represents the small buffer item.
   * The rights are undefined and shall not be used.
   * \pre type() == #L4_msg_item::Map
   * \pre is_small_obj() == true
   * \return the flex page (L4_fpage) representing the single
   *         object slot with index index().
   */
  L4_fpage constexpr get_small_buf() const
  { return L4_fpage::obj(_raw, 0, L4_fpage::Rights(0)); }

  /**
   * Create a map item.
   */
  static constexpr L4_buf_item map() { return L4_buf_item(Map); }
};


class L4_rcv_item_writer
{
  Mword *_words;
public:
  explicit constexpr L4_rcv_item_writer(Mword words[], L4_snd_item snd, L4_fpage fp)
    : _words(words)
  { _words[0] = (snd.raw() & ~0x0ff6) | (fp.raw() & 0x0ff0); }

  enum Rcv_type
  {
    Rcv_map_something = 0,  ///< Mapping done, something mapped.
    Rcv_map_nothing   = 2,  ///< Mapping done, nothing mapped.
    Rcv_id            = 4,  ///< IPC gate label and permissions are transferred.
    Rcv_flexpage      = 6,  ///< A flexpage word is transferred.
    Rcv_type_mask     = 6
  };

  void constexpr set_rcv_type_map_nothing()
  {
    assert(!(_words[0] & Rcv_type_mask));
    _words[0] |= L4_rcv_item_writer::Rcv_map_nothing;
  }

  void constexpr set_rcv_type_id(Mword id, Mword rights)
  {
    assert(!(_words[0] & Rcv_type_mask));
    _words[0] |= L4_rcv_item_writer::Rcv_id;
    _words[1]  = id | rights;
  }

  void constexpr set_rcv_type_flexpage(L4_fpage sfp)
  {
    assert(!(_words[0] & Rcv_type_mask));
    _words[0] |= L4_rcv_item_writer::Rcv_flexpage;
    _words[1]  = sfp.raw();
  }
};
