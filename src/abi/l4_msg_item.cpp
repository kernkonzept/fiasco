INTERFACE:

#include "types.h"
#include "l4_fpage.h"
#include <cxx/bitfield>

/**
 * The first word of a message item, either a send item or a receive buffer.
 *
 * #L4_msg_item is the first word of a typed message item in system calls (incl.
 * IPC). The #L4_msg_item may be followed by additional words that are part of
 * the typed message item. The number of following words and their
 * interpretation depend on the contents of the #L4_msg_item.
 *
 * If all bits of the first word of a typed message item are zero, then it is a
 * void item (see is_void()).
 *
 * The first word of a generic non-void typed message item has the following
 * binary layout:
 *
 *      MSB                        4  3  2      0  bits
 *     ┌────────────────────────────┬───┬────────┐
 *     │                            │ t │        │
 *     └────────────────────────────┴───┴────────┘
 *
 * Bit 3 (`t`) is the type bit. If `t` is set, the item is a map item.
 * \note Fiasco.OC currently has no support for other types than map items.
 *
 * There are three sub-types of typed message items with variations in the
 * layout of the first word:
 *
 * 1. Typed message items set by the sender in its message registers.
 * 2. Typed message items set by the receiver in its buffer registers.
 * 3. Typed message items set by the kernel in the receiver’s message registers.
 *
 * They are abbreviated by *send item*, *buffer item*, and *return item*,
 * respectively.
 *
 * The first word of the first two can be interpreted by the sub-classes
 * #L4_snd_item and #L4_buf_item. The latter can be written by the independent
 * class #L4_return_item_writer.
 *
 * A typed message item in the message registers (case 1 and case 3) always
 * consists of two words (even if it is a void item). The size of a typed
 * message item in the buffer registers (case 2) is determined by its first
 * word. A void item in the buffer registers consists of a single word.
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


/**
 * This class represents the first word of a typed message item set by the
 * sender in its message registers (send item).
 *
 * The second word of a non-void send item is a flexpage (not represented by
 * this class). The type of the flexpage determines the interpretation of the
 * `attr` bits (see below).
 *
 * The getters of this sub-class are defined only if this is a non-void item
 * (see is_void()).
 *
 * If not void, the layout is defined as follows:
 *
 *      MSB     12 11  8 7    4  3    2      1        0       bits
 *     ┌──────────┬─────┬──────┬───┬─────┬───────┬──────────┐
 *     │ hot_spot │ SBZ │ attr │ 1 │ SBZ │ grant │ compound │
 *     └──────────┴─────┴──────┴───┴─────┴───────┴──────────┘
 *
 * `SBZ` means “should be zero”.
 *
 * Bit 0 (`compound`): If this bit is set on a send item, the next send item
 * with the same flexpage type shall be mapped using the same buffer item as
 * this send item. The caller should properly use the `hot_spot` to avoid
 * overlapping mappings. Useful to implement scatter-gather behaviour.
 *
 * Bit 1 (`grant`): On a send map item, a set `grant` bit flags a grant
 * operation. This means, the sender delegates access to the receiver and
 * removes the own rights (basically a move operation).
 *
 * Bits 7..4 (`attr`): These bits contain extra attributes that influence the
 * mapping itself. The concrete meaning is determined by the type of the
 * flexpage in the second word of the send item. For memory mappings, these bits
 * contain cacheability information (see L4_snd_item::Memory_type). For object
 * mappings these bits contain extra rights on the object (see #Obj_attribs).
 *
 * Bits MSB­..12 (`hot_spot`): These bits are called *hot spot* and are used to
 * disambiguate the cases where the send flexpage and the receive flexpage have
 * different sizes.
 */
class L4_snd_item : public L4_msg_item
{
private:
  enum
  {
    Addr_shift = 12, ///< number of bits an index must be shifted, or
                     ///  an address must be aligned to, in the control word
  };

public:
  /**
   * Create a from binary representation.
   */
  explicit constexpr L4_snd_item(Mword raw) : L4_msg_item(raw) {}

