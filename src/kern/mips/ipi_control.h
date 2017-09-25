
#pragma once

class Ipi;
class Irq_base;

class Ipi_control
{
public:
  virtual void send_ipi(Cpu_number to_cpu, Ipi *target) = 0;
  virtual void ack_ipi(Cpu_number cpu) = 0;
  virtual void init_ipis(Cpu_number cpu, Irq_base *irq) = 0;
};


