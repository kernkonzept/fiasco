INTERFACE [i8259]:

#include "atomic.h"
#include "lock_guard.h"
#include "irq_chip.h"
#include "spin_lock.h"
#include "types.h"
#include "pm.h"
#include "bitmap.h"

template<typename IO>
class Irq_chip_i8259 : public Irq_chip_icu, private Pm_object
{
  friend class Jdb_kern_info_pic_state;

public:
  enum { Nr_pins = 16 };

  typedef typename IO::Port_addr Io_address;
  using Pm_object::register_pm;

  unsigned nr_pins() const override { return Nr_pins; }
  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  bool set_cpu(Mword, Cpu_number) override { return false; }

protected:
  mutable Spin_lock<> _lock;

private:
  enum
  {
    OFF_ICW = 0x00,
    OFF_OCW = 0x01,
  };

  enum
  {
    // ICW1
    ICW_TEMPLATE    = 0x10,

    LEVL_TRIGGER    = 0x08,
    EDGE_TRIGGER    = 0x00,
    ADDR_INTRVL4    = 0x04,
    ADDR_INTRVL8    = 0x00,
    SINGLE__MODE    = 0x02,
    CASCADE_MODE    = 0x00,
    ICW4__NEEDED    = 0x01,
    NO_ICW4_NEED    = 0x00,

    // ICW3
    SLAVE_ON_IR0    = 0x01,
    SLAVE_ON_IR1    = 0x02,
    SLAVE_ON_IR2    = 0x04,
    SLAVE_ON_IR3    = 0x08,
    SLAVE_ON_IR4    = 0x10,
    SLAVE_ON_IR5    = 0x20,
    SLAVE_ON_IR6    = 0x40,
    SLAVE_ON_IR7    = 0x80,

    I_AM_SLAVE_0    = 0x00,
    I_AM_SLAVE_1    = 0x01,
    I_AM_SLAVE_2    = 0x02,
    I_AM_SLAVE_3    = 0x03,
    I_AM_SLAVE_4    = 0x04,
    I_AM_SLAVE_5    = 0x05,
    I_AM_SLAVE_6    = 0x06,
    I_AM_SLAVE_7    = 0x07,

    // ICW4
    SNF_MODE_ENA    = 0x10,
    SNF_MODE_DIS    = 0x00,
    BUFFERD_MODE    = 0x08,
    NONBUFD_MODE    = 0x00,
    AUTO_EOI_MOD    = 0x02,
    NRML_EOI_MOD    = 0x00,
    I8086_EMM_MOD   = 0x01,
    SET_MCS_MODE    = 0x00,

    // OCW1
    PICM_MASK       = 0xFF,
    PICS_MASK       = 0xFF,

    // OCW2
    NON_SPEC_EOI    = 0x20,
    SPECIFIC_EOI    = 0x60,
    ROT_NON_SPEC    = 0xa0,
    SET_ROT_AEOI    = 0x80,
    RSET_ROTAEOI    = 0x00,
    ROT_SPEC_EOI    = 0xe0,
    SET_PRIORITY    = 0xc0,
    NO_OPERATION    = 0x40,

    SND_EOI_IR0    = 0x00,
    SND_EOI_IR1    = 0x01,
    SND_EOI_IR2    = 0x02,
    SND_EOI_IR3    = 0x03,
    SND_EOI_IR4    = 0x04,
    SND_EOI_IR5    = 0x05,
    SND_EOI_IR6    = 0x06,
    SND_EOI_IR7    = 0x07,

    // OCW3
    OCW_TEMPLATE    = 0x08,
    SPECIAL_MASK    = 0x40,
    MASK_MDE_SET    = 0x20,
    MASK_MDE_RST    = 0x00,
    POLL_COMMAND    = 0x04,
    NO_POLL_CMND    = 0x00,
    READ_NEXT_RD    = 0x02,
    READ_IR_ONRD    = 0x00,
    READ_IS_ONRD    = 0x01,

    // Standard PIC initialization values for PCs.
    PICM_ICW1       = ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8
                      | CASCADE_MODE | ICW4__NEEDED,
    PICM_ICW3       = SLAVE_ON_IR2,
    PICM_ICW4       = SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD
                      | I8086_EMM_MOD,

    PICS_ICW1       = ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8
                      | CASCADE_MODE | ICW4__NEEDED,
    PICS_ICW3       = I_AM_SLAVE_2,
    PICS_ICW4       = SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD
                      | I8086_EMM_MOD,
  };

  Unsigned8 read_ocw(Io_address base)
  { return IO::in8(base + OFF_OCW); }

