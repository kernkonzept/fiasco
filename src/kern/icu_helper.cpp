INTERFACE:

#include "irq.h"
#include "irq_mgr.h"
#include "kobject_helper.h"
#include "kobject_rpc.h"

/**
 * Helper IRQ chip for virtual ICUs.
 *
 * This IRQ chip provides a simple software-only IRQ chip that can bind up to
 * NR_PINS interrupt pins and can be used with the Icu_h<> template.
 */
template<unsigned NR_PINS = 1>
class Irq_chip_virt : public Irq_chip_soft
{
public:
  struct Chip_pin
  {
    Chip_pin() = default;
    Chip_pin(Irq_chip_virt *_chip, Mword _pin) : chip(_chip), pin(_pin) {}

    Irq_chip_virt *chip = nullptr;
    Mword pin;

    Irq_base *irq() const
    { return chip->_pins[pin]; }
  };

  unsigned nr_pins() const { return NR_PINS; }

  Irq_base *icu_irq(Mword pin) const
  {
    if (pin < NR_PINS)
      return _pins[pin];

    return nullptr;
  }

  Chip_pin icu_chip_pin(Mword pin)
  {
    if (pin < NR_PINS)
      return Chip_pin(this, pin);

    return Chip_pin();
  }

  int icu_attach(Mword pin, Irq_base *irq)
  {
    if (pin >= NR_PINS)
      return -L4_err::EInval;

    if (_pins[pin])
      return -L4_err::EInval;

    bind(irq, pin);
    if (cas<Irq_base *>(&_pins[pin], nullptr, irq))
      return 0;

    irq->detach();
    return -L4_err::EInval;
  }

  int icu_info(Mword *features, Mword *num_pins, Mword *num_msis)
  {
    *num_pins = NR_PINS;
    *num_msis = 0;
    *features = 0; // Supported features (only normal IRQs)
    return 0;
  }

  int icu_msi_info(Mword, Unsigned64, Irq_mgr::Msi_info *)
  { return -L4_err::ENosys; }

  void detach(Irq_base *irq) override
  {
    if (irq->chip() != this)
      return;

    Mword pin = irq->pin();
    if (pin >= NR_PINS)
      return;

    if (_pins[pin] != irq)
      return;

    if (cas<Irq_base *>(&_pins[pin], irq, nullptr))
      Irq_chip_soft::detach(irq);
  }

protected:
  Irq_base *const *icu_irq_ptr(Mword pin)
  { return &_pins[pin]; }

private:
  Irq_base *_pins[NR_PINS];
};

class Icu_h_base
{
public:
  enum class Op : Mword
  {
    Bind       = 0,
    Unbind     = 1,
    Info       = 2,
    Msi_info   = 3,
    Eoi        = static_cast<Mword>(Irq::Op::Eoi), // 4
    Unmask     = Eoi,
    Mask       = 5,
    Set_mode   = 6,
  };

  enum Feature : Mword
  {
    Msi_bit = 0x80000000U
  };
};

template<typename REAL_ICU>
class Icu_h : public Kobject_h<REAL_ICU>, public Icu_h_base
{
protected:
  typedef Irq_mgr::Msi_info Msi_info;

  REAL_ICU const *this_icu() const
  { return nonull_static_cast<REAL_ICU const *>(this); }

  REAL_ICU *this_icu()
  { return nonull_static_cast<REAL_ICU *>(this); }

