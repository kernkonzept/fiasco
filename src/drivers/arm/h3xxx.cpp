INTERFACE[ arm-{h3600,h3800} ]:

#include <types.h>

template< unsigned long Egpio_base >
class H3xxx_generic
{
public:
  enum {
    H3800_asic1_gpio_offset         = 0x1e00,
    
    /// R/W 0:don't mask, 1:mask interrupt 
    H3800_asic1_gpio_mask           = 0x60,
    /// R/W 0:input, 1:output              
    H3800_asic1_gpio_direction      = 0x64,
    /// R/W 0:output low, 1:output high    
    H3800_asic1_gpio_out            = 0x68,
    /// R/W 0:level, 1:edge                
    H3800_asic1_gpio_trigger_type   = 0x6c,
    /// R/W 0:falling, 1:rising            
    H3800_asic1_gpio_edge_trigger   = 0x70,
    /// R/W 0:low, 1:high level detect     
    H3800_asic1_gpio_level_trigger  = 0x74,
    /// R/W 0:none, 1:detect               
    H3800_asic1_gpio_level_status   = 0x78,
    /// R/W 0:none, 1:detect               
    H3800_asic1_gpio_edge_status    = 0x7c,
    /// R   See masks below  (default 0)         
    H3800_asic1_gpio_state          = 0x80,
    /// R/W See masks below  (default 0x04)      
    H3800_asic1_gpio_reset          = 0x84,
    /// R/W 0:don't mask, 1:mask trigger in sleep mode  
    H3800_asic1_gpio_sleep_mask     = 0x88,
    /// R/W direction 0:input, 1:ouput in sleep mode    
    H3800_asic1_gpio_sleep_dir      = 0x8c,
    /// R/W level 0:low, 1:high in sleep mode           
    H3800_asic1_gpio_sleep_out      = 0x90,
    /// R   Pin status                                  
    H3800_asic1_gpio_status         = 0x94,
    /// R/W direction 0:input, 1:output in batt_fault   
    H3800_asic1_gpio_batt_fault_dir = 0x98,
    /// R/W level 0:low, 1:high in batt_fault           
    H3800_asic1_gpio_batt_fault_out = 0x9c,

    /// Apply power to the IR Module
    H3800_asic1_ir_on               = 1 <<  0,
    /// Secure Digital power on
    H3800_asic1_sd_pwr_on           = 1 <<  1,
    /// Turn on power to the RS232 chip ?
    H3800_asic1_rs232_on            = 1 <<  2,
    /// Goes to speaker / earphone
    H3800_asic1_pulse_gen           = 1 <<  3,
    /// 
    H3800_asic1_ch_timer            = 1 <<  4,
    /// Enables LCD_5V
    H3800_asic1_lcd_5v_on           = 1 <<  5,
    /// Enables LCD_3V
    H3800_asic1_lcd_on              = 1 <<  6,
    /// Connects to PDWN on LCD controller
    H3800_asic1_lcd_pci             = 1 <<  7,
    /// Drives VGH on the LCD (+9??)
    H3800_asic1_vgh_on              = 1 <<  8,
    /// Drivers VGL on the LCD (-6??)
    H3800_asic1_vgl_on              = 1 <<  9,
    /// Frontlight power on
    H3800_asic1_fl_pwr_on           = 1 << 10,
    /// Bluetooth power on
    H3800_asic1_bt_pwr_on           = 1 << 11,
    /// 
    H3800_asic1_spk_on              = 1 << 12,
    /// 
    H3800_asic1_ear_on              = 1 << 13,
    ///
    H3800_asic1_aud_pwr_on          = 1 << 14,
  };

private:
  template< typename Reg_type >
  static inline void h3800_asic1_gpio( Reg_type val, unsigned long offset )
  {
    *((volatile Reg_type*)(Egpio_base + H3800_asic1_gpio_offset + offset)) = 
      val;
  }
  
  template< typename Reg_type >
  static inline Reg_type h3800_asic1_gpio( unsigned long offset )
  {
    return *((volatile Reg_type*)(Egpio_base + H3800_asic1_gpio_offset 
	                          + offset));
  }

  // define all the asic accessor functions. 
# define ____H3800_asic1_ac( t, x, o ) \
  *((volatile t*)(Egpio_base + H3800_asic1_## x ##_offset + (o)))
# define ____H3800_asic1( t, x, m ) \
  static inline void h3800_asic1_## x ##_## m ( t v ) \
  { ____H3800_asic1_ac(t, x, H3800_asic1_## x ##_## m) = v; } \
  static inline t h3800_asic1_## x ##_## m () \
  { return ____H3800_asic1_ac( t, x, H3800_asic1_## x ##_## m); }

public:
  ____H3800_asic1( Unsigned16, gpio, mask )
  ____H3800_asic1( Unsigned16, gpio, direction )
  ____H3800_asic1( Unsigned16, gpio, out )
  ____H3800_asic1( Unsigned16, gpio, trigger_type )
  ____H3800_asic1( Unsigned16, gpio, edge_trigger )
  ____H3800_asic1( Unsigned16, gpio, level_trigger )
  ____H3800_asic1( Unsigned16, gpio, level_status )
  ____H3800_asic1( Unsigned16, gpio, edge_status )
  ____H3800_asic1( Unsigned8,  gpio, state )
  ____H3800_asic1( Unsigned8,  gpio, reset )
  ____H3800_asic1( Unsigned16, gpio, sleep_mask )
  ____H3800_asic1( Unsigned16, gpio, sleep_dir )
  ____H3800_asic1( Unsigned16, gpio, sleep_out )
  ____H3800_asic1( Unsigned16, gpio, status )
  ____H3800_asic1( Unsigned16, gpio, batt_fault_dir )
  ____H3800_asic1( Unsigned16, gpio, batt_fault_out )
  
# undef ____H3800_asic1
# undef ____H3800_asic1_ac
 
};

IMPLEMENTATION[ arm-{h3600,h3800} ]:

PUBLIC static 
template< unsigned long Egpio_base >
void H3xxx_generic<Egpio_base>::h3800_init()
{
  h3800_asic1_gpio_direction( 0x7fff ); // all outputs
  h3800_asic1_gpio_mask( 0x7fff ); // all outputs
  h3800_asic1_gpio_sleep_mask( 0x7fff );
  h3800_asic1_gpio_sleep_dir( 0x7fff );
  h3800_asic1_gpio_sleep_out( H3800_asic1_ear_on );
  h3800_asic1_gpio_batt_fault_dir( 0x7fff );
  h3800_asic1_gpio_batt_fault_out( H3800_asic1_ear_on );

  h3800_asic1_gpio_out(  H3800_asic1_ear_on | H3800_asic1_rs232_on );
}

