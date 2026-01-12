/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2022-2025 Kernkonzept GmbH.
 * Author(s): Stephan Gerhold <stephan.gerhold@kernkonzept.com>
 */

IMPLEMENTATION[arm && iommu && pf_qcom]:

struct Qcom_smmu
{
  Address base;
  Address size;
  unsigned mask;
};

// ------------------------------------------------------------------------
IMPLEMENTATION[arm && iommu && (pf_qcom_msm8909 || pf_qcom_msm8916 || pf_qcom_msm8939)]:

static Qcom_smmu const apps_smmu =
{
  .base = 0x01e00000,
  .size = 0x60000,
  .mask = 0x3f,
};

static Qcom_smmu const gpu_smmu =
{
  .base = 0x01f00000,
  .size = 0x20000,
  .mask = 0,
};

static unsigned const gpu_smmu_irqs[] = {
  // Global interrupt
  32 + 43,
  // Context interrupts
  32 + 240, 32 + 241, 32 + 242, 32 + 245,
};

// ------------------------------------------------------------------------
IMPLEMENTATION[arm && iommu && pf_qcom_msm8909]:

static unsigned const apps_smmu_irqs[] = {
  // Global interrupt
  32 + 41,
  // Context interrupts
  32 + 101, 32 + 102, 32 + 103, 32 + 104, 32 + 105, 32 + 106, 32 + 109,
  32 + 110, 32 + 111, 32 + 112, 32 + 113, 32 + 114, 32 + 115, 32 + 116,
  32 + 117, 32 + 118, 32 + 119, 32 + 120, 32 + 121, 32 + 122, 32 + 223,
  32 + 224, 32 + 225, 32 + 228, 32 + 229, 32 + 230, 32 + 231, 32 + 232,
  32 + 233, 32 + 150, 32 + 151, 32 + 152,
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && iommu && pf_qcom_msm8916]:

static unsigned const apps_smmu_irqs[] = {
  // Global interrupt
  32 + 41,
  // Context interrupts (aggregated)
  32 + 70
};

// ------------------------------------------------------------------------
IMPLEMENTATION[arm && iommu && pf_qcom_msm8939]:

static unsigned const apps_smmu_irqs[] = {
  // Global interrupt
  32 + 41,
  // Context interrupts
  32 + 254, 32 + 255, 32 +  53, 32 +  54, 32 +  58, 32 +  60, 32 +  61,
  32 +  76, 32 +  77, 32 +  80, 32 +  94, 32 + 101, 32 + 102, 32 + 103,
  32 + 104, 32 + 105, 32 + 106, 32 + 109, 32 + 110, 32 + 111, 32 + 112,
  32 + 113, 32 + 114, 32 + 115, 32 + 116, 32 + 117, 32 + 118, 32 + 119,
  32 + 120, 32 + 121, 32 + 122, 32 + 127,
};

// ------------------------------------------------------------------------
IMPLEMENTATION[arm && iommu && pf_qcom_sm8150]:

static Qcom_smmu const apps_smmu =
{
  .base = 0x15000000,
  .size = 0x100000,
  .mask = 0,
};

static unsigned const apps_smmu_irqs[] = {
  // Global interrupt
  32 + 65,
  // Context interrupts
  32 +  97, 32 +  98, 32 +  99, 32 + 100, 32 + 101, 32 + 102, 32 + 103,
  32 + 104, 32 + 105, 32 + 106, 32 + 107, 32 + 108, 32 + 109, 32 + 110,
  32 + 111, 32 + 112, 32 + 113, 32 + 114, 32 + 115, 32 + 116, 32 + 117,
  32 + 118, 32 + 181, 32 + 182, 32 + 183, 32 + 184, 32 + 185, 32 + 186,
  32 + 187, 32 + 188, 32 + 189, 32 + 190, 32 + 191, 32 + 192, 32 + 315,
  32 + 316, 32 + 317, 32 + 318, 32 + 319, 32 + 320, 32 + 321, 32 + 322,
  32 + 323, 32 + 324, 32 + 325, 32 + 326, 32 + 327, 32 + 328, 32 + 329,
  32 + 330, 32 + 331, 32 + 332, 32 + 333, 32 + 334, 32 + 335, 32 + 336,
  32 + 337, 32 + 338, 32 + 339, 32 + 340, 32 + 341, 32 + 342, 32 + 343,
  32 + 344, 32 + 345, 32 + 395, 32 + 396, 32 + 397, 32 + 398, 32 + 399,
  32 + 400, 32 + 401, 32 + 402, 32 + 403, 32 + 404, 32 + 405, 32 + 406,
  32 + 407, 32 + 408, 32 + 409,
};

static Qcom_smmu const gpu_smmu =
{
  .base = 0x02ca0000,
  .size = 0x10000,
  .mask = 0,
};

static unsigned const gpu_smmu_irqs[] = {
  // Global interrupt
  32 + 674,
  // Context interrupts
  32 + 681, 32 + 682, 32 + 683, 32 + 684,
  32 + 685, 32 + 686, 32 + 687, 32 + 688,
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && iommu && pf_qcom]:

#include "io.h"
#include "kmem_mmio.h"
#include "warn.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  static_assert(Max_iommus == 1 || Max_iommus == 2,
                "Unexpected number of IOMMUs.");

  _iommus = Iommu_array(new Boot_object<Iommu>[Max_iommus], Max_iommus);

  /*
   * The APPS SMMU on MSM8916 has a special "interrupt aggregation logic"
   * built around the SMMUv2 that routes the context bank interrupts to one of
   * 3 configurable interrupts. Currently, Fiasco supports such shared context
   * interrupts only for SMMUv1.
   *
   * For now we can use a single interrupt for all context banks and pretend
   * that we have a SMMUv1 instead of SMMUv2. This might need to be revisited
   * later in case the SMMU version is used for more than just the interrupt
   * configuration.
   */
  Version version = Version::Smmu_v2;
  if (cxx::size(apps_smmu_irqs) == 2) // Global + single context interrupt?
    version = Version::Smmu_v1;

  void *base = Kmem_mmio::map(apps_smmu.base, apps_smmu.size);
  _iommus[0].setup(version, base, apps_smmu.mask);
  _iommus[0].setup_irqs(apps_smmu_irqs, cxx::size(apps_smmu_irqs), 1);

  /*
   * There are 2 IOMMUs on all supported platforms but the GPU SMMU is not
   * working yet on SM8150. It seems to be powered off by default so additional
   * code needs to be added somewhere (probably in bootstrap) to turn it on.
   */
  if (Max_iommus > 1)
    {
      base = Kmem_mmio::map(gpu_smmu.base, gpu_smmu.size);
      _iommus[1].setup(Version::Smmu_v2, base, gpu_smmu.mask);
      _iommus[1].setup_irqs(gpu_smmu_irqs, cxx::size(gpu_smmu_irqs), 1);
    }

  return true;
}
