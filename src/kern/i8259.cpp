INTERFACE [i8259]:

#include "atomic.h"
#include "lock_guard.h"
#include "irq_chip.h"
#include "spin_lock.h"
#include "types.h"
#include "pm.h"

template<typename IO>
class Irq_chip_i8259 : public Irq_chip_icu, private Pm_object
{
  friend class Jdb_kern_info_pic_state;
public:
  typedef typename IO::Port_addr Io_address;
  unsigned nr_irqs() const override { return 16; }
  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}
  using Pm_object::register_pm;

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

  // power-management hooks
  void pm_on_suspend(Cpu_number) override
  { _pm_saved_state = disable_all_save(); }

  void pm_on_resume(Cpu_number) override
  { restore_all(_pm_saved_state); }

  Io_address _master, _slave;
  Spin_lock<> _lock;
  bool _sfn = false;

  // power-management state
  Unsigned16 _pm_saved_state;
};

template<typename IO>
class Irq_chip_i8259_gen : public Irq_chip_i8259<IO>
{
public:
  typedef typename Irq_chip_i8259<IO>::Io_address Io_address;
  Irq_chip_i8259_gen(Io_address master, Io_address slave)
  : Irq_chip_i8259<IO>(master, slave)
  {
    for (auto &i: _irqs)
      i = 0;
  }

  Irq_base *irq(Mword pin) const override
  {
    if (pin >= 16)
      return 0;

    return _irqs[pin];
  }

  bool alloc(Irq_base *irq, Mword pin) override
  {
    if (pin >= 16)
      return false;

    if (_irqs[pin])
      return false;

    if (!mp_cas(&_irqs[pin], (Irq_base *)0, irq))
      return false;

    this->bind(irq, pin);
    return true;
  }

  bool reserve(Mword pin) override
  {
    if (pin >= 16)
      return false;

    if (_irqs[pin])
      return false;

    _irqs[pin] = (Irq_base*)1;

    return true;
  }

  void unbind(Irq_base *irq) override
  {
    _irqs[irq->pin()] = 0;
    Irq_chip_icu::unbind(irq);
  }

private:
  Irq_base *_irqs[16];
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
  Unsigned16 s =   (Unsigned16)read_ocw(_master)
                 | (Unsigned16)read_ocw(_slave) << 8;
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
  auto g = lock_guard(_lock);
  _sfn = use_sfn;

  // disable all IRQs
  write_ocw(0xff, _master);
  write_ocw(0xff, _slave);

  write_icw(PICM_ICW1, _master); iodelay();
  write_ocw(vect_base, _master); iodelay();
  write_ocw((1U << 2), _master); iodelay(); // cascade at IR2
  Unsigned8 icw4 = PICM_ICW4;
  if (use_sfn)
    icw4 |= SNF_MODE_ENA;
  write_ocw(icw4, _master); iodelay();

  write_icw(PICS_ICW1, _slave); iodelay();
  write_ocw(vect_base + 8, _slave); iodelay();
  write_ocw(PICS_ICW3, _slave); iodelay();
  write_ocw(icw4, _slave); iodelay();

  if (use_sfn && high_prio_ir8)
    {
      // setting specific rotation (specific priority) 
      // -- see Intel 8259A reference manual
      // irq 1 on master hast lowest prio
      // => irq 2 (cascade) = irqs 8..15 have highest prio
      write_icw(SET_PRIORITY | 1, _master); iodelay();
      // irq 7 on slave has lowest prio
      // => irq 0 on slave (= irq 8) has highest prio
      write_icw(SET_PRIORITY | 7, _slave); iodelay();
    }

  // set initial masks
  write_ocw(0xfb, _master); iodelay(); // unmask ir2
  write_ocw(0xff, _slave); iodelay();  // mask everything

  /* Ack any bogus intrs by setting the End Of Interrupt bit. */
  write_icw(NON_SPEC_EOI, _master); iodelay();
  write_icw(NON_SPEC_EOI, _slave); iodelay();
}

PRIVATE template<typename IO> inline
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
  auto g = lock_guard(_lock);
  _mask(pin);
}

PUBLIC template<typename IO>
void
Irq_chip_i8259<IO>::unmask(Mword pin) override
{
  auto g = lock_guard(_lock);
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
            return; // still active IRQs at the slave, don't EOI master
        }
    }
  write_icw(NON_SPEC_EOI, _master);
}

PUBLIC template<typename IO>
void
Irq_chip_i8259<IO>::ack(Mword pin) override
{
  auto g = lock_guard(_lock);
  _ack(pin);
}

PUBLIC template<typename IO>
void
Irq_chip_i8259<IO>::mask_and_ack(Mword pin) override
{
  auto g = lock_guard(_lock);
  _mask(pin);
  _ack(pin);
}

// -----------------------------------------------------------------
IMPLEMENTATION [i8259 && debug]:

PUBLIC template<typename IO>
char const *
Irq_chip_i8259<IO>::chip_type() const override
{ return "i8259"; }

