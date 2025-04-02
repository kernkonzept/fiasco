INTERFACE:

#include "irq_mgr.h"

template<typename CHIP, typename MSI_CHIP>
class Irq_mgr_msi : public Irq_mgr
{
private:
  CHIP *_chip;
};

//-------------------------------------------------------------------
INTERFACE[arm_gic_msi]:

EXTENSION class Irq_mgr_msi
{
private:
  MSI_CHIP *_msi_chip;
};

//-------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC template<typename CHIP, typename MSI_CHIP>
unsigned
Irq_mgr_msi<CHIP, MSI_CHIP>::nr_gsis() const override
{ return _chip->CHIP::nr_pins(); }

//-------------------------------------------------------------------
IMPLEMENTATION[arm_gic_msi]:

PUBLIC template<typename CHIP, typename MSI_CHIP> inline
Irq_mgr_msi<CHIP, MSI_CHIP>::Irq_mgr_msi(CHIP *chip, MSI_CHIP *msi_chip)
: _chip(chip), _msi_chip(msi_chip)
{}

PUBLIC template<typename CHIP, typename MSI_CHIP>
Irq_mgr::Chip_pin
Irq_mgr_msi<CHIP, MSI_CHIP>::chip_pin(Mword gsi) const override
{
  if (gsi & Msi_bit)
    return _msi_chip ? Chip_pin(_msi_chip, gsi & ~Msi_bit) : Chip_pin();

  return Chip_pin(_chip, gsi);
}

PUBLIC template<typename CHIP, typename MSI_CHIP>
unsigned
Irq_mgr_msi<CHIP, MSI_CHIP>::nr_msis() const override
{ return _msi_chip ? _msi_chip->MSI_CHIP::nr_pins() : 0; }

PUBLIC template<typename CHIP, typename MSI_CHIP>
int
Irq_mgr_msi<CHIP, MSI_CHIP>::msi_info(Mword msi, Unsigned64 src,
                                      Msi_info *info) const override
{
  if ((msi & Msi_bit) && _msi_chip)
    return _msi_chip->MSI_CHIP::msi_info(msi & ~Msi_bit, src, info);

  return -L4_err::ERange;
}

//-------------------------------------------------------------------
IMPLEMENTATION[!arm_gic_msi]:

PUBLIC template<typename CHIP, typename MSI_CHIP> inline
Irq_mgr_msi<CHIP, MSI_CHIP>::Irq_mgr_msi(CHIP *chip, MSI_CHIP *)
: _chip(chip)
{}

PUBLIC template<typename CHIP, typename MSI_CHIP>
Irq_mgr::Chip_pin
Irq_mgr_msi<CHIP, MSI_CHIP>::chip_pin(Mword gsi) const override
{ return Chip_pin(_chip, gsi); }

PUBLIC template<typename CHIP, typename MSI_CHIP>
unsigned
Irq_mgr_msi<CHIP, MSI_CHIP>::nr_msis() const override
{ return 0; }

PUBLIC template<typename CHIP, typename MSI_CHIP>
int
Irq_mgr_msi<CHIP, MSI_CHIP>::msi_info(Mword, Unsigned64, Msi_info *) const override
{ return -L4_err::ENosys; }
