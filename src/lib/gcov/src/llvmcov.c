/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Manuel Kalettka <manuel.kalettka@kernkonzept.com>
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#include "InstrProfilingInternal.h"
#pragma clang diagnostic pop

#include "llvmcov.h"
#include "output.h"
#include "base64.h"

uint64_t __llvm_profile_get_size_for_buffer(void);
int __llvm_profile_write_buffer(char *Buffer);

unsigned written = 0;
unsigned __llvmcov_dumped = 0;

static uint32_t
lprofCustomWriter(ProfDataWriter *this,
                  ProfDataIOVec *io_vecs,
                  uint32_t n_io_vecs)
{
  (void) this;
  for (unsigned i = 0; i < n_io_vecs; i++)
    {
      if (io_vecs[i].Data)
        {
          written += store_b64(io_vecs[i].Data, io_vecs[i].ElmSize,
                               io_vecs[i].NumElm);
        }
    }
  return 0;
}


static void
finalize_coverage_dump(int to_write, int written)
{
  int left_to_write = to_write - written;
  for (int i = 0; i < left_to_write; ++i)
    {
      char const c[] = {0};
      store(c, 1);
    }
}

void
dump_coverage(void)
{
  int to_write = __llvm_profile_get_size_for_buffer();

  ProfDataWriter custom_writer;
  custom_writer.Write = lprofCustomWriter;
  lprofWriteData(&custom_writer, NULL, 0);

  finalize_coverage_dump(to_write, written);
}

void cov_print(void);

void
cov_print(void)
{
  if (__llvmcov_dumped)
    return;

  output_llvmcov_data();

  __llvmcov_dumped = 1;
}