  /**
   * Additional rights for object send items.
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
   * Use the same buffer item for the next send item.
   * \return true if the next send item shall be handled with the
   *         same buffer item as this one.
   */
  bool constexpr compound() const { return _raw & 1; }

  /**
   * Is the map item actually a grant item?
   * \return true if the sender does a grant operation.
   */
  bool constexpr is_grant() const { return _raw & 2; }

  /**
   * Create a map item.
   * \param base the hot spot address of the map item.
   */
  static constexpr L4_snd_item map(Mword base) { return L4_snd_item(base | Map); }

  /** \name Attribute bits of the send item
   *
   * The semantics of the extra attributes depends on
   * the type of the second word, the #L4_fpage, of the
   * complete send item.
   * \see L4_snd_item::Memory_type, #Obj_attribs
   */
  CXX_BITFIELD_MEMBER_UNSHIFTED( 4, 7, attr, _raw);

  /** \name the hot-spot address encoded in the send item
   *  \note Useful for memory send items. */
  CXX_BITFIELD_MEMBER_UNSHIFTED(Addr_shift, sizeof(_raw)*8-1, address, _raw);

  /** \name the hot-spot index encoded in the send item
   *  \note In particular useful for IO-port send items. */
  CXX_BITFIELD_MEMBER          (Addr_shift, sizeof(_raw)*8-1, index, _raw);
};


/**
 * This class represents the first word of a message item in the buffer
 * registers of the UTCB (buffer item).
 *
 * The getters of this sub-class are defined only if this is a non-void item
 * (see is_void()).
 *
 * If not void, the general layout is defined as follows:
 *
 *      MSB                      4  3      2       1      0    bits
 *     ┌──────────────────────────┬───┬────────┬───────┬─────┐
 *     │                          │ 1 │ rcv_id │ small │ fwd │
 *     └──────────────────────────┴───┴────────┴───────┴─────┘
 *
 * The `small` and `fwd` bits determine the details of the layout of the whole
 * message item.
 *
 * If `small` is unset, then also `rcv_id` must be unset, and the most
 * significant bits should be zero:
 *
 *     ┌──────────────────────────┬───┬────────┬───────┬─────┐
 *     │   SBZ (should be zero)   │ 1 │   0    │   0   │ fwd │
 *     └──────────────────────────┴───┴────────┴───────┴─────┘
 *
 * If `small` is set, the most significant bits are layouted as follows:
 *
 *      MSB        12 11         4  3      2       1      0    bits
 *     ┌─────────────┬────────────┬───┬────────┬───────┬─────┐
 *     │ rcv cap idx │    SBZ     │ 1 │ rcv_id │   1   │ fwd │
 *     └─────────────┴────────────┴───┴────────┴───────┴─────┘
 *
 * At most one of `rcv_id` and `fwd` may be set.
 *
 * This class only represents the first word of a buffer item. However, it
 * determines the number and meaning of the words in the whole item. They are
 * determined by the `small` and `fwd` bits:
 *
 *      rcv_id small  fwd
 *     ─┬─────┬─────┬─────┐┌───────────────────┐
 *      │  0  │  0  │  0  ││   rcv flexpage    │
 *     ─┴─────┴─────┴─────┘└───────────────────┘            12 11  0
 *     ─┬─────┬─────┬─────┐┌───────────────────┐┌─────────────┬─────┐
 *      │  0  │  0  │  1  ││   rcv flexpage    ││ fwd cap idx │ SBZ │
 *     ─┴─────┴─────┴─────┘└───────────────────┘└─────────────┴─────┘
 *     ─┬─────┬─────┬─────┐
 *      │ 0/1 │  1  │  0  │
 *     ─┴─────┴─────┴─────┘            12 11  0
 *     ─┬─────┬─────┬─────┐┌─────────────┬─────┐
 *      │  0  │  1  │  1  ││ fwd cap idx │ SBZ │
 *     ─┴─────┴─────┴─────┘└─────────────┴─────┘
 *
 * The meaning of the bits in detail:
 *
 * - Bit 0 (`fwd`): If `fwd` is unset, the task of the receiving thread is
 *   implicitly the destination task for any received capabilites. If the `fwd`
 *   bit is set, the received capabilities are forwarded to the destination task
 *   explicitly selected via the capability index in the second or third word of
 *   the buffer item (`fwd cap idx`).
 *
 * - Bit 1 (`small`): A buffer item needs to specify a *receive window*. The
 *   receive window determines which kind of capabilities (object, memory, I/O
 *   ports) may be received where in the respective space. If `small` is 0, the
 *   receive window is specified in the second word of the item via a flexpage
 *   (`rcv flexpage`). If `small` is 1, the receive window consists of a single
 *   capability index in the object space and the capability index is specified
 *   in the most significant bits of the first word starting at bit 12
 *   (`rcv cap idx`).
 *
 * - Bit 2 (`rcv_id`): This bit may be set only if `small` is set and `fwd` is
 *   unset. A set `rcv_id` bit denotes that the receiver wants to avoid a full
 *   mapping. Instead, if all preconditions are met, the receiver will get
 *   either the sent flexpage word or the label of an IPC gate (disjoined with
 *   some of the sender’s permissions) in its message registers. If the `rcv_id`
 *   bit is set:
 *
 *   - If the sending and the receiving thread are in the same task, then the
 *     flexpage word specified by the sender is received and nothing is mapped.
 *   - Otherwise, if the IPC was sent via an IPC gate and the receiving thread
 *     is in the same task as the thread that is attached to the IPC gate, then
 *     the label of the IPC gate disjoined with some permissions of the the sent
 *     IPC gate capability is received.
 *   - Otherwise the behavior is the same as if the `rcv_id` bit was not set.
 */
