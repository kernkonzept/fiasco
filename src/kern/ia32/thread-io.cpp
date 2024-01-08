IMPLEMENTATION [io]:

#include "config.h"

/**
 * Make sure the IO bitmap of the current CPU is up-to-date.
 *
 * \note This method needs to be called with the CPU lock held.
 *
 * \retval false  The IO bitmap was not updated (it is already up-to-date).
 * \retval true   The IO bitmap was updated (the IO access should be retried).
 */
PRIVATE inline
bool
Thread::update_io_bitmap()
{
  assert(cpu_lock.test());

  Tss *const tss = Cpu::cpus.current().get_tss();
  Context *const current_context = ::current();
  Space *const current_space = current_context->space();

  if (current_space->update_io_bitmap(&tss->_io_bitmap_revision,
                                      &tss->_hw.io.bitmap))
    {
      // Make sure the IO bitmap is actually examined by the CPU.
      tss->_hw.ctx.iopb = offsetof(Tss, _hw.io.bitmap);
      return true;
    }

  return false;
}

IMPLEMENTATION [io && (ia32 || amd64 || ux) && !no_io_pagefault]:

/**
 * Compute port number and size for an IO instruction.
 *
 * Disassamble IO statements to compute the port address and the number of
 * ports accessed.
 *
 * \param[in]  eip    Address of the instruction.
 * \param[in]  ts     Trap state with registers.
 * \param[out] port   Decoded port address.
 * \param[out] size   Decoded number of ports accessed.
 *
 * \retval false  The decoding failed.
 * \retval true   The decoding was successful.
 */
PRIVATE inline
bool
Thread::decode_io(Address eip, Trap_state *ts, unsigned int *port, unsigned int *size)
{
  bool from_user = ts->cs() & 3;

  // handle 1-byte IO instruction
  switch (mem_space()->peek((Unsigned8 *)eip, from_user))
    {
    case 0xec:  // in dx, al
    case 0x6c:  // insb
    case 0xee:  // out dx, al
    case 0x6e:  // outb
      *port = ts->dx() & 0xffff;
      *size = 0;
      return true;
    case 0xed:  // in dx, eax
    case 0x6d:  // insd
    case 0xef:  // out eax, dx
    case 0x6f:  // outd
      *port = ts->dx() & 0xffff;
      *size = 2;

      if (*port + 4 <= Config::Io_port_count)
        return true;

      // Access beyond L4_IOPORT_MAX
      return false;
    case 0xfa:  // cli
    case 0xfb:  // sti
      *port = 0;
      *size = 16; /* 16bit IO space */
      return true;
    }

  // handle 2-byte IO instruction
  if (!(eip < Mem_layout::User_max))
    return false;

  switch (mem_space()->peek((Unsigned8 *)eip, from_user))
    {
    case 0xe4:  // in imm8, al
    case 0xe6:  // out al, imm8
      *port = mem_space()->peek((Unsigned8 *)(eip + 1), from_user);
      *size = 0;
      return true;
    case 0xe5:  // in imm8, eax
    case 0xe7:  // out eax, imm8
      *port = mem_space()->peek((Unsigned8 *)(eip + 1), from_user);
      *size = 2;

      if (*port + 4 <= Config::Io_port_count)
        return true;

      // Access beyond L4_IOPORT_MAX
      return false;
    case 0x66:  // operand size override
      switch (mem_space()->peek((Unsigned8 *)(eip + 1), from_user))
        {
        case 0xed:  // in dx, ax
        case 0xef:  // out ax, dx
        case 0x6d:  // insw
        case 0x6f:  // outw
          *port = ts->dx() & 0xffff;
          *size = 1;

          if (*port + 2 <= Config::Io_port_count)
            return true;

          // Access beyond L4_IOPORT_MAX
          return false;
        case 0xe5:  // in imm8, ax
        case 0xe7:  // out ax,imm8
          *port = mem_space()->peek((Unsigned8 *)(eip + 2), from_user);
          *size = 1;

          if (*port + 2 <= Config::Io_port_count)
            return true;

          // Access beyond L4_IOPORT_MAX
          return false;
        }
      [[fallthrough]];
    case 0xf3:  // REP
      switch (mem_space()->peek((Unsigned8 *)(eip + 1), from_user))
        {
        case 0x6c:  // REP insb
        case 0x6e:  // REP outb
          *port = ts->dx() & 0xffff;
          *size = 0;
          return true;
        case 0x6d:  // REP insd
        case 0x6f:  // REP outd
          *size = 2;
          *port = ts->dx() & 0xffff;

          if (*port + 4 <= Config::Io_port_count)
            return true;

          // Access beyond L4_IOPORT_MAX
          return false;
        }
    }

  // handle 3-byte IO instruction
  if (!(eip < Mem_layout::User_max - 1))
    return false;

  Unsigned16 word = mem_space()->peek((Unsigned16 *)eip, from_user);

  // sizeoverride REP or REP sizeoverride
  if (word == 0x66f3 || word == 0xf366)
    {
      switch (mem_space()->peek((Unsigned8 *)(eip + 2), from_user))
        {
        case 0x6d:  // REP insw
        case 0x6f:  // REP outw
          *port = ts->dx() & 0xffff;
          *size = 1;

          if (*port + 2 <= Config::Io_port_count)
            return true;

          // Access beyond L4_IOPORT_MAX
          return false;
        }
    }

  // nothing appropriate found
  return false;
}

