INTERFACE[ia32,amd64]:

class Rtc
{
private:
  Rtc();
  Rtc(const Rtc&);
};

IMPLEMENTATION[ia32,amd64]:

#include "io.h"
#include "globalconfig.h"

#define RTC_STATUSA	 0x0a	/* status register A */
#define  RTCSA_TUP	 0x80	/* time update, don't look now */
#define  RTCSA_DIVIDER   0x20   /* divider correct for 32768 Hz */
#define  RTCSA_8192      0x03
#define  RTCSA_4096      0x04
#define  RTCSA_2048      0x05
#define  RTCSA_1024      0x06
#define  RTCSA_512       0x07
#define  RTCSA_256       0x08
#define  RTCSA_128       0x09
#define  RTCSA_64        0x0a
#define  RTCSA_32        0x0b

#define RTC_STATUSB	0x0b	/* status register B */
#define	 RTCSB_DST	 0x01	/* Daylight Savings Time enable	*/
#define	 RTCSB_24HR	 0x02	/* 0 = 12 hours, 1 = 24	hours */
#define	 RTCSB_BCD	 0x04	/* 0 = BCD, 1 =	Binary coded time */
#define	 RTCSB_SQWE	 0x08	/* 1 = output sqare wave at SQW	pin */
#define	 RTCSB_UINTR	 0x10	/* 1 = enable update-ended interrupt */
#define	 RTCSB_AINTR	 0x20	/* 1 = enable alarm interrupt */
#define	 RTCSB_PINTR	 0x40	/* 1 = enable periodic clock interrupt */
#define  RTCSB_HALT      0x80	/* stop clock updates */

#define RTC_INTR	0x0c	/* status register C (R) interrupt source */
#define  RTCIR_UPDATE	 0x10	/* update intr */
#define  RTCIR_ALARM	 0x20	/* alarm intr */
#define  RTCIR_PERIOD	 0x40	/* periodic intr */
#define  RTCIR_INT	 0x80	/* interrupt output signal */


static inline
unsigned char
Rtc::reg_read(unsigned char reg)
{
  Io::out8_p(reg, 0x70);
  return Io::in8_p(0x71);
}

static inline
void
Rtc::reg_write(unsigned char reg, unsigned char val)
{
  Io::out8_p(reg,0x70);
  Io::out8_p(val,0x71);
}

// set up timer interrupt (~ 1ms)
PUBLIC static
void
Rtc::init()
{
  while (reg_read(RTC_STATUSA) & RTCSA_TUP) 
    ; // wait till RTC ready

#ifdef CONFIG_SLOW_RTC
  // set divider to 64 Hz
  reg_write(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_64);
#else
  // set divider to 1024 Hz
  reg_write(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_1024);
#endif

  // set up interrupt
  reg_write(RTC_STATUSB, reg_read(RTC_STATUSB) | RTCSB_PINTR | RTCSB_SQWE); 

  // reset
  reg_read(RTC_INTR);
}

PUBLIC static
void
Rtc::done()
{
  // disable all potential interrupt sources
  reg_write(RTC_STATUSB,
	 reg_read(RTC_STATUSB) & ~(RTCSB_PINTR | RTCSB_AINTR | RTCSB_UINTR));

  // reset
  reg_read(RTC_INTR);
}

PUBLIC static
void
Rtc::set_freq_slow()
{
  // set divider to 32 Hz
  reg_write(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_32);
}

PUBLIC static
void
Rtc::set_freq_normal()
{
  // set divider to 1024 Hz
  reg_write(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_1024);
}

// acknowledge RTC interrupt
PUBLIC static inline
void
Rtc::reset()
{
  // reset irq by reading the cmos port
  // do it fast because we are cli'd
  asm volatile ("movb $0xc, %%al\n\t"
                "outb %%al,$0x70\n\t"
                "outb %%al,$0x80\n\t"
		"inb  $0x71,%%al\n\t" : : : "eax");
}