class L4_buf_item : public L4_msg_item
{
public:
  explicit constexpr L4_buf_item(Mword raw) : L4_msg_item(raw) {}

  /**
   * Is the destination task specified explicitly in a separate word?
   */
  bool constexpr forward_mappings() const { return _raw & 1; }

  /**
   * Is the buffer item a small object buffer?
   * \return true if the buffer is a single-word single-object
   *         receive buffer, false else.
   */
  bool constexpr is_small_obj() const { return _raw & 2; }

  /**
   * Receiver tries to receive an IPC gate label or the sent flexpage word?
   * \pre is_small_obj() && !has_dst_task()
   */
  bool constexpr is_rcv_id() const { return _raw & 4; }

  /**
   * Get the #L4_fpage that represents the small buffer item.
   * The rights are undefined and shall not be used.
   * \pre is_small_obj() == true
   * \return the flexpage (#L4_fpage) representing the single
   *         object slot with index `rcv cap idx`.
   */
  L4_fpage constexpr get_small_buf() const
  {
    assert(is_small_obj());
    return L4_fpage::obj(_raw, 0, L4_fpage::Rights(0));
  }

  /**
   * Create a map item.
   */
  static constexpr L4_buf_item map() { return L4_buf_item(Map); }
};


/**
 * Helper class for writing a typed message item in the message registers of the
 * receiver (return item).
 *
 * \note This class can only write non-void items.
 *
 * A return item always consists of two words. The general layout is defined as
 * follows:
 *
 *      MSB     12 11    6 5    4  3  2        1  0   bits
 *     ┌──────────┬───────┬──────┬───┬──────────┬───┐┌─────────────────┐
 *     │ hot_spot │ order │ type │ 1 │ rcv_type │ c ││     payload     │
 *     └──────────┴───────┴──────┴───┴──────────┴───┘└─────────────────┘
 *     └──────────┘              └───┘          └───┘ from send item‘s first word
 *                └──────────────┘                    from send item‘s flexpage
 *                                   └──────────┘     initially zero
 *
 * As indicated above, the `hot_spot`, `1` (see L4_msg_item::Map), and `c`
 * (`compound`) are copied from the sender’s send item’s first word (see
 * #L4_snd_item), and `order` and `type` are copied from the sender’s send
 * item’s flexpage. The `rcv_type` and `payload` are determined by what is
 * actually transferred, which is also affected by the `rcv_id` bit in the
 * receiver‘s buffer item (see #L4_buf_item). The `rcv_type` determines the
 * content and layout of the payload.
 *
 * There are four cases for `rcv_type`:
 *
 * #Rcv_map_something: Used if at least one mapping was actually transferred for
 * the corresponding send item. The payload is left unset:
 *
 *     ┌──────────┬───────┬──────┬───┬──────────┬───┐┌─────────────────┐
 *     │ hot_spot │ order │ type │ 1 │    00    │ c ││      unset      │
 *     └──────────┴───────┴──────┴───┴──────────┴───┘└─────────────────┘
 *
 * #Rcv_map_nothing: Used if transfer of mappings was attempted, but actually
 * nothing was transferred, because nothing was mapped on the sender’s side for
 * the corresponding send item. The payload is left unset:
 *
 *     ┌──────────┬───────┬──────┬───┬──────────┬───┐┌─────────────────┐
 *     │ hot_spot │ order │ type │ 1 │    01    │ c ││      unset      │
 *     └──────────┴───────┴──────┴───┴──────────┴───┘└─────────────────┘
 *
 * #Rcv_id: Used if the buffer item’s `rcv_id` bit was set and the conditions
 * for transferring an IPC gate label were fulfilled. In that case, the payload
 * consists of the IPC gate label (`obj_id`; see Kobject_common::obj_id())
 * disjoined with the write and special permissions (see
 * Obj::Capability::rights()) of the sent capability (`rights`):
 *
 *                                                           2 1      0
 *     ┌──────────┬───────┬──────┬───┬──────────┬───┐┌────────┬────────┐
 *     │ hot_spot │ order │ type │ 1 │    10    │ c ││ obj_id │ rights │
 *     └──────────┴───────┴──────┴───┴──────────┴───┘└────────┴────────┘
 *
 * #Rcv_flexpage: Used if the buffer item’s `rcv_id` bit was set and the
 * conditions for transferring the sender’s flexpage word were fulfilled. In
 * that case, the payload is a copy of the sender’s flexpage word:
 *
 *     ┌──────────┬───────┬──────┬───┬──────────┬───┐┌─────────────────┐
 *     │ hot_spot │ order │ type │ 1 │    11    │ c ││  send flexpage  │
 *     └──────────┴───────┴──────┴───┴──────────┴───┘└─────────────────┘
 */
