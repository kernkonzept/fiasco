INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Thread
{
protected:
  struct Log_thread_exregs : public Tb_entry
  {
    Mword       id, ip, sp, op;
    void print(String_buffer *) const;
  };
};

//--------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include <cstdio>
#include "config.h"
#include "kmem.h"
#include "mem_layout.h"
#include "simpleio.h"
#include "string_buffer.h"

IMPLEMENT
void
Thread::Log_thread_exregs::print(String_buffer *buf) const
{
  buf->printf("D=%lx ip=%lx sp=%lx op=%s%s%s",
              id, ip, sp,
              op & Exr_cancel ? "Cancel" : "",
              ((op & (Exr_cancel | Exr_trigger_exception))
               == (Exr_cancel | Exr_trigger_exception))
               ? ","
               : ((op & (Exr_cancel | Exr_trigger_exception))
                  == 0 ? "0" : "") ,
              op & Exr_trigger_exception ? "TrExc" : "");
}