  void write_ocw(Unsigned8 val, Io_address base)
  { IO::out8(val, base + OFF_OCW); }

  Unsigned8 read_icw(Io_address base)
  { return IO::in8(base + OFF_ICW); }

  void write_icw(Unsigned8 val, Io_address base)
  { IO::out8(val, base + OFF_ICW); }

  void iodelay() const { IO::iodelay(); }

  // Power-management hooks
  void pm_on_suspend(Cpu_number) override
  { _pm_saved_state = disable_all_save(); }

  void pm_on_resume(Cpu_number) override
  { restore_all(_pm_saved_state); }

  Io_address _master;
  Io_address _slave;
  bool _sfn = false;

  // Power-management state
  Unsigned16 _pm_saved_state;
};

template<typename IO>
class Irq_chip_i8259_gen : public Irq_chip_i8259<IO>
{
public:
  using typename Irq_chip_i8259<IO>::Io_address;
  using Irq_chip_i8259<IO>::Nr_pins;

  /**
   * Construct a cascade of i8259 chips.
   *
   * \param master  I/O address of the master chip.
   * \param slave   I/O address of the slave chip.
   */
  Irq_chip_i8259_gen(Io_address master, Io_address slave)
  : Irq_chip_i8259<IO>(master, slave)
  {
    for (auto &pin: _pins)
      pin = nullptr;

    _reserved.clear_all();
  }

  /**
   * Get the IRQ attached to a pin.
   *
   * \note This method does not take the #_lock spinlock and does not check
   *       the #_reserved bitmap. This is possible thanks to the fact that
   *       a non-null pointer can be stored in #_pins only if the respective
   *       bit field in #_reserved is not set (which is checked with the
   *       spinlock taken) and the store is atomic.
   *
   * \param pin  Pin to examine.
   *
   * \return IRQ attached to the pin.
   */
  Irq_base *irq(Mword pin) const override
  {
    if (pin >= Nr_pins)
      return nullptr;

    return _pins[pin];
  }

  /**
   * Attach an IRQ to a pin.
   *
   * \param irq   IRQ to attach.
   * \param pin   Pin to attach the IRQ to.
   * \param init  If true (default value), do a complete initialization of the
   *              IRQ (mode set, masking/unmasking, etc.).
   *
   * \retval true   Attachment successful.
   * \retval false  Attachment failed (pin out of range, reserved or already
   *                attached).
   */
  bool attach(Irq_base *irq, Mword pin, bool init = true) override
  {
    {
      auto guard = lock_guard(_lock);

      if (pin >= Nr_pins)
        return false;

      if (_reserved[pin] || _pins[pin])
        return false;

      _pins[pin] = irq;
    }

    this->bind(irq, pin, !init);
    return true;
  }

  /**
   * Mark a pin as reserved.
   *
   * When a pin is marked as reserved, no IRQ can be attached to it.
   *
   * \param pin  Pin to mark as reserved.
   *
   * \retval true   Reservation successful.
   * \retval false  Reservation failed (pin out of range or already attached).
   */
  bool reserve(Mword pin) override
  {
    auto guard = lock_guard(_lock);

    if (pin >= Nr_pins)
      return false;

    if (_pins[pin] || _reserved[pin])
      return false;

    _reserved.set_bit(pin);
    return true;
  }

  /**
   * Detach an IRQ.
   *
   * \pre The IRQ must be attached to this IRQ chip.
   *
   * \param irq  IRQ to detach.
   */
  void detach(Irq_base *irq) override
  {
    auto guard = lock_guard(_lock);

    Mword pin = irq->pin();

    this->_mask(pin);
    Irq_chip_icu::detach(irq);
    _pins[pin] = nullptr;
  }

private:
  using Irq_chip_i8259<IO>::_lock;

  Irq_base::Ptr _pins[Nr_pins];
  Bitmap<Nr_pins> _reserved;
};

// --------------------------------------------------------
IMPLEMENTATION [i8259]:

/**
 * Create a i8259 chip, does not do any hardware access.
 * \note Hardware initalization is done in init().
 */
PUBLIC template<typename IO>
Irq_chip_i8259<IO>::Irq_chip_i8259(Irq_chip_i8259::Io_address master,
                                   Irq_chip_i8259::Io_address slave)
: _master(master), _slave(slave)
{}

PUBLIC template<typename IO> inline
Unsigned16
Irq_chip_i8259<IO>::disable_all_save()
{
  Unsigned16 s =   Unsigned16{read_ocw(_master)}
                 | Unsigned16{read_ocw(_slave)} << 8;
  write_ocw(0xff, _master);
  write_ocw(0xff, _slave);
  return s;
}