  L4_RPC(Op::Bind,     icu_bind,     (Mword pin, Ko::Cap<Irq> irq));
  L4_RPC(Op::Unbind,   icu_unbind,   (Mword pin, Ko::Cap<Irq> irq));
  L4_RPC(Op::Set_mode, icu_set_mode, (Mword pin, Irq_chip::Mode mode));
  L4_RPC(Op::Info,     icu_info,     (Mword *features, Mword *num_pins, Mword *num_msis));
  L4_RPC(Op::Msi_info, icu_msi_info, (Mword msi, Unsigned64 src_id, Msi_info *info));
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
template<typename REAL_ICU>
void
Icu_h<REAL_ICU>::icu_mask_irq(bool mask, unsigned pin)
{
  Irq_base *irq = this_icu()->icu_irq(pin);

  if (EXPECT_FALSE(!irq))
    return;

  if (mask)
    irq->mask();
  else
    irq->unmask();
}

PUBLIC inline
template<typename REAL_ICU>
L4_msg_tag
Icu_h<REAL_ICU>::op_icu_bind(unsigned pin, Ko::Cap<Irq> const &irq)
{
  if (!Ko::check_rights(irq.rights, Ko::Rights::CW()))
    return Kobject_iface::commit_result(-L4_err::EPerm);

  auto guard = lock_guard(irq.obj->irq_lock());
  irq.obj->detach();

  return Kobject_iface::commit_result(this_icu()->icu_attach(pin, irq.obj));
}

PUBLIC inline
template<typename REAL_ICU>
L4_msg_tag
Icu_h<REAL_ICU>::op_icu_unbind(unsigned pin, Ko::Cap<Irq> const &)
{
  Irq_base *irq = this_icu()->icu_irq(pin);

  if (irq)
    irq->detach();

  return Kobject_iface::commit_result(0);
}

PUBLIC inline
template<typename REAL_ICU>
L4_msg_tag
Icu_h<REAL_ICU>::op_icu_set_mode(Mword pin, Irq_chip::Mode mode)
{
  auto cp = this_icu()->icu_chip_pin(pin);

  if (!cp.chip)
    return Kobject_iface::commit_result(-L4_err::EInval);

  if (cp.pin >= cp.chip->nr_pins())
    return Kobject_iface::commit_result(-L4_err::EInval);

  int ret = cp.chip->set_mode(cp.pin, mode);

  Irq_base *irq = cp.irq();
  if (irq)
    {
      auto guard = lock_guard(irq->irq_lock());
      if (irq->chip() == cp.chip && irq->pin() == cp.pin)
        irq->switch_mode(cp.chip->is_edge_triggered(cp.pin));
    }

  return Kobject_iface::commit_result(ret);
}

PUBLIC inline
template<typename REAL_ICU>
L4_msg_tag
Icu_h<REAL_ICU>::op_icu_info(Mword *features, Mword *num_pins, Mword *num_msis)
{
  return Kobject_iface::commit_result(
    this_icu()->icu_info(features, num_pins, num_msis));
}

PUBLIC inline
template<typename REAL_ICU>
L4_msg_tag
Icu_h<REAL_ICU>::op_icu_msi_info(Mword msi, Unsigned64 source, Irq_mgr::Msi_info *out)
{
  return Kobject_iface::commit_result(
    this_icu()->icu_msi_info(msi, source, out));
}

PUBLIC template< typename REAL_ICU >
inline
L4_msg_tag
Icu_h<REAL_ICU>::icu_invoke(L4_obj_ref, L4_fpage::Rights /*rights*/,
                            Syscall_frame *f,
                            Utcb const *utcb, Utcb *out)
{
  L4_msg_tag tag = f->tag();

  // Ko::check_basics()
  if (EXPECT_FALSE(tag.words() < 1))
    return Kobject_iface::commit_result(-L4_err::EMsgtooshort);

  switch (Icu_h_base::Op{utcb->values[0]})
    {
    case Op::Bind:
      return Msg_icu_bind::call(this_icu(), tag, utcb, out);

    case Op::Unbind:
      return Msg_icu_unbind::call(this_icu(), tag, utcb, out);

    case Op::Info:
      return Msg_icu_info::call(this_icu(), tag, utcb, out);

    case Op::Msi_info:
      return Msg_icu_msi_info::call(this_icu(), tag, utcb, out);

    case Op::Unmask:
    case Op::Mask:
      if (tag.words() < 2)
        return Kobject_h<REAL_ICU>::no_reply();

      this_icu()->icu_mask_irq(Op{utcb->values[0]} == Op::Mask, utcb->values[1]);
      return Kobject_h<REAL_ICU>::no_reply();

    case Op::Set_mode:
      return Msg_icu_set_mode::call(this_icu(), tag, utcb, out);

    default:
      return Kobject_iface::commit_result(-L4_err::ENosys);
    }
}

// This function is actually never used because all users of Icu_h implement
// their own kinvoke() method.
PUBLIC
template< typename REAL_ICU >
L4_msg_tag
Icu_h<REAL_ICU>::kinvoke(L4_obj_ref ref, L4_fpage::Rights rights,
                         Syscall_frame *f,
                         Utcb const *in, Utcb *out)
{
  L4_msg_tag tag = f->tag();

  if (!Ko::check_basics(&tag, L4_msg_tag::Label_irq))
    return tag;

  return icu_invoke(ref, rights, f, in, out);
}
