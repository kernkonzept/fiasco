INTERFACE [arm && cpu_virt]:

EXTENSION class Vcpu_irq_list_item
{
public:
  unsigned lr = 0;
};
