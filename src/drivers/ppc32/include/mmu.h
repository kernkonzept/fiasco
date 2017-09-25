#ifndef __DRIVERS_PPC32_INCLUDE_MMU_H_
#define __DRIVERS_PPC32_INCLUDE_MMU_H_

/** 
 * This guard enables and disables the data-cache of G2 cores using the HID0
 * register
 */
class Mmu_guard
{
public:
  Mmu_guard() : _interrupt(0), _hid0(0)
  {
    asm volatile ( " mfmsr %[intr]                       \n" //disable interrupts
                   " andc  %[ee], %[intr], %[ee]         \n"
                   " mtmsr %[ee]                         \n"
                   " mfspr %[hid0], 1008                 \n" //read HID0
                   " or    %[dcache], %[hid0], %[dcache] \n"
                   " sync                                \n"
                   " mtspr 1008, %[dcache]               \n" //disable data cache
  //                 " isync \n"
                   : [intr] "=r" (_interrupt),
                     [hid0] "=r" (_hid0)
                   : [ee] "r"(1 << 15),
                     [dcache] "r"(1 << 12)
                   : "memory"
                 );
  }

  ~Mmu_guard()
  {
    asm volatile(" sync           \n"
                 " mtspr 1008, %1 \n"
                 " mtmsr %0       \n"
//                 " isync \n"
                 :
                 : "r"(_interrupt),
                   "r"(_hid0));

  }

private:
  unsigned int _interrupt;
  unsigned int _hid0;
};

#endif //__DRIVERS_PPC32_INCLUDE_MMU_H_
