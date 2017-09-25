INTERFACE [arm && s3c2410]:

#include "initcalls.h"
#include "kmem.h"

EXTENSION class Pic
{
public:
};

// ---------------------------------------------------------------------
IMPLEMENTATION [arm && s3c2410]:

#include "assert.h"
#include "config.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"

#include <cstdio>

class S3c_chip : public Irq_chip_gen, Mmio_register_block
{
  enum
  {
    SRCPND    = 0x00,
    INTMODE   = 0x04,
    INTMSK    = 0x08,
    PRIORITY  = 0x0c,
    INTPND    = 0x10,
    INTOFFSET = 0x14,
    SUBSRCPND = 0x18,
    INTSUBMSK = 0x1c,
  };

  enum
  {
    MAIN_0        = 0,
    MAIN_EINT4_7  = 4,
    MAIN_EINT8_23 = 5,
    MAIN_UART2    = 15,
    MAIN_LCD      = 16,
    MAIN_UART1    = 23,
    MAIN_UART0    = 28,
    MAIN_ADC      = 31,
    MAIN_31       = 31,

    SUB_RXD0 = 0,
    SUB_TXD0 = 1,
    SUB_ERR0 = 2,
    SUB_RXD1 = 3,
    SUB_TXD1 = 4,
    SUB_ERR1 = 5,
    SUB_RXD2 = 6,  // 53
    SUB_TXD2 = 7,  // 54
    SUB_ERR2 = 8,  // 52
    SUB_TC   = 9,  // 64
    SUB_ADC  = 10, // 63
  };

  enum // Interrupts
  {
    // EINT4_7
    EINT4  = 32,
    EINT7  = 35,
    // EINT8_23
    EINT8  = 36,
    EINT23 = 51,
    // UART2
    INT_UART2_ERR = 52,
    INT_UART2_RXD = 53,
    INT_UART2_TXD = 54,
    // LCD
    INT_LCD_FRSYN = 55,
    INT_LCD_FICNT = 56,
    // UART1
    INT_UART1_ERR = 57,
    INT_UART1_RXD = 58,
    INT_UART1_TXD = 59,
    // UART0
    INT_UART0_ERR = 60,
    INT_UART0_RXD = 61,
    INT_UART0_TXD = 62,
    // ADC
    INT_ADC = 63,
    INT_TC  = 64,
  };

public:
  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
};

PUBLIC
S3c_chip::S3c_chip()
: Irq_chip_gen(32),
  Mmio_register_block(Kmem::mmio_remap(Mem_layout::Pic_phys_base))
{

  write<Mword>(0xffffffff, INTMSK); // all masked
  write<Mword>(0x7fe, INTSUBMSK);   // all masked
  write<Mword>(0, INTMODE);         // all IRQs, no FIQs
  modify<Mword>(0, 0, SRCPND); // clear source pending
  modify<Mword>(0, 0, SUBSRCPND); // clear sub src pnd
  modify<Mword>(0, 0, INTPND); // clear pending interrupt
}


PUBLIC
void
S3c_chip::mask(Mword irq)
{
  Mword mainirq;

  switch (irq)
    {
      case INT_TC:        modify<Mword>(1 << SUB_TC,   0, INTSUBMSK); mainirq = MAIN_ADC;   break;
      case INT_ADC:       modify<Mword>(1 << SUB_ADC,  0, INTSUBMSK); mainirq = MAIN_ADC;   break;
      case INT_UART0_RXD: modify<Mword>(1 << SUB_RXD0, 0, INTSUBMSK); mainirq = MAIN_UART0; break;
      case INT_UART0_TXD: modify<Mword>(1 << SUB_TXD0, 0, INTSUBMSK); mainirq = MAIN_UART0; break;
      case INT_UART0_ERR: modify<Mword>(1 << SUB_ERR0, 0, INTSUBMSK); mainirq = MAIN_UART0; break;
      case INT_UART1_RXD: modify<Mword>(1 << SUB_RXD1, 0, INTSUBMSK); mainirq = MAIN_UART1; break;
      case INT_UART1_TXD: modify<Mword>(1 << SUB_TXD1, 0, INTSUBMSK); mainirq = MAIN_UART1; break;
      case INT_UART1_ERR: modify<Mword>(1 << SUB_ERR1, 0, INTSUBMSK); mainirq = MAIN_UART1; break;
      case INT_UART2_RXD: modify<Mword>(1 << SUB_RXD2, 0, INTSUBMSK); mainirq = MAIN_UART2; break;
      case INT_UART2_TXD: modify<Mword>(1 << SUB_TXD2, 0, INTSUBMSK); mainirq = MAIN_UART2; break;
      case INT_UART2_ERR: modify<Mword>(1 << SUB_ERR2, 0, INTSUBMSK); mainirq = MAIN_UART2; break;
      default:
         if (irq > 31)
           return; // XXX: need to add other cases
         mainirq = irq;
    };

  modify<Mword>(1 << mainirq, 0, INTMSK);
}

