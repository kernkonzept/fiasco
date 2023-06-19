#include "base64.h"
#include "output.h"
#include "zstd.h"

unsigned outOffset;
static ZSTD_outBuffer output;
static ZSTD_CCtx *ctx;

void
init_zstd(void)
{
  // This is initialized here to work around an issue with initialization
  output.dst  = buffer;
  output.size = BUF_SIZE;
  output.pos  = 0;

  outOffset = 0;

  if (init_storage_backend() != 0)
    {
      vconprint("COV: ERROR: Failed to initialize ZSTD storage\n");
      return;
    }

  ctx = ZSTD_initStaticCCtx((void *)zstd_workspace, zstd_workspace_size);
  if (ctx == NULL)
    {
      vconprint("COV: ERROR: Failed to init ZSTD Context\n");
      return;
    }


  // TODO: Add checks?
  ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, CompressionLevel);
  ZSTD_CCtx_setParameter(ctx, ZSTD_c_checksumFlag, 1);

  set_compression_parameters(ctx);
}

void
flush_base64_buffers(void)
{
  int nout = print_base64(buffer, output.pos, outbuf);
  vconprintn(outbuf, nout);
}

void
store_string(char const *s, ZSTD_EndDirective mode)
{
  unsigned len = __builtin_strlen(s);
  len += (len == 0) ? 0 : 1; // also store zero unless it is the empty string
                             // we need to care for the empty string case to
                             // provide an easy way to flush the zstd state
  outOffset += len;

  ZSTD_inBuffer input = {s, len, 0};
  for (;;)
    {
      size_t const remaining = ZSTD_compressStream2(ctx, &output, &input, mode);
      if (ZSTD_isError(remaining))
        {
          vconprint("\nZSTD: ERROR: Failed to compress: ");
          vconprint(ZSTD_getErrorName(remaining));
          vconprint("\n");
        }
      if (remaining <= 0)
        break;
      if (output.pos == BUF_SIZE)
        {
          flush_base64_buffers();
          output.pos = 0;
        }
    }
}

void
store(const void *v, unsigned length)
{
  ZSTD_inBuffer input = {v, length, 0};
  outOffset += length;

  for (;;)
    {
      size_t const remaining =
        ZSTD_compressStream2(ctx, &output, &input, ZSTD_e_continue);
      if (ZSTD_isError(remaining))
        {
          vconprint("\nZSTD: ERROR: Failed to compress: ");
          vconprint(ZSTD_getErrorName(remaining));
          vconprint("\n");
        }
      if (remaining <= 0)
        break;
      if (output.pos == BUF_SIZE)
        {
          flush_base64_buffers();
          output.pos = 0;
        }
    }
}