PUBLIC template<typename IO> inline
void
Irq_chip_i8259<IO>::restore_all(Unsigned16 s)
{
  write_ocw(s & 0x0ff, _master);
  write_ocw((s >> 8) & 0x0ff, _slave);
}

/**
 * Initialize the i8259 hardware.
 * \pre The IO access must be enabled in the constructor if needed,
 *      for example when using memory-mapped registers.
 */
PUBLIC template<typename IO>
void
Irq_chip_i8259<IO>::init(Unsigned8 vect_base,
                         bool use_sfn = false,
                         bool high_prio_ir8 = false)
{
  auto guard = lock_guard(_lock);

  _sfn = use_sfn;

  // Disable all IRQs.
  write_ocw(0xff, _master);
  write_ocw(0xff, _slave);

  write_icw(PICM_ICW1, _master);
  iodelay();

  write_ocw(vect_base, _master);
  iodelay();

  // Cascade at IR2.
  write_ocw((1U << 2), _master);
  iodelay();

  Unsigned8 icw4 = PICM_ICW4;
  if (use_sfn)
    icw4 |= SNF_MODE_ENA;
  write_ocw(icw4, _master);
  iodelay();

  write_icw(PICS_ICW1, _slave);
  iodelay();

  write_ocw(vect_base + 8, _slave);
  iodelay();

  write_ocw(PICS_ICW3, _slave);
  iodelay();

  write_ocw(icw4, _slave);
  iodelay();

  if (use_sfn && high_prio_ir8)
    {
      // Setting specific rotation (specific priority),
      // see Intel 8259A reference manual.
      // IRQ 1 on master has the lowest priority
      // => IRQ 2 (cascade) = IRQs 8..15 have the highest priority
      write_icw(SET_PRIORITY | 1, _master);
      iodelay();

      // IRQ 7 on slave has the lowest priority
      // => IRQ 0 on slave (= IRQ 8) has the highest priority
      write_icw(SET_PRIORITY | 7, _slave);
      iodelay();
    }

  // Set initial masks: unmask IR2 on master, mask everything on slave.
  write_ocw(0xfb, _master);
  iodelay();
  write_ocw(0xff, _slave);
  iodelay();

  // Acknowledge any bogus interrupts by setting the End Of Interrupt bit.
  write_icw(NON_SPEC_EOI, _master);
  iodelay();
  write_icw(NON_SPEC_EOI, _slave);
  iodelay();
}

PROTECTED template<typename IO> inline
void
Irq_chip_i8259<IO>::_mask(Mword pin)
{
  if (pin < 8)
    write_ocw(read_ocw(_master) | (1U << pin), _master);
  else
    write_ocw(read_ocw(_slave) | (1U << (pin - 8)), _slave);
}

PUBLIC template<typename IO>
void
Irq_chip_i8259<IO>::mask(Mword pin) override
{
  auto guard = lock_guard(_lock);
  _mask(pin);
}

PUBLIC template<typename IO>
void
Irq_chip_i8259<IO>::unmask(Mword pin) override
{
  auto guard = lock_guard(_lock);

  if (pin < 8)
    write_ocw(read_ocw(_master) & ~(1U << pin), _master);
  else
    write_ocw(read_ocw(_slave) & ~(1U << (pin - 8)), _slave);
}

PRIVATE template<typename IO> inline
void
Irq_chip_i8259<IO>::_ack(Mword pin)
{
  if (pin >= 8)
    {
      write_icw(NON_SPEC_EOI, _slave);
      if (_sfn)
        {
          write_icw(OCW_TEMPLATE | READ_NEXT_RD | READ_IS_ONRD, _slave);
          if (read_icw(_slave))
            return; // Still active IRQs at the slave, don't EOI master
        }
    }

  write_icw(NON_SPEC_EOI, _master);
}

PUBLIC template<typename IO>
void
Irq_chip_i8259<IO>::ack(Mword pin) override
{
  auto guard = lock_guard(_lock);
  _ack(pin);
}

PUBLIC template<typename IO>
void
Irq_chip_i8259<IO>::mask_and_ack(Mword pin) override
{
  auto guard = lock_guard(_lock);
  _mask(pin);
  _ack(pin);
}

// -----------------------------------------------------------------
IMPLEMENTATION [i8259 && debug]:

PUBLIC template<typename IO>
char const *
Irq_chip_i8259<IO>::chip_type() const override
{ return "i8259"; }
