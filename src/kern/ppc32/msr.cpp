INTERFACE:

#include "types.h"

class Msr
{
public:
  enum Msr_bits {
    Msr_ri  = 1UL << 1,  //(30)Recoverable exception 
    Msr_dr  = 1UL << 4,  //(27)Data-address translation 
    Msr_ir  = 1UL << 5,  //(26)Instruction-address translation
    Msr_ce  = 1UL << 4,  //(24)Critical interrupt enable
    Msr_ip  = 1UL << 6,  //(25)Interrupt prefix
    Msr_fe1 = 1UL << 8,  //(23)Floating-point exception mode 1
    Msr_be  = 1UL << 9,  //(22)branch trace enable
    Msr_se  = 1UL << 10, //(21)Signal-step trace enable
    Msr_fe0 = 1UL << 11, //(20)Floating-point exception mode 0
    Msr_me  = 1UL << 12, //(19)Machine check enable
    Msr_fp  = 1UL << 13, //(18)Floating-point available
    Msr_pr  = 1UL << 14, //(17)Privilege-level user
    Msr_ee  = 1UL << 15, //(16)External interrupt enable
    Msr_pow = 1UL << 18, //(13)Power management enable 
    //Msr_fp is just enabled to not raise FPU load/store exceptions in user land
    Msr_user = Msr_dr | Msr_ir | Msr_ee | Msr_me | Msr_pr | Msr_fp,
    Msr_kernel =  Msr_me | Msr_ri
  };

private:
  static void check_msr_sync(Mword bits);
};

//------------------------------------------------------------------------------
IMPLEMENTATION:


IMPLEMENT inline
void
Msr::check_msr_sync(Mword bits)
{
  //for these context synchronization is mandatory
  static Mword mask = ( Msr_pr | Msr_me | Msr_dr  | Msr_fp | Msr_se |
                        Msr_be | Msr_ir | Msr_fe0 | Msr_fe1 );
  if(bits & mask)
    asm volatile("isync \n" : : :);
}

PUBLIC static inline
void
Msr::set_msr_bit(Mword bits)
{
  asm volatile ( " mfmsr %%r5                \n"
		 " or    %%r5, %%r5, %[bits] \n"
		 " mtmsr %%r5                \n"
		 :
		 : [bits]"r" (bits)
		 : "r5", "memory"
		 );
  check_msr_sync(bits);
}

PUBLIC static inline
void
Msr::clear_msr_bit(Mword bits)
{
  asm volatile ( " mfmsr %%r5                \n"
		 " andc  %%r5, %%r5, %[bits] \n"
		 " mtmsr %%r5                \n"
		 : 
		 : [bits]"r" (bits)
		 : "r5", "memory"
		 );
  check_msr_sync(bits);
}

PUBLIC static inline 
Mword
Msr::read_msr()
{
  Mword ret;
  asm volatile ( " mfmsr %0  \n"
		 : "=r" (ret)
		 );
  return ret;
}