/**
 * Handle IO bitmap fault.
 *
 * \param ts  Trap state.
 *
 * \retval Ignored  The fault has not been handled.
 * \retval Retry    The fault has been handled, the IO access should be retried.
 * \retval Fail     The fault has been handled, the IO access failed.
 */
PRIVATE inline
Thread::Io_return
Thread::handle_io_fault(Trap_state *ts)
{
  Address eip = ts->ip();

  // Exception #13 (#GP) means that either (a) the access to the IO bitmap
  // was not within the bounds of the TSS segment or (b) the corresponding bit
  // in the IO bitmap is not set (i.e. the port is not enabled).
  //
  // We have to dispatch the code at the faulting eip to deterine the IO port
  // and send an IO flexpage to our pager.

  if (eip <= Mem_layout::User_max && ts->_trapno == 13
      && (ts->_err & 7) == 0)
    {
      // If the IO bitmap is not up-to-date, we update it and retry.
      if (update_io_bitmap())
        return Retry;

      unsigned int port;
      unsigned int size;

      if (decode_io(eip, ts, &port, &size))
        {
          Mword io_page = L4_fpage::io(port, size).raw();

          // Set user mode flag to get correct IP in handle_page_fault_pager
          // pretend a write page fault.
          static const unsigned io_error_code = PF_ERR_WRITE | PF_ERR_USERMODE;

          CNT_IO_FAULT;

          if (EXPECT_FALSE(log_page_fault()))
            page_fault_log(io_page, io_error_code, eip);

          // Treat it as a page fault in the region above 0xf0000000.

          // We could also reset the Thread_cancel at slowtraps entry but it
          // could be harmful for debugging (see also comment at slowtraps:).
          //
          // This must be done while interrupts are off to prevent that an
          // other thread sets the flag again.
          state_del(Thread_cancel);

          // Set CR2 in ts so that we also get the io_page value in an
          // consecutive exception
          ts->_cr2 = io_page;

          if (EXPECT_FALSE((state() & Thread_alien)
                           || vcpu_exceptions_enabled(vcpu_state().access())))
            {
              // Special case for alien tasks: Don't generate fault but
              // send exception to pager.
              ts->_err = 0;
              clear_recover_jmpbuf();

              if (send_exception(ts))
                return Retry;

              // Fail, don't send exception again.
              return Fail;
            }

          bool ipc_code = handle_page_fault_pager(_pager, io_page,
                                                  io_error_code,
                                                  L4_msg_tag::Label_io_page_fault);

          if (ipc_code)
            {
              clear_recover_jmpbuf();
              return Retry;
            }

          // Send IO-Port fault.
          ts->_err = 0;
        }
    }

  return Ignored;
}

// ----------------------------------------------------------
IMPLEMENTATION [io && no_io_pagefault]:

#include "cpu.h"

/**
 * Handle IO bitmap fault.
 *
 * \param ts  Trap state.
 *
 * \retval Ignored  The fault has not been handled.
 * \retval Retry    The fault has been handled, the IO access should be retried.
 * \retval Fail     The fault has been handled, the IO access failed.
 */
PRIVATE inline
Thread::Io_return
Thread::handle_io_fault(Trap_state *ts)
{
  // Exception #13 (#GP) means that either (a) the access to the IO bitmap
  // was not within the bounds of the TSS segment or (b) the corresponding bit
  // in the IO bitmap is not set (i.e. the port is not enabled).
  if (ts->ip() <= Mem_layout::User_max && ts->_trapno == 13
      && (ts->_err & 7) == 0)
    {
      // If the IO bitmap is not up-to-date, we update it and retry.
      if (update_io_bitmap())
        return Retry;

      // We dispatch the exception to the pager.
      ts->_cr2 = 0;
      ts->_err = 0;
      clear_recover_jmpbuf();

      if (send_exception(ts))
        return Retry;

      // Fail, don't send exception again.
      return Fail;
    }

  return Ignored;
}