PUBLIC
void
S3c_chip::unmask(Mword irq)
{
  int mainirq;

  switch (irq)
    {
      case INT_TC:        modify<Mword>(0, 1 << SUB_TC,   INTSUBMSK); mainirq = MAIN_ADC;   break;
      case INT_ADC:       modify<Mword>(0, 1 << SUB_ADC,  INTSUBMSK); mainirq = MAIN_ADC;   break;
      case INT_UART0_RXD: modify<Mword>(0, 1 << SUB_RXD0, INTSUBMSK); mainirq = MAIN_UART0; break;
      case INT_UART0_TXD: modify<Mword>(0, 1 << SUB_TXD0, INTSUBMSK); mainirq = MAIN_UART0; break;
      case INT_UART0_ERR: modify<Mword>(0, 1 << SUB_ERR0, INTSUBMSK); mainirq = MAIN_UART0; break;
      case INT_UART1_RXD: modify<Mword>(0, 1 << SUB_RXD1, INTSUBMSK); mainirq = MAIN_UART1; break;
      case INT_UART1_TXD: modify<Mword>(0, 1 << SUB_TXD1, INTSUBMSK); mainirq = MAIN_UART1; break;
      case INT_UART1_ERR: modify<Mword>(0, 1 << SUB_ERR1, INTSUBMSK); mainirq = MAIN_UART1; break;
      case INT_UART2_RXD: modify<Mword>(0, 1 << SUB_RXD2, INTSUBMSK); mainirq = MAIN_UART2; break;
      case INT_UART2_TXD: modify<Mword>(0, 1 << SUB_TXD2, INTSUBMSK); mainirq = MAIN_UART2; break;
      case INT_UART2_ERR: modify<Mword>(0, 1 << SUB_ERR2, INTSUBMSK); mainirq = MAIN_UART2; break;
      default:
         if (irq > 31)
           return; // XXX: need to add other cases
         mainirq = irq;
    };

  modify<Mword>(0, 1 << mainirq, INTMSK);
}

PUBLIC
void
S3c_chip::ack(Mword irq)
{
  int mainirq;

  switch (irq)
    {
      case INT_TC:        write<Mword>(1 << SUB_TC,   SUBSRCPND); mainirq = MAIN_ADC;   break;
      case INT_ADC:       write<Mword>(1 << SUB_ADC,  SUBSRCPND); mainirq = MAIN_ADC;   break;
      case INT_UART0_RXD: write<Mword>(1 << SUB_RXD0, SUBSRCPND); mainirq = MAIN_UART0; break;
      case INT_UART0_TXD: write<Mword>(1 << SUB_TXD0, SUBSRCPND); mainirq = MAIN_UART0; break;
      case INT_UART0_ERR: write<Mword>(1 << SUB_ERR0, SUBSRCPND); mainirq = MAIN_UART0; break;
      case INT_UART1_RXD: write<Mword>(1 << SUB_RXD1, SUBSRCPND); mainirq = MAIN_UART1; break;
      case INT_UART1_TXD: write<Mword>(1 << SUB_TXD1, SUBSRCPND); mainirq = MAIN_UART1; break;
      case INT_UART1_ERR: write<Mword>(1 << SUB_ERR1, SUBSRCPND); mainirq = MAIN_UART1; break;
      case INT_UART2_RXD: write<Mword>(1 << SUB_RXD2, SUBSRCPND); mainirq = MAIN_UART2; break;
      case INT_UART2_TXD: write<Mword>(1 << SUB_TXD2, SUBSRCPND); mainirq = MAIN_UART2; break;
      case INT_UART2_ERR: write<Mword>(1 << SUB_ERR2, SUBSRCPND); mainirq = MAIN_UART2; break;
      default:
         if (irq > 31)
           return; // XXX: need to add other cases
        mainirq = irq;
    };

  write<Mword>(1 << mainirq, SRCPND); // only 1s are set to 0
  write<Mword>(1 << mainirq, INTPND); // clear pending interrupt
}

PUBLIC
void
S3c_chip::mask_and_ack(Mword irq)
{
  assert(cpu_lock.test());
  mask(irq);
  ack(irq);
}


static Static_object<Irq_mgr_single_chip<S3c_chip> > mgr;


PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

PUBLIC inline
Unsigned32 S3c_chip::pending()
{
  int mainirq = read<Mword>(INTOFFSET);

  switch (mainirq)
    {
    case MAIN_ADC:
	{
	  int subirq = read<Mword>(SUBSRCPND);
	  if ((1 << SUB_ADC) & subirq)
	    return INT_ADC;
	  else if ((1 << SUB_TC) & subirq)
	    return INT_TC;
	}
      break;
    // more: tbd
    default:
      return mainirq;
    }
  return 32;
}

extern "C"
void irq_handler()
{
  Unsigned32 i = mgr->c.pending();
  if (i != 32)
    mgr->c.handle_irq<S3c_chip>(i, 0);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug && s3c2410]:

PUBLIC
char const *
S3c_chip::chip_type() const
{ return "HW S3C2410 IRQ"; }