class L4_return_item_writer
{
  /** Pointer to the first word of the written return item. */
  Mword *_words;
public:
  /**
   * Create a writer and immediately write the information from the
   * corresponding send item.
   *
   * The `rcv_type` is initially set to #Rcv_map_something.
   *
   * \param regs  Pointer to the first of two words of the return item to be
   *              written.
   * \param snd   First word of the corresponding send item.
   * \param fp    Second word of the corresponding send item.
   */
  explicit constexpr L4_return_item_writer(Mword words[], L4_snd_item snd, L4_fpage fp)
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

  /**
   * Set the `rcv_type` to #Rcv_map_nothing.
   *
   * \pre No `set_…` member must have been called on this instance before.
   */
  void constexpr set_rcv_type_map_nothing()
  {
    _words[0] |= L4_return_item_writer::Rcv_map_nothing;
  }

  /**
   * Set the `rcv_type` to #Rcv_id and set the second word to the the
   * disjunction of the provided object id and permissions.
   *
   * \param id      IPC gate label (see Kobject_common::obj_id()) of the sent
   *                IPC gate.
   * \param rights  Write and special permissions of the sent capability (see
   *                Obj::Capability::rights()).
   *
   * \pre No `set_…` member must have been called on this instance before.
   */
  void constexpr set_rcv_type_id(Mword id, Mword rights)
  {
    _words[0] |= L4_return_item_writer::Rcv_id;
    _words[1]  = id | rights;
  }

  /**
   * Set the `rcv_type` to #Rcv_flexpage and set the second word to the provided
   * flexpage.
   *
   * \param sfp  Flexpage of the corresponding send item.
   *
   * \pre No `set_…` member must have been called on this instance before.
   */
  void constexpr set_rcv_type_flexpage(L4_fpage sfp)
  {
    _words[0] |= L4_return_item_writer::Rcv_flexpage;
    _words[1]  = sfp.raw();
  }
};
