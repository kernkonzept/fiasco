#include "base64.h"
#include "InstrProfilingInternal.h"
#include "llvmcov.h"
#include "output.h"
#include "zstd.h"

extern char const *__COV_PATH;


unsigned
store_b64(void const *data, unsigned elem_size, unsigned n_elem)
{
  size_t length = elem_size * n_elem;
  store(data, length);
  return length;
}

void
output_llvmcov_data(void)
{
  init_zstd();
  vconprint("@@ llvmcov: PATH '");
  vconprint(__COV_PATH);
  vconprint("' ZSTD '");

  dump_coverage();
  store_string("", ZSTD_e_end); // Flush zstd buffers
  flush_base64_buffers();
  vconprint("'\n");
}
