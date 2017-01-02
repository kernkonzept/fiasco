INTERFACE [pf_sa1100]:

#include "types.h"

EXTENSION class Uart
{
public:

  bool startup(Address address, unsigned irq);

  enum {
    PAR_NONE = 0x00,
    PAR_EVEN = 0x03,
    PAR_ODD  = 0x01,
    DAT_5    = (unsigned)-1,
    DAT_6    = (unsigned)-1,
    DAT_7    = 0x00,
    DAT_8    = 0x08,
    STOP_1   = 0x00,
    STOP_2   = 0x04,

    MODE_8N1 = PAR_NONE | DAT_8 | STOP_1,
    MODE_7E1 = PAR_EVEN | DAT_7 | STOP_1,

  // these two values are to leave either mode
  // or baud rate unchanged on a call to change_mode
    MODE_NC  = 0x1000000,
    BAUD_NC  = 0x1000000,

  };

private:
  Address address;
  unsigned _irq;
  enum {
    UTCR0_PE  = 0x01,
    UTCR0_OES = 0x02,
    UTCR0_SBS = 0x04,
    UTCR0_DSS = 0x08,
    UTCR0_SCE = 0x10,
    UTCR0_RCE = 0x20,
    UTCR0_TCE = 0x40,

    UTCR3_RXE = 0x01,
    UTCR3_TXE = 0x02,
    UTCR3_BRK = 0x04,
    UTCR3_RIE = 0x08,
    UTCR3_TIE = 0x10,
    UTCR3_LBM = 0x20,


    UTSR0_TFS = 0x01,
    UTSR0_RFS = 0x02,
    UTSR0_RID = 0x04,
    UTSR0_RBB = 0x08,
    UTSR0_REB = 0x10,
    UTSR0_EIF = 0x20,

    UTSR1_TBY = 0x01,
    UTSR1_RNE = 0x02,
    UTSR1_TNF = 0x04,
    UTSR1_PRE = 0x08,
    UTSR1_FRE = 0x10,
    UTSR1_ROR = 0x20,

    UARTCLK = 3686400,
  };

};

//---------------------------------------------------------------------------
IMPLEMENTATION [pf_sa1100]:

#include <cassert>
#include "processor.h"

PRIVATE INLINE static Address Uart::_utcr0(Address a)
{ return a; }

PRIVATE INLINE static Address Uart::_utcr1(Address a)
{ return (a+0x04); }

PRIVATE INLINE static Address Uart::_utcr2(Address a)
{ return (a+0x08); }

PRIVATE INLINE static Address Uart::_utcr3(Address a)
{ return (a+0x0c); }

PRIVATE INLINE static Address Uart::_utcr4(Address a)
{ return (a+0x10); }

PRIVATE INLINE static Address Uart::_utdr(Address a)
{ return (a+0x14); }

PRIVATE INLINE static Address Uart::_utsr0(Address a)
{ return (a+0x1c); }

PRIVATE INLINE static Address Uart::_utsr1(Address a)
{ return (a+0x20); }


PRIVATE INLINE NEEDS[Uart::_utcr0]
unsigned Uart::utcr0()  const
{ return *((volatile unsigned*)(_utcr0(address))); }

PRIVATE INLINE NEEDS[Uart::_utcr1]
unsigned Uart::utcr1()  const
{ return *((volatile unsigned*)(_utcr1(address))); }

PRIVATE INLINE NEEDS[Uart::_utcr2]
unsigned Uart::utcr2()  const
{ return *((volatile unsigned*)(_utcr2(address))); }

PRIVATE INLINE NEEDS[Uart::_utcr3]
unsigned Uart::utcr3()  const
{ return *((volatile unsigned*)(_utcr3(address))); }

PRIVATE INLINE NEEDS[Uart::_utcr4]
unsigned Uart::utcr4()  const
{ return *((volatile unsigned*)(_utcr4(address))); }

PRIVATE INLINE NEEDS[Uart::_utdr]
unsigned Uart::utdr()  const
{ return *((volatile unsigned*)(_utdr(address))); }

PRIVATE INLINE NEEDS[Uart::_utsr0]
unsigned Uart::utsr0()  const
{ return *((volatile unsigned*)(_utsr0(address))); }

PRIVATE INLINE NEEDS[Uart::_utsr1]
unsigned Uart::utsr1()  const
{ return *((volatile unsigned*)(_utsr1(address))); }


PRIVATE INLINE NEEDS[Uart::_utcr0]
void Uart::utcr0(unsigned v)
{ *((volatile unsigned*)(_utcr0(address)))= v; }

PRIVATE INLINE NEEDS[Uart::_utcr1]
void Uart::utcr1(unsigned v)
{ *((volatile unsigned*)(_utcr1(address)))= v; }

