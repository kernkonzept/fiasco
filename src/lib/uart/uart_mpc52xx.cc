#include "uart_mpc52xx.h"
#include "of1275.h"
#include "mmu.h"

namespace L4
{

  enum
  {
    MR1_RXIRQ  = 0x040,

    SR_TXRDY   = 0x400,
    SR_TXEMPTY = 0x800,
    SR_RXRDY   = 0x100,

    CMD_TX_ENABLE  = 0x004,
    CMD_TX_RESET   = 0x030,
    CMD_RX_ENABLE  = 0x001,
    CMD_RX_DISABLE = 0x002,
    CMD_RX_RESET   = 0x020,
    CMD_RESET_MODE = 0x010,
    CMD_RESET_ERR  = 0x040,
    CMD_RESET_BREAK= 0x050,

    IMR_RXRDY  = 0x200
  };

  template < typename T >
  void Uart_mpc52xx::wr_dirty(unsigned long addr, T val) const
  {
    asm volatile("sync \n" :::);
    *(volatile T*)addr = val;
  }

  template < typename T >
  void Uart_mpc52xx::wr(unsigned long addr, T val) const
  {
    Mmu_guard dcache;
    wr_dirty<T>(addr, val);
  }

  template < typename T >
  T Uart_mpc52xx::rd_dirty(unsigned long addr) const
  {
    volatile T ret;
    asm volatile("sync \n" :::);
    ret = *(volatile T*)addr;
    asm volatile("isync\n" :::);
    return ret;
   }

  template < typename T >
  T Uart_mpc52xx::rd(unsigned long addr) const
  {
    Mmu_guard dcache;
    return rd_dirty<T>(addr);
  }

  bool Uart_mpc52xx::startup(const L4::Io_register_block *regs)
  {
    _regs = regs;
    //wait for Of uart driver to finish transmission
    Mmu_guard dcache;
    while (!(rd_dirty<u16>(status()) & SR_TXEMPTY))
      ;
    //read MR1
    wr_dirty<u8>(command(), CMD_RESET_MODE);
    u8 mode_val = rd_dirty<u8>(mode());
    //mode points to MR2
    wr_dirty<u8>(command(), CMD_RESET_MODE);
    //mode points to MR1

    //clear RxIRQ in MR1 register (RxRDY is now source of interrupt)
    mode_val &= ~MR1_RXIRQ;
    wr_dirty<u8>(mode(), mode_val);

    //disable interrupts
    wr_dirty<u16>(imr(), 0);

    wr_dirty<u16>(status(), 0xdd00);
    wr_dirty<u32>(sicr(), 0);

    //enable transmit, receive
    wr_dirty<u8>(command(), CMD_TX_RESET);
    wr_dirty<u8>(command(), CMD_TX_ENABLE);
    wr_dirty<u8>(command(), CMD_RX_RESET);
    wr_dirty<u8>(command(), CMD_RX_ENABLE);

    write("MPC52xx serial port: replaced OF\n", 33);
    return true;
  }

  void Uart_mpc52xx::out_char(char c) const
  {
    Mmu_guard dcache;
    mpc52xx_out_char(c);
  }

  int Uart_mpc52xx::write(char const *s, unsigned long count) const
  {
    Mmu_guard dcache;
    unsigned long c = count;
    while (c--)
	mpc52xx_out_char(*s++);
    return count;
  }


  inline int Uart_mpc52xx::char_avail() const
  {
    return rd<u16>(status()) & SR_RXRDY;
  }

  int Uart_mpc52xx::get_char(bool blocking) const
  {
    while(!char_avail())
      if(!blocking) return -1;
    int c = (int)rd<u8>(buffer());

    //reset error conditions
    wr<u8>(command(), CMD_RESET_ERR);
    return c;
  }

  bool Uart_mpc52xx::enable_rx_irq(bool enable)
  {
    u16 mask = 0;
    if(enable)
      mask |= IMR_RXRDY;

    wr<u16>(imr(), mask);

    return true;
  }

  //PRIVATE
  inline void Uart_mpc52xx::mpc52xx_out_char(char c) const
  {
    while(!(rd_dirty<u16>(status()) & SR_TXRDY))
      ;
    wr_dirty<char>(buffer(), c);
  }

  //TODO: implement
  void Uart_mpc52xx::shutdown() {}
  bool Uart_mpc52xx::change_mode(Transfer_mode, Baud_rate) { return true; }
};
