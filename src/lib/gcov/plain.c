#include "output.h"
#include "base64.h"

unsigned buf_idx;

// run length encoding (rle) mechanics using a counter buffer (cb)
struct
{
  char init; // is initialized
  char last; // character
  int count; // how often the character was seen
} cb = {0, 0, 0};

// Flush and print the runlength encoding buffer
static void
rle_flush(void)
{
  if (cb.init)
    {
      if (cb.count > 3)
        { // worth it to compress
          char const res[3] = {'@', cb.last, b64[cb.count]};
          vconprintn(res, 3);
        }
      else
        {
          char res[3];
          for (int c = 0; c < cb.count; c++)
            res[c] = cb.last;
          vconprintn(res, cb.count);
        }
    }
  cb.init = 0;
}

// add character to output
static void
rle(char const c)
{
  if (cb.init && cb.last == c && cb.count < 63)
    {
      // if the buffer is initialized, and the character is a repeat, and it was
      // repeated less than 63 times increase the count.
      cb.count++;
    }
  else
    {
      // otherwise flush buffer if filled and initialize with new character
      if (cb.init)
        rle_flush();
      cb.init  = 1;
      cb.last  = c;
      cb.count = 1;
    }
}

// runlength encode the raw base64 buffer
void
flush_base64_buffers(void)
{
  //printf("\nflushing. idx: %d\n", buf_idx);
  int n_out = print_base64(buffer, buf_idx, outbuf);
  for (int i = 0; i < n_out; i++)
    rle(outbuf[i]);
  buf_idx = 0;
  rle_flush();
}


void
store(void const *v, unsigned length)
{
  char const *cp = v;
  unsigned offset = 0;

  // Each loop iteration fills the buffer at most once. This means it should
  // either flush at the end or, if not, be the last iteration.
  while (offset < length)
    {
      // Fill rest of the buffer.
      unsigned n_to_fill = BUF_SIZE - buf_idx;

      // Check if rest of values fits completely into the buffer.
      if (n_to_fill > length - offset)
        n_to_fill = length - offset;

      // Fill the buffer.
      for (unsigned i = 0; i < n_to_fill; ++i)
        {
          buffer[buf_idx] = cp[offset];
          ++buf_idx;
          ++offset;
        }

      // The last iteration might not fill the buffer completely. That's why we
      // have to check whether the buffer is filled before flushing.
      if (buf_idx == BUF_SIZE)
        flush_base64_buffers();
    }
}
