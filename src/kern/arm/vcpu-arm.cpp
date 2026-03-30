INTERFACE [arm && cpu_virt && irq_direct_inject]:

EXTENSION class Vcpu_irq_list_item
{
public:
  unsigned char lr_idx = 0;
  bool queued = false;
};
