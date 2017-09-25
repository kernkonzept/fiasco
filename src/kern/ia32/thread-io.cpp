IMPLEMENTATION [io && (ia32 || amd64 || ux)]:

//
// disassamble IO statements to compute the port address and
// the number of ports accessed
//

/** Compute port number and size for an IO instruction.
    @param eip address of the instruction
    @param ts thread state with registers
    @param port return port address
    @param size return number of ports accessed
    @return true if the instruction was handled successfully
      false otherwise
*/
bool
Thread::get_ioport(Address eip, Trap_state *ts, unsigned *port, unsigned *size)
{
  int from_user = ts->cs() & 3;

  // handle 1 Byte IO
  switch (mem_space()->peek((Unsigned8*)eip, from_user))
    {
    case 0xec:			// in dx, al
    case 0x6c:			// insb
    case 0xee:			// out dx, al
    case 0x6e:			// outb
      *size = 0;
      *port = ts->dx() & 0xffff;
      return true;
    case 0xed:			// in dx, eax
    case 0x6d:			// insd
    case 0xef:			// out eax, dx
    case 0x6f:			// outd
      *size = 2;
      *port = ts->dx() & 0xffff;
      if (*port + 4 <= Mem_layout::Io_port_max)
	return true;
      else		   // Access beyond L4_IOPORT_MAX
	return false;
    case 0xfa:			// cli
    case 0xfb:			// sti
      *size = 16; /* 16bit IO space */
      *port = 0;
      return true;
    }

  // handle 2 Byte IO
  if (!(eip < Mem_layout::User_max))
    return false;

  switch (mem_space()->peek((Unsigned8 *)eip, from_user))
    {
    case 0xe4:			// in imm8, al
    case 0xe6:			// out al, imm8
      *size = 0;
      *port = mem_space()->peek((Unsigned8 *)(eip + 1), from_user);
      return true;
    case 0xe5:			// in imm8, eax
    case 0xe7:			// out eax, imm8
      *size = 2;
      *port = mem_space()->peek((Unsigned8 *)(eip + 1), from_user);
      return *port + 4 <= Mem_layout::Io_port_max ? true : false;

    case 0x66:			// operand size override
      switch (mem_space()->peek((Unsigned8 *)(eip + 1), from_user))
	{
	case 0xed:			// in dx, ax
	case 0xef:			// out ax, dx
	case 0x6d:			// insw
	case 0x6f:			// outw
	  *size = 1;
	  *port = ts->dx() & 0xffff;
	  if (*port + 2 <= Mem_layout::Io_port_max)
	    return true;
	  else		   // Access beyond L4_IOPORT_MAX
	    return false;
	case 0xe5:			// in imm8, ax
	case 0xe7:			// out ax,imm8
	  *size = 1;
	  *port = mem_space()->peek((Unsigned8 *)(eip + 2), from_user);
	  if (*port + 2 <= Mem_layout::Io_port_max)
	    return true;
	  else
	    return false;
	}

    case 0xf3:			// REP
      switch (mem_space()->peek((Unsigned8*)(eip + 1), from_user))
	{
	case 0x6c:			// REP insb
	case 0x6e:			// REP outb
	  *size = 0;
	  *port = ts->dx() & 0xffff;
	  return true;
	case 0x6d:			// REP insd
	case 0x6f:			// REP outd
	  *size = 2;
	  *port = ts->dx() & 0xffff;
	  if (*port + 4 <= Mem_layout::Io_port_max)
	    return true;
	  else		   // Access beyond L4_IOPORT_MAX
	    return false;
	}
    }

  // handle 3 Byte IO
  if (!(eip < Mem_layout::User_max - 1))
    return false;

  Unsigned16 w = mem_space()->peek((Unsigned16 *)eip, from_user);
  if (w == 0x66f3 || // sizeoverride REP
      w == 0xf366)   // REP sizeoverride
    {
      switch (mem_space()->peek((Unsigned8 *)(eip + 2), from_user))
	{
	case 0x6d:			// REP insw
	case 0x6f:			// REP outw
	  *size = 1;
	  *port = ts->dx() & 0xffff;
	  if (*port + 2 <= Mem_layout::Io_port_max)
	    return true;
	  else		   // Access beyond L4_IOPORT_MAX
	    return false;
	}
    }

  // nothing appropriate found
  return false;
}

PRIVATE inline
int
Thread::handle_io_page_fault(Trap_state *ts)
{
  Address eip = ts->ip();
  if (!check_io_bitmap_delimiter_fault(ts))
    return 0;

  // Check for IO page faults. If we got exception #14, the IO bitmap page is
  // not available. If we got exception #13, the IO bitmap is available but
  // the according bit is set. In both cases we have to dispatch the code at
  // the faulting eip to deterine the IO port and send an IO flexpage to our
  // pager. If it was a page fault, check the faulting address to prevent
  // touching userland.
  if (eip <= Mem_layout::User_max &&
      ((ts->_trapno == 13 && (ts->_err & 7) == 0) ||
       (ts->_trapno == 14 && Kmem::is_io_bitmap_page_fault(ts->_cr2))))
    {
      unsigned port, size;
      if (get_ioport(eip, ts, &port, &size))
        {
          Mword io_page = L4_fpage::io(port, size).raw();

          // set User mode flag to get correct IP in handle_page_fault_pager
          // pretend a write page fault
          static const unsigned io_error_code = PF_ERR_WRITE | PF_ERR_USERMODE;

	  CNT_IO_FAULT;

          if (EXPECT_FALSE (log_page_fault()))
	    page_fault_log(io_page, io_error_code, eip);

          // treat it as a page fault in the region above 0xf0000000,

	  // We could also reset the Thread_cancel at slowtraps entry but it
	  // could be harmful for debugging (see also comment at slowtraps:).
	  //
	  // This must be done while interrupts are off to prevent that an
	  // other thread sets the flag again.
          state_del(Thread_cancel);

	  // set cr2 in ts so that we also get the io_page value in an
	  // consecutive exception
	  ts->_cr2 = io_page;

	  if (EXPECT_FALSE((state() & Thread_alien)
                           || vcpu_exceptions_enabled(vcpu_state().access())))
	    {
	      // special case for alien tasks: Don't generate pagefault but
	      // send (pagefault) exception to pager.
	      ts->_trapno = 13;
              ts->_err = 0;
              _recover_jmpbuf = 0;
	      if (send_exception(ts))
		return 1;
              else
		return 2; // fail, don't send exception again
            }

          bool ipc_code = handle_page_fault_pager(_pager, io_page,
                                                  io_error_code,
                                                  L4_msg_tag::Label_io_page_fault);

          if (ipc_code)
            {
              _recover_jmpbuf = 0;
              return 1;
            }

          // send IO-Port fault allways as trap 13 even if it comes from a PF
          // on the IO bitmap (would be trap 14 with io flex page in cr2)
          ts->_trapno = 13;
          ts->_err = 0;
        }
    }
  return 0; // fail
}
