INTERFACE [ppc32]:

/**
 * Device specification 
 */
class Of_device
{
public:
  char     type[32];     //device type name
  char     name[32];     //device name
  Address  reg;          //memory-mapped-register base

  union {
    struct {
      unsigned long cpu_freq;
      unsigned long bus_freq;
      unsigned long time_freq;
    } freq;
    unsigned interrupt[3]; //pin, int nr, sense
  };
};
