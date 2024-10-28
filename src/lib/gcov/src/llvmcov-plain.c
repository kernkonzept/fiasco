#include "base64.h"
#include "llvmcov.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#include "InstrProfilingInternal.h"
#pragma clang diagnostic pop
#include "output.h"

extern char const *__COV_PATH;

unsigned
store_b64(const void *data, unsigned elem_size, unsigned n_elem)
{
  size_t length = elem_size * n_elem;
  store(data, length);
  return length;
}


void
output_llvmcov_data(void)
{
  cov_output("@@ llvmcov: PATH '");
  cov_output(__COV_PATH);
  cov_output("' ZDATA '");
  dump_coverage();
  flush_base64_buffers();
  cov_output("'\n");
}