PRIVATE INLINE NEEDS[Uart::_utcr2]
void Uart::utcr2(unsigned v)
{ *((volatile unsigned*)(_utcr2(address)))= v; }

PRIVATE INLINE NEEDS[Uart::_utcr3]
void Uart::utcr3(unsigned v)
{ *((volatile unsigned*)(_utcr3(address)))= v; }

PRIVATE INLINE NEEDS[Uart::_utcr4]
void Uart::utcr4(unsigned v)
{ *((volatile unsigned*)(_utcr4(address)))= v; }

PRIVATE INLINE NEEDS[Uart::_utdr]
void Uart::utdr(unsigned v)
{ *((volatile unsigned*)(_utdr(address)))= v; }

PRIVATE INLINE NEEDS[Uart::_utsr0]
void Uart::utsr0(unsigned v)
{ *((volatile unsigned*)(_utsr0(address)))= v; }

PRIVATE INLINE NEEDS[Uart::_utsr1]
void Uart::utsr1(unsigned v)
{ *((volatile unsigned*)(_utsr1(address)))= v; }


IMPLEMENT Uart::Uart() : Console(DISABLED)
{
  address = (unsigned)-1;
  _irq = (unsigned)-1;
}

IMPLEMENT Uart::~Uart()
{
  utcr3(0);
}


IMPLEMENT bool Uart::startup(Address _address, unsigned irq)
{
  address =_address;
  _irq = irq;
  utsr0((unsigned)-1); //clear pending status bits
  utcr3(UTCR3_RXE | UTCR3_TXE); //enable transmitter and receiver
  add_state(ENABLED);
  return true;
}

IMPLEMENT void Uart::shutdown()
{
  utcr3(0);
}


IMPLEMENT bool Uart::change_mode(TransferMode m, BaudRate baud)
{
  unsigned old_utcr3, quot;
  Proc::Status st;
  if(baud == (BaudRate)-1)
    return false;
  if(baud != BAUD_NC && (baud>115200 || baud<96))
    return false;
  if(m == (TransferMode)-1)
    return false;

  st = Proc::cli_save();
  old_utcr3 = utcr3();
  utcr3(old_utcr3 & ~(UTCR3_RIE|UTCR3_TIE));
  Proc::sti_restore(st);

  while(utsr1() & UTSR1_TBY);

  /* disable all */
  utcr3(0);

  /* set parity, data size, and stop bits */
  if(m!=MODE_NC)
    utcr0(m & 0x0ff); 

  /* set baud rate */
  if(baud!=BAUD_NC) 
    {
      quot = (UARTCLK / (16*baud)) -1;
      utcr1((quot & 0xf00) >> 8);
      utcr2(quot & 0x0ff);
    }

  utsr0((unsigned)-1);

  utcr3(old_utcr3);
  return true;
}

PUBLIC bool Uart::tx_empty()
{
  return !(utsr1() & UTSR1_TBY);
}


IMPLEMENT
int Uart::write(const char *s, size_t count)
{
  unsigned old_utcr3;
  Proc::Status st;
  st = Proc::cli_save();
  old_utcr3 = utcr3();
  utcr3((old_utcr3 & ~(UTCR3_RIE|UTCR3_TIE)) | UTCR3_TXE);

  /* transmission */
  for(unsigned i =0; i<count; i++)
    {
      while(!(utsr1() & UTSR1_TNF))
        ;
      utdr(s[i]);
      if( s[i]=='\n' )
        {
          while(!(utsr1() & UTSR1_TNF))
            ;
          utdr('\r');
        }
    }

  /* wait till everything is transmitted */
  while(utsr1() & UTSR1_TBY)
    ;

  utcr3(old_utcr3);
  Proc::sti_restore(st);
  return 1;
}

IMPLEMENT
int Uart::getchar(bool blocking)
{
  unsigned old_utcr3, ch;

  old_utcr3 = utcr3();
  utcr3(old_utcr3 & ~(UTCR3_RIE|UTCR3_TIE));
  while(!(utsr1() & UTSR1_RNE))
    if(!blocking)
      return -1;

  ch = utdr();
  utsr0(0x04); // clear RID
  utcr3(old_utcr3);
  return ch;
}


IMPLEMENT
int Uart::char_avail() const
{
  if((utsr1() & UTSR1_RNE))
    {
      return 1;
    }
  else
    return 0;
}


IMPLEMENT inline
int Uart::irq() const
{
  return (int)_irq;
}

IMPLEMENT inline NEEDS[Uart::utcr3]
void Uart::enable_rcv_irq()
{
  utcr3(utcr3() | UTCR3_RIE);
}

IMPLEMENT inline NEEDS[Uart::utcr3]
void Uart::disable_rcv_irq()
{
  utcr3(utcr3() & ~UTCR3_RIE);
}

