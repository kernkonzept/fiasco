.. vim: set spell:
.. header:: User Manual for the built-in Fiasco System Debugger
.. footer:: Page ###Page### / ###Total###

=============================
 L4Re Fiasco JDB User Manual
=============================

.. contents::
   :depth: 3
..

.. raw:: pdf

   PageBreak

General Remarks
===============

About JDB
---------

The Fiasco Kernel Debugger (JDB) is primarily a tool to test Fiasco and to fix
bugs in Fiasco. However, it is also useful for testing higher layers.
Consequently,

- JDB always freezes the entire machine state when invoked. All interrupts are
  disabled, even the clock is halted. All CPUs are halted and only the boot CPU
  enters JDB. No kernel or user process proceeds while JDB is active.

- JDB is a stand-alone debugger. It neither use parts of Fiasco nor other
  device drivers. JDB includes simple device drivers for keyboard, display, and
  the serial interfaces. These device drivers are *not* interrupt driven.

Although developed and traditionally packaged together with Fiasco, the Fiasco
Kernel Debugger is *not* part of the Fiasco μ-kernel. Fiasco can run without
JDB. The only connection between Fiasco and JDB is that JDB intimately knows
Fiasco’s data structures.

About Exception Handling
------------------------

JDB is invoked by exceptions, the particular exception is necessarily
architecture-specific. Some exceptions are handled normally by Fiasco, for
example a page fault. All other exceptions are distributed by Fiasco either to
JDB or to the current user-level thread. The corresponding algorithm is:

::

  exception dispatch:
    if exception occurred in kernel then
      invoke JDB with kernel error
    elif current thread defined an exception handler then
      invoke user-level exception handler by upcall
    else
      invoke JDB with user error
    fi

Summarizing: JDB is used for kernel errors. It is used for user errors only as
long as no debugger is installed at user level.


.. raw:: pdf

   PageBreak

Navigation and Abbreviations
----------------------------

Some key bindings correspond to different actions based on context and
based on object type. Below are some common bindings.

.. table::
   :widths: 20 80

   +----------------------------+----------------------------------------------+
   | Key                        | Description                                  |
   +============================+==============================================+
   | *«Esc»* (Escape)           | enter JDB (from running program)             |
   +----------------------------+----------------------------------------------+
   | *«Esc»* (Escape)           | return to top level context (from within JDB)|
   +----------------------------+----------------------------------------------+
   | *«Home»*                   | go up/back one level (in many contexts)      |
   +----------------------------+----------------------------------------------+
   | *«Enter»*                  | select (depends on context)                  |
   +----------------------------+----------------------------------------------+
   | *«PgUp»*, *«PgDn»*,        | navigate page left, down, begin, end         |
   | *«Home»*, *«End»*          |                                              |
   +----------------------------+----------------------------------------------+
   | ``H``, ``J``, ``K``, ``L`` | navigate page left, down, begin, end         |
   +----------------------------+----------------------------------------------+
   || ``←``, ``↑``, ``↓``, ``→``| navigate left, down, up, right               |
   || (cursor keys)             |                                              |
   +----------------------------+----------------------------------------------+
   | ``h``, ``j``, ``k``, ``l`` | navigate left, down, up, right               |
   +----------------------------+----------------------------------------------+

Some common abbreviations for object listing are shown here.

.. table::
   :widths: 15 85

   ======= ================
   Key     Description
   ======= ================
   ``R``   Reference count
   ``C``   Capability
   ``C``   CPU
   ``D``   Debug IDentifier
   ``DID`` Debug IDentifier
   ======= ================

Status line
-----------

In many commands the bottom of the screen shows a status bar suggesting
further context-specific commands.


.. raw:: pdf

   PageBreak

General Commands
================

Enter JDB: *«Esc»*
------------------

Enter the kernel debugger from a running program. Requires that the kernel be
built with the ``CONFIG_JDB`` option.

To enable entering JDB using the *«Escape»* key refer to the flags
``-serial_esc`` and ``-esc``.

For another way of entering JDB refer to ``l4_kd_enter()``.

Go: ``g``
---------

Exits JDB and resumes execution after/at the instruction which invoked
JDB.

Help: ``h``
-----------

Shows a screen with a short description of JDB commands.

Command mode: ``*`` / ``mode``
------------------------------

Switch between long and short command mode.

Debug Message: *«Return»*
-------------------------

Shows the CPU, instruction pointer and trap type for the entry into JDB.


JDB options: ``J``
------------------

.. table::
   :widths: 15 85

   ================ ===========================================================
   Cmd              Abbreviation
   ================ ===========================================================
   ``JS``           Use full screen for JDB. Adapts the width / height of the
                    screen to the width / height of the terminal window by
                    detecting its current size using escape sequences. Some
                    terminals may not support the necessary escape sequence and
                    can be set using ``Jw`` and ``Jh``.
   ``Jw``           Set the JDB screen width.
   ``Jh``           Set the JDB screen height. Also used to revert from full
                    screen.
   ``Jc``           Set the color of the debug prompt.
   ``Jd-``/``Jd+``  Disable/enable direct console.
   ``Ji``           Show screen information.
   ``Jo``           List attached consoles.
   ================ ===========================================================


Screen Output Buffer: ``B``
---------------------------

The output buffer stores all output written to the Fiasco kernel debugger
console.

.. table::
   :widths: 15 85

   =========== ===================================
   Cmd         Abbreviation
   =========== ===================================
   ``B``       Show complete output buffer.
   ``Bn``      Show last n lines of output buffer.
   ``B/«str»`` Print lines that contain «str».
   =========== ===================================


.. raw:: pdf

   PageBreak

Information about Kernel State
==============================

System Report: ``Y`` / ``report``
---------------------------------

Generate a system report or ``Yc`` for a compressed system report.

``Y{c}|report``
    (compressed) system report

This generates a screen dump equivalent to running the following JDB commands:

* List Kernel Objects,
* List Threads,
* List Timeouts,
* Show Kernel Information and subcommands,
* Show IPI Information,
* Show CPU Information about CPU calls,
* Show RCU Information,
* Trace Buffer,
* Console Output,
* Show Thread Control Block for all threads.

Kernel Information: ``k``
-------------------------

Displays some global Fiasco kernel information. Some common subcommands are
included here.

.. table::
   :widths: 15 85

   ======= ==========================================================
   Cmd     Abbreviation
   ======= ==========================================================
   ``ka``  Show Local APIC state (x86, amd64)
   ``kA``  Show I/O APIC state (x86, amd64)
   ``kb``  Benchmark privileged instructions (arm, x86, amd64)
   ``kc``  Show CPU features
   ``kd``  Show Debug Registers (x86, amd64)
   ``kf``  Interprets the content of the kernel info page
   ``kg``  Show Global Descriptor Table (GDT) (x86, amd64)
   ``kh``  Short help about available subcommands
   ``ki``  Miscellaneous kernel info
   ``kI``  Show Interrupt Descriptor Table (IDT) (x86, amd64)
   ``km``  Kernel memory footprint
   ``kM``  Show Memory Type Range Registers
   ``kp``  Show PIC ports (x86, amd64)
   ``kP``  Show PCI devices (x86, amd64)
   ======= ==========================================================


.. raw:: pdf

   PageBreak

List Kernel Objects: ``Q`` / ``listkobj``
-----------------------------------------

List kernel objects. Show information and explore the relationships between
different objects. For each object the listing shows the debug ID, object
pointer, object type, and debug name string (if available). The remainder of
the line shows object-specific information.

Typical object information:

**«DID» «object address» [«object type»] {«debug_name»} ...
«object-specific information»**

The status line shows possibilities for drilling down deeper and also shows
object-specific keys.

*«Enter»*
    Display further information about some object types.
*«Space»*
    Cycle through different sublists of object types.
*«Tab»*
    Link to related or parent objects.
``/``
    Filter the displayed information using a regular expression.

The following shows some examples of «object-specific information»:

.. table::
   :widths: 15 85

   =============== =============================================================
   Key             Description
   =============== =============================================================
   ``D=``          Debug identifier (DID) of the thread the IPC gate is attached
                   to.
   ``C=``          CPU the thread is running on
   ``R=``          Object reference count
   ``S=D:``        DID of task providing dataspace
   ``L=``          Gate or IRQ object ID (label)
   ``I=``          IRQ pin (0 if not applicable)
   ``Soft|Hard``   IRQ chip type
   ``F=``          IRQ flags
   ``T=``          DID of thread triggered by IRQ
   ``Q=``          0|1: IRQ is queued or not
   ``c=.. l=..``   Allocated quota / total quota for a factory (0 means
                   unlimited)
   =============== =============================================================


.. table::
   :widths: 15 85

   ============= ===========================
   Object        Object-specific Information
   ============= ===========================
   ``[Factory]`` c=«n» l=«n»
   ``[Task]``    R=«n»
   ``[Thread]``  C=«n» R=«n
   ``[Thread]``  C=«n» S=D: R=«n»
   ``[Gate]``    L=«n» D=«DID»
   ``[IRQ ipc]`` I=«object» Soft L=«label»
   ``[IRQ sem]`` I=«0» Soft F= Q=<queued?>
   ============= ===========================


.. raw:: pdf

   PageBreak

Task list: ``s`` / ``spacelist``
--------------------------------

Show task list. For each object the listing shows the debug ID, object pointer,
object type, debug name string (if available) and reference count.

The status line shows possibilities for drilling down deeper.

*«Enter»*
    Display task UTCB addresses and memory usage.
``o=objspace p=ptab m=mappings``
    Drill down for object space, page table and mapping database.

Thread Present List / Thread Ready List: ``lp`` / ``lr`` / ``list``
-------------------------------------------------------------------

Display the *present list* (``lp`` command) or the *ready list* (``lr`` command).

For each thread the following information is displayed: debug ID, current CPU,
debug name (if available), priority, debug ID of the space, debug ID of thread
being waited on (if any), timeout, thread state.

.. table::
   :widths: 15 85

   ======= =======================================================
   Cmd     Abbreviation
   ======= =======================================================
   ``lp``  Display list of thread present.
   ``lr``  Display list of threads with state set to ready to run.
   ======= =======================================================

Keys:

.. table::
   :widths: 15 85

   +--------------+-----------------------------------------------------------+
   | Key          | Description                                               |
   +==============+===========================================================+
   | ``↑``, ``↓`` | Scroll in the list.                                       |
   +--------------+-----------------------------------------------------------+
   | *«Enter»*    | Show the TCB of the selected thread.                      |
   +--------------+-----------------------------------------------------------+
   | *«Home»*     | Navigate back to the list.                                |
   +--------------+-----------------------------------------------------------+
   | *«Space»*    | Cycle through the sort mode: Unsorted, sorted by thread   |
   |              | priority, sorted by thread identifier, and sorted by      |
   |              | space.                                                    |
   +--------------+-----------------------------------------------------------+
   | *«Tab»*      | Switches to the thread in the list that the current       |
   |              | thread is waiting for.                                    |
   +--------------+-----------------------------------------------------------+

Note that the thread name prefixed with ``#`` refers to the L4Re thread
typically serving as default page fault handler, exception handler, and region
manager for other threads in the system. For example:

::

   19 810902f8 [Task   ] {hello           } R=3
   1a 80f480f8 [Thread ] {#hello          } C=0 S=D:19 R=1
   1d 80f4a0f8 [Thread ] {hello           } C=0 S=D:19 R=1

Timeout List: ``lt``
--------------------

Display all current active timeouts.

Keys:

============ ==================================================
Key          Description
============ ==================================================
``↑``, ``↓`` Scroll in the list.
*«Enter»*    Show the TCB of the owner of the selected timeout.
============ ==================================================


Sender list: ``ls`` / ``senderlist``
------------------------------------

Show sender list of a thread.


.. raw:: pdf

   PageBreak

Thread Control Block: ``t`` / ``tcb``
-------------------------------------

Displays the commented thread control block (TCB), the thread’s kernel
stack, and the thread’s general purpose registers.

``t``
    Displays the thread control block of the current thread.

``txxx``
    Displays the thread control block of thread xxx. xxx is the global
    debugger ID and is hexadecimal.

``t+/–``
    Enable/disable automatic displaying of current thread control block
    when entering kernel debugger.

*«thread control block»*
    The fields of the thread control block (TCB) are labeled.

*«kernel stack»*
    The kernel stack of the displayed thread is dumped from the
    current stack top (first line) down to the stack bottom. Use the cursor keys
    to scroll inside the TCB borders of the displayed thread. Press the
    *«Enter»* key to dump the address at the cursor in memory mode. Press the
    *«Space»* key to disassemble the address at the cursor (requires the
    kernel to be configured with ``CONFIG_JDB_DISASM``).

    When entering kernel mode, the processor first pushes an
    architecture-specific trap frame onto the kernel stack. The stack pointer,
    instruction pointer, a subset of registers and flags are displayed on the
    bottom line.

*«registers»*
    are displayed if the *currently active* thread is displayed; this is the
    last thread running before entering JDB. For this thread JDB gets the
    registers from the trap frame on the stack. Depending on their state other
    threads might not have saved all their registers on the stack.


**Thread Control Block Detail**

The thread control block display contains very detailed information.
Some of the key fields are annotated here. The details are architecture
specific and may change in the future however this should serve as a
useful starting point.

::

    1 thread  :  20 <0xfcd5f000>      CPU: 0:0        prio: 02
    2 state   : 8008 rcv_wait,fpu
    3 wait for:   20  polling:        rcv descr: 00000000     timeout:
    4 cpu time:  67.000 ms            timeslice: 7540 us
    5 pager   : [C:   3] D:  1d       task     : D:  1c
    6 exc-hndl: [C:   3] D:  1d       UTCB     : fcd56200/b3000200
    7 vCPU    : ---
    8 vCPU    : ---
    9
   10
   11 EAX=00000000  ESI=00000000              popl    %ebp
   12 EBX=00000000  EDI=01081020              popl    %ebx
   13 ECX=00000000  EBP=00000000
   14 EDX=fffff802  ESP=80007e20  SS=0023
   15 in ipc (user level registers)
   16 CS=009b  EIP=01003a12  EFlags=00000200 kernel ESP=fcd5fee8
   17 fcd5fee8 06000000 000c0000 00056000 00000046 f002df30
   18     eec  fcd5f000 00000000 fcd90000 fcd5f000 00000000
   19     f00  fcd5f000 f002e5c4 fcd5ff3c fcd5ff9c b3000200
   20     f14  fcd5f000 f0038333 00000001 fcd81207 fcd54494

.. raw:: pdf

   PageBreak

line 1
    ``thread: «DID» <«object_address»> CPU:`` «current_cpu»:«home cpu» «priority»

line 2
    ``thread state:`` bit mask, decoded state

    These are some common thread states:

    :``ready``:
        Thread can be scheduled.
    :``drq_rdy``:
        DRQ pending for this context.
    :``send``:
        Waiting to send a message.
    :``rcv_wait``:
        Waiting for a message.
    :``rcv_in_progr``:
        Actively receiving a message. A thread is carrying
        this flag while performing the IPC transfer operation
        to itself in the context of the next sender.
    :``transfer``:
        Passively waiting until this flag is cleared by the
        partner. Either this thread receives a message from
        the partner of the partner is sending a message to
        this context.
    :``trans_failed``: The IPC operation is canceled by the receiver.
    :``cancel``:
        State has changed -- cancel activity.
    :``timeout``:
        IPC timeout hit. Either expired, or the timeout is
        zero and there is no sender at the IPC receive phase.
    :``dead``:
        inactive (not in any queue)
    :``dyning``:
        Thread is about to be killed and must not execute user code.
    :``migrate``:
        Thread must finish migration before executing userland code.
    :``resched``:
        needs rescheduling
    :``fpu``: body
        FPU owner.
    :``alien``:
        Thread not allowed to do any system call.
    :``dealien``:
        Alien thread is allowed to do next system call only.
    :``exc_progr``:
        Exception in progress.
    :``drq``:
        Thread polls for DRQs.
    :``rcu_wait``:
        Thread waits for an RCU grace period.
    :``vcpu``:
        vCPU is enabled for thread.
    :``vcpu_user``:
        Thread is a vCPU user thread.
    :``vcpu_fpu_disabled``:
        FPU disabled in vCPU.
    :``vcpu_ext``:
        Extended vCPU thread (guest extentions).

line 3
    ``wait for:`` DID of thread being waited on, self = sleep
    ``polling:`` DID of send partner, waiting for receiver while inside IPC call
    ``timeout:`` wait duration of current timeout

line 4
    cpu time spent in thread

line 5
    ``pager:`` C: capability slot of the pager and DID
    ``task:`` DID of parent task

line 6
    ``exc-hndl: C:``
        capability slot of thread's exception handler and DID
    ``UTCB :``
        kernel_address/user_address

line 7
     ``VCPU:`` if vCPU present:
        pointer to vCPU state: kernel address / user address,
        ``S=`` DID of guest address space
line 8
    ``VCPU:`` if vCPU present:
       ``c=`` vCPU current state, ``s=`` saved vCPU state, ``sf=`` sticky
       flags,

       e.g. ``c=FuDEpi s=FuDEpi sf=-`` upper case is set, lower case is
       clear

       ``e-ip=«addr»``
       ``e-sp=«addr»``
       host exit handler: entry instruction pointer and entry stack pointer

line 11
    registers, current instruction pointer (disassembled if avail.)
line 12
    registers, next instruction (disassembled if avail.)
line 13
    registers
line 14
    registers
line 15
    information line
line 16
     Architecture-specific subset of registers
line 17
     This is the TCB and kernel stack, e.g. thread object starting at the
     current stack pointer.
line 18
     kernel stack

kernel stack continues below ...

Display Thread ID of TCB address: ``t?``
----------------------------------------

Used to find the thread ID corresponding to a TCB address. The command
prompts for a TCB address (e.g. from within a thread’s TCB dump).


.. raw:: pdf

   PageBreak


List IRQ threads: ``R`` / ``irq``
---------------------------------

``Rl``
    list all IRQ threads


Show UTCB and vCPU State: ``z``
-------------------------------

This command dumps the address and the contents of the UTCB of a thread and
also the vCPU state if applicable for the thread.

``z``
    Display the UTCB of the current thread.

``zxxx``
    Display the UTCB of thread with debug ID xxx.

``zP<kobj_ptr>``
    Display the UTCB of thread with kernel object address <kobj_ptr>


Show Kernel Object: ``K`` / ``kobj``
------------------------------------

Show information about the specified kernel object.

``K<kobj_ptr>``
    Show information for kernel object at address ``obj_ptr``.


.. raw:: pdf

   PageBreak

Show IPI information: ``ipi``
-----------------------------

Show information about inter-process interrupts. For each hardware CPU, show the
number of received and sent IPIs.


Show CPU state: ``cpustate``
----------------------------

``cpustate all``
    Show information about all CPUs.

``cpustate «n»``
    Show information about a single CPU.


.. raw:: pdf

   PageBreak

Mapping: ``m`` / ``mapdb``
--------------------------

Parse the Fiasco mapping database.

``mxxxxx``
    Starts parsing the mapping subtree corresponding to physical page frame
    xxxxx000. The *CursorUp* and *CursorDown* keys can be used to proceed to
    the next or previous page.


Dump Page Tables: ``p`` / ``ptab``
----------------------------------

Displays page tables and virtual memory.

``p``
    displays the higher-level page table (page directory) of the current address
    space.

    The cursor keys, *«PgUp»*, *«PgDn»* and *«Home»* can be used to move the
    dump cursor and display further memory. Pressing the *«Enter»* key when the
    cursor points to a valid lower-level page table switches to this table. In
    the same way, the *«Enter»* key there switches to the data page. The
    *«Home»* key always “returns” to the lower- or higher-level page table,
    respectively.

``pxxx``
    displays the higher-level page table of task ``xxx``.

*«Space»*
    changes between page table mode and raw mode.

Page-table entry formats:

``–``
    Nil entry.
``xxxx–r``
    User-level, read-only, pointing to physical address ``0xxxx000``.
``xxxx–w``
    User-level, read/write, pointing to physical address ``0xxxx000``.
``xxxx–R``
    Kernel, read-only, pointing to physical address ``0xxxx000``.
``xxxx–W``
    Kernel, read/write, pointing to physical address ``0xxxx000``.
``xx/4–{r,w,R,W}``
    4M-page entry, pointing to physical address
``0xx×4MB.``
    4M-page entry.

Dump object mapping database: ``dumpmapdbobjs``
-----------------------------------------------

Dump the complete object mapping database compressed and uuencoded to the
console.


.. raw:: pdf

   PageBreak

Monitoring Kernel Events
========================

Monitor Extended Logging Events: ``O``
--------------------------------------

Select from a list of extended logging events which can be activated or
deactivated for monitoring. For a complete list of available events type
``O`` without any argument.

The JDB extended logging feature can be completely disabled at compile
time and must be enabled using the configuration option ``JDB_LOGGING``.

Press the *«Enter»* key to toggle individual events and navigate using the
following:

``↑``, ``↓``, ``j``, ``k``, *«PgUp»*, *«PgDn»*, *«Home»*, *«End»*

Unlike the ``P`` command and the ``I`` command, enabling the extended logging
feature adds a small additional overhead at runtime (and even smaller when
events are individually deactivated).


.. raw:: pdf

   PageBreak

IPC Monitoring: ``I``
---------------------

Monitors IPC operations. IPCs are monitored *before* they are handled by
Fiasco.

Some abbreviations used in IPC log events are shown here.

``ipc => C:capability DID:«debug identifier» L:«internal object ID» TO:«timeout»``

``I+``
    switches *single* IPC monitoring on. Whenever an IPC operation is invoked
    the following information is displayed: message type, current thread debug
    ID, operation type (call, send, wait, wait for, reply and wait), [raw
    capability used for destination], destination thread debug ID (if
    specified), [protocol ID ORed with the message flags], message words 0 and
    1 and the instruction pointer are displayed. The system stops until a key
    is hit on the debug console. Pressing the *«i»* key invokes the full JDB
    menu; any other key resumes normal operation, i.e. starts normal IPC
    handling.

``I–``
    switches IPC monitoring off (both *single* and *traced*).

``I*``
    switches *traced* IPC monitoring on. Instead of presenting any single IPC,
    all IPCs are monitored in a trace buffer. When JDB is next entered, the
    entries can be displayed using the *dump trace* command ``T``.

``IR+``
    switches monitoring of IPC system call results on. To log these events, IPC
    logging must be enabled either by ``I+`` or by ``I*``.

``IR–``
    switches monitoring of IPC system call results off.

``IT+``
    Use special log format containing both IPC data and IPC result. The format
    is used together with the L4 userland tracing library.

``IT–``
    Switch back to normal IPC log format.

``Ir...``
    restricts the monitored IPCs. IPCs that do not meet all specified
    restrictions are ignored by the monitoring system.
``... txxx``
    restricts IPCs to thread ``xxx``.
``... Txxx``
    restricts IPCs to threads ``!=xxx``.
``... axxx``
    restricts IPCs to task ``xxx``.
``... Axxx``
    restricts IPCs to tasks ``!=xxx``.
``...s``
    Only IPCs containing a send part are monitored, i.e. call, send, reply and
    wait.
``...–``
    All IPC restrictions are reset.


.. raw:: pdf

   PageBreak

Page Fault Monitoring: ``P``
----------------------------

Monitors page faults. Page faults are monitored *before* they are
handled by Fiasco.

``P+``
    Switches *single* page fault monitoring on. Whenever a page fault occurs,
    page fault address, instruction pointer and thread number are displayed and
    the system stops until a key is hit on the debug console. Pressing the
    *«i»* key invokes the full JDB menu; any other key resumes normal
    operation, i.e. starts normal page fault handling.

``P–``
    Switches page fault monitoring off (both *single* and *traced*).

``P*``
    Switches *traced* page fault monitoring on. Instead of presenting any
    single page fault all page faults are monitored in a trace buffer. When JDB
    is invoked the next time, the entries can be displayed by the *dump trace*
    command ``T``.

``PR+``
    Enables monitoring of the return code of the page fault handler. To log
    these events, PF logging must be enabled either by ``P+`` or by ``P*``.

``PR–``
    Disables monitoring of the page fault handler return code.

``Pr...``
    Restricts the monitored page fault. Page faults that do not meet all
    specified restrictions are ignored by the monitoring system.
``... txxx``
    Restricts page faults to thread ``xxx``.
``... Txxx``
    Restricts page faults to threads ``!=xxx``.
``... x[y,z]``
    Only page faults with fault addresses inside the
    interval ``[y,z]`` (``y≤z``) or outside the
    interval ``[z,y]`` (``y>z``) are monitored.
``... –``
    All page fault restrictions are reset.


.. raw:: pdf

   PageBreak

Trace Buffer: ``T`` / ``tbuf``
------------------------------

Shows the trace buffer. Trace entry messages are stored in a trace
buffer together with timing information and up to two performance
counter values. The trace buffer size, usage percentage and performance
counter information is shown at the top of the screen. The buffer size
can be modified using the ``-tbuf_entries=`` command line switch.

**ARCH:** Timestamp resolution is architecture-specific

``T``
    Enters trace buffer dump.

The newest information is displayed at the top. Trace buffer entries get
timestamped when they are entered into the buffer. Accordingly, they can be
displayed in various timing modes.

*index*
    Entries are shown with their sequence number in the buffer.

*tsc diff*
    Entries are shown with their time difference (in 40 bit hexadecimal
    notation or μs, ms, or s – switchable by *«Space»* key) to their displayed
    predecessor entry.

*tsc rel*
    Entries are shown with their time difference relative to the reference
    element.

*tsc start*
    Entries are shown with their time difference relative to the system
    start.

*kclock rel*
    The kernel clock with their difference relative to the reference
    element is shown.

*kclock*
    The absolute kernel clock is shown.

*pmc1 diff*
    Entries are shown with the differences of their first performance
    counter.

*pmc2 diff*
    Entries are shown with the differences of their second performance
    counter.

*pmc1 rel*
    Entries are shown with the difference of their first performance
    counter relative to the reference element.

*pmc2 rel*
    Entries are shown with the difference of their second performance
    counter relative to the reference element.

**Key Description**

``←``, ``→``
     Cycle between displaying the event index and the different timing modes.

*«Space»*
    Cycle between displaying raw timestamp counter values and hexadecimal
    timer representation.

``h, l``
    Scroll horizontally.

``↑``, ``↓``, ``j``, ``k``, *«PgUp»*, *«PgDn»*, *«Home»*, *«End»*
    Navigate within trace buffer.

``c``
    Clear the trace buffer.

*«Enter»*
    The code the event occurred at is disassembled. Requires the kernel to be
    configured with ``CONFIG_JDB_DISASM`` otherwise only the instruction pointer
    is shown. The *«Home»* key then returns to the trace buffer dump.

``r``
    Set the *reference entry* used when displaying relative time/counter values.

``jr``
    Jump to the reference element.

``s0-9``
    Set mark0 .. mark9. Marks are highlighted.

``j0-9``
    Jump to mark0 .. mark9.

``/``, ``?``, ``n``
    The trace buffer can be searched for a regular expression (similar to
    *less*).

``F``
    Filter the tracebuffer view by specifying a regular expression.

*«Tab»*
    Toggle the displaying thread names on/off.

``D``
    Dump a number of tracebuffer events compressed and uuencoded to the
    console.

**Kernel Tracing Events**

Kernel events:

- IPCs and IPC results (see ``I*`` command),
- page faults and page fault results (see ``P*`` command),
- other logging events (see ``O`` command),
- break points (see ``b*`` command)

**User Level Tracing Events**

User level programs can add events to the trace buffer and perform a few
simple control operations such as trace buffer clear and dump.

See the JDB user level function call section in this document. See also
``l4_kd_enter()`` and ``fiasco_tbuf_log()``.

**Performance Monitoring**

The following options are only accessible while the trace buffer dump
mode is active and if the CPU supports performance counters.

**ARCH:** Only x86, amd64

**TODO** which ARCHs support perf counters?

``P``
    Used to activate, deactivate and change performance monitoring while you are
    in the trace buffer dump. (This command does not work outside the trace
    buffer dump. Performance monitoring is not available on 486 processors. P5,
    P6, and K7 processors have two counters. Not all events are available for
    each counter.

``P[num]+``
    Turns performance monitoring for counter ``num`` on (kernel and user-mode
    activities)

``P[num]–``
    Turns performance monitoring for counter ``num`` off

``P[num]u``
    Turns performance monitoring on (only user-mode activities)

``P[num]k``
    Turns performance monitoring on (only kernel-mode activities)

``P[num]e``
    Count on edge. This measures not only the fraction of time spent in a
    particular state, but also the average length of time spent in such a state
    (for example, the time spent waiting for an interrupt to be served). This
    option makes only sense with selected events (see hardware documentation).

``P[num]d``
  Count on duration

``P[num]?``
  Select a performance monitoring event from list for counter ``num``.


Kernel Event Counters: ``C`` / ``cnt``
--------------------------------------

Kernel event counters are a low-overhead form of tracing and must first
be enabled using the configuration option ``CONFIG_JDB_ACCOUNTING``. There
are counters for IPCs, context switches, page faults, and other events.

``Cl``
    Show kernel event counters.

``Cr``
    Reset kernel event counters.


.. raw:: pdf

   PageBreak

Debugging and Architecture-Specific Commands
============================================

Reboot: ``^`` / ``exit``
------------------------

The ``^`` key (European keyboard: ``«Shift»-6``) resets the machine.


Dump Memory: ``d`` / ``dump``
-----------------------------

Displays physical and virtual memory.

``dxxxxxxxx``
    displays memory beginning from address xxxxxxxx. The dump is 32-bit-word
    oriented so that addresses are always truncated to 4-byte aligned
    addresses.

    The cursor keys, *«PgUp»*, *«PgDn»* and *«Home»* can be used to move the dump
    cursor and display further memory. The *«Enter»* key can be used to jump to
    the address at the cursor, the *«Home»* key returns if possible. This command
    always displays virtual memory of the current address space. Pages that are
    not mapped to physical memory are shown as .........

``dtyyy xxxxxxxx``
    displays virtual memory of task ``yyy`` beginning from address ``xxxxxxxx``.

``dtyyyyyyyy xxxxxxxx``
   displays virtual memory of a task specifying the low dword of the thread id
   beginning from address xxxxxxxx.

Memory can be displayed in various modes. The *«Space»* key switches between
these modes:

``d``-mode
    Machine words (32-bit or 64-bit) are shown as dwords, uppermost nibble
    leftmost, e.g. ``00000002`` for the value ``2``. Values ``0`` and
    ``FFFFFFFF`` are displayed specially: ``0`` and ``-1``.

``b``-mode
    Machine words (32-bit or 64-bit) are shown bytewise, uppermost byte
    rightmost, e.g. ``02000000`` for the value ``2``. Values ``0`` and
    ``FFFFFFFF`` are not treated differently.

``c``-mode
    Bytes are shown as ASCII characters. Unprintable characters are shown as.

The view offers several keys to change the view:

*«Space»*
    Changes display modes between ``d``, ``b`` and ``c`` mode, see above.

*«Return»*
    Goes to the address under the cursor.

*«Tab»*
    Whether to show contents of adapter memory or not. Reading adapter
    memory may cause unwanted behaviour. Default is not to show adapter
    memory.

``e``
    Memory values can be edited. When pressing the key, a new value for
    the address where the cursor points to can be entered.

``c``
    Pressing the key highlights all values which are up to 0x100000 below
    or above the current value. Only works in d-mode.

``u``
    Changes into disassembler view with the value under the cursor preselected
    as the address as well as the current task (requires kernel to be
    configured with ``CONFIG_JDB_DISASM``).


.. raw:: pdf

   PageBreak

Disassemble Memory: ``u``
-------------------------

Disassembles virtual memory. Requires the kernel to be configured with
``CONFIG_JDB_DISASM``.

``uxxxxxxxx``
    disassembles memory beginning from address xxxxxxxx. The cursor keys,
    *«PgUp»* and *«PgDn»* can be used to move one line backwards or forwards or
    to jump one page backwards or forwards, respectively. This command always
    disassembles virtual memory of the current address space. Pages that are
    not mapped to physical memory are shown as .........

``utyyy xxxxxxxx``
    disassembles virtual memory of task ``y`` beginning from address
    ``xxxxxxxx``.

``utyyyyyyyy xxxxxxxx``
    disassembles virtual memory of a task specifying the low
    dword of the thread id beginning from address ``xxxxxxxx``.


Physical memory: ``A`` / ``adapter``
------------------------------------

Display or modify physical memory.

**ARCH:** Only x86, amd64

``Arxxxxxxxx``
    Read 32 bits from physical address xxxxxxxx and display the result.

``Awxxxxxxxx yyyyyyyy``
    Write 32 bits contained in ``yyyyyyyy`` to address ``xxxxxxxx``.


Machine Specific Registers: ``M``
---------------------------------

Display and modify machine status registers.

**ARCH:** Only x86, amd64

``Mrxxxxxxxx``
    read 32 bits from the machine specific register ``xxxxxxxx`` and display
    the result

``Mwxxxxxxxx yyyyyyyy``
    write 32 bits yyyyyyyy to the machine specific register ``xxxxxxxx``


Performance Monitor Events: ``M``
---------------------------------

Display performance monitor cycle and event counters.

**ARCH:** Only arm, aarch64


.. raw:: pdf

   PageBreak

I/O Bitmap: ``r`` / ``iomap``
-----------------------------

Displays mapped ports, gives information about pages mapped for the I/O
bitmap, and shows the I/O port counter (used for determining privileged
tasks).

**ARCH:** Only x86, amd64

``r``
    display the I/O bitmap for the current task.

``rxxx``
    display the I/O bitmap for task xxx.


Port I/O: ``i``/``o`` / ``in`` / ``out``
----------------------------------------

**ARCH:** Only x86, amd64

I/O from/to ports of the IO ports address space.

``i{1,2,4}xxxx``
    Reads (in) from the specified 1-, 2-, or 4-byte port.

``ipxxxxxxxx``
    Reads (in) from the specified PCI configuration register. It is not
    required to set bit 32 of xxxxxxxx. PCI configuration registers are always
    4-byte registers. Their address ``xxxxxxxx`` must always be 4-byte aligned.

``o{1,2,4}xxxx{yy...}``
    Writes (out) yy... to the specified 1-, 2-, or 4-byte port.

``opxxxxxxxx``
    Writes (out) ``yyyyyyyy`` to the specified PCI configuration register. It is
    not required to set bit 32 of ``xxxxxxxx``. PCI configuration registers are
    always 4-byte registers. Their address ``xxxxxxxx`` must always be 4-byte
    aligned.


.. raw:: pdf

   PageBreak

Dump Backtrace: ``bt`` / ``backtrace``
--------------------------------------

Displays the backtrace of a thread.

**ARCH:** Only x86, amd64

+-----------------+------------------------------------------------------------+
| Cmd             | Description                                                |
+=================+============================================================+
| ``bt``          | Display the user-backtrace of the last thread running      |
|                 | (currently active thread) before entering JDB. Instruction |
|                 | pointers are shown from newest at the top down to oldest.  |
+-----------------+------------------------------------------------------------+
| ``bttxxx``      | Display the user-backtrace of thread xxx. Also shows the   |
|                 | kernel backtrace of the selected thread.                   |
+-----------------+------------------------------------------------------------+
| ``bttxxxxxxxx`` | Display the user-backtrace specifying the low dword of     |
|                 | thread id                                                  |
+-----------------+------------------------------------------------------------+
| ``btxxxxxxxx``  | Display the backtrace of the current thread interpreting   |
|                 | address  xxxxxxxx as framepointer.                         |
+-----------------+------------------------------------------------------------+


.. raw:: pdf

   PageBreak

Breakpoint: ``b`` / ``bp``
--------------------------

Sets/resets four global breakpoints for kernel-mode and user mode.

``bl``
    Displays the breakpoints and breakpoint restrictions

``bi...``
    Sets an instruction breakpoint. b{w,a,p}{1,2,4}... Sets a data-write
    breakpoint (w) or a data-access breakpoint (a) for a 1-, 2- or 4-byte
    variable or sets a breakpoint for in/out to a 1-, 2- or 4-byte i/o port (p)

``...xxxxxxxx xxxxxxxx``
    is the absolute breakpoint address and given as a
    hexadecimal number where leading zeros can be omitted.

``b–[bpn]``
    Resets a system-global breakpoint. The number of the breakpoint ``bpn``
    (1..4) has to be specified

``b+[bpn]``
    «Enter» kernel debugger when breakpoint matched (default action)

``b*[bpn]``
    Don't enter kernel debugger when breakpoint matched, instead create a
    tracebuffer entry

``br[bpn]...``
    Restricts a breakpoint. The breakpoint exception invokes JDB only
    when all set restrictions are met. Otherwise, the breakpoint exception is
    ignored. The number of the breakpoint ``bpn``, which should be restricted
    must be specified.

``... txxx``
   Restricts breakpoints to thread xxx.
``... Txxx``
   Restricts breakpoints to threads !=xxx.
``... axxx``
   Restricts breakpoints to task xxx.
``... Axxx``
   Restricts breakpoints to tasks !=xxx.
``... e[reg] [yyyyyyyy,zzzzzzzz]``
   The specified register ``reg`` must have a value in the specified interval
   (if yyyyyyyy≤zzzzzzzz) or outside the interval [zzzzzzzz,yyyyyyyy]
   (if yyyyyyyy>zzzzzzzz).
``... {1,2,4}xxxxxxxx [yyyyyyyy,zzzzzzzz]``
   The specified 1-, 2- or 4-byte variable must have a value in the specified
   interval (if yyyyyyyy≤zzzzzzzz) or outside the interval [zzzzzzzz,yyyyyyyy]
   (if yyyyyyyy>zzzzzzzz).
``... –``
   All breakpoint restrictions of the breakpoint with number ``bpn`` are reset.

.. Note::
   Breakpoint restrictions are *not* reset when the breakpoint address or type
   are changed or when the breakpoint is reset (``b-``). Breakpoint
   restrictions can be explicitly reset (``br-``). However, changes of the
   restrictions do not require prior reset. ``br{t,T}``, ``br{e..}`` and
   ``br{1,2,4}`` restrictions are logically anded. However, setting a
   restriction overwrites a prior restriction of that type.


.. raw:: pdf

   PageBreak

Single Stepping: ``S`` / ``singlestep``
---------------------------------------

If the single step mode is activated, the processor enters the kernel
debugger after each instruction. 

.. Note:: On ``int 0xx``, the whole system call is executed.

**ARCH:** Only x86, amd64

``S+``
    enables single stepping mode

``S–``
    disables single stepping mode


Halt Thread: ``H`` / ``halt``
-----------------------------

Halt a specific thread. This can be useful for instance if a thread
stays in an endless loop raising page faults or is filling the trace
buffer.

**ARCH:** Only x86, amd64


Special Go: ``j``
-----------------

Resumes execution after/at the instruction which invoked JDB.

**ARCH:** Only x86, amd64

``jb``
    Continue until next branch (``call``, ``ret``, ``jmp``, ``int``, ``iret``
    instruction). This mode works using the single step mode of the processor.

``jr``
    Continue until current function returns (until ``ret`` or ``iret``
    instruction at the current code level). This mode uses the single step mode
    of the processor.

``js``
    Single Step (in difference to ``S+``, we do not enter the single step mode)


.. raw:: pdf

   PageBreak

JDB-related User Level Function Calls
=====================================

Assigning user-defined debug names for objects
----------------------------------------------

As a convenience when debugging some capabilities are by default created
with a name string which is displayed in JDB. Names can also be assigned
to other capabilities in the system as an aid to debugging.

.. code-block:: c++

    #include <l4/sys/debugger.h>

    // Set the name of the current thread to 'worker thread`.
    auto const myself = Atkins::Thread_helper::this_thread_cap();
    l4_debugger_set_object_name(myself.cap(), "worker thread");

    // Determine the kernel debug ID of the current thread object.
    unsigned long id = l4_debugger_global_id(myself.cap());
    printf("Debugger ID of current thread is %lx.\n", id);


Adding Trace Buffer Events from User Level
------------------------------------------

Trace buffer events can be added from a user level program using the
following functions.

User level trace events:

.. code-block:: c++

    #include <l4/sys/kdebug.h>

    l4_kd_enter("*«text»");

This creates a trace buffer entry containing the string «text».

.. code-block:: c++

    #include <l4/sys/ktrace.h>

    fiasco_tbuf_log("«text»");

Creates a trace buffer entry containing the string «text». In contrast to using
the sequence ``l4_kd_enter("*«text»")``, the string «text» may be created at
runtime.

.. code-block:: c++

    fiasco_tbuf_log_3val("«text»", val1, val2, val3);

The string «text» and three hexadecimal values are stored into a single trace
buffer entry.

.. code-block:: c++

    fiasco_tbuf_log_binary(unsigned char const *data);

Creates a trace buffer entry containing the 24 bytes of the binary ``data``.


.. raw:: pdf

   PageBreak

Trace Buffer Control from User Level
------------------------------------

Some trace buffer control commands are callable from a user level program.
These can be used to clear and dump the trace buffer and obtain counts of
system events (e.g. context switches, page faults, irqs, etc).

Note that the ``Jdb`` capability is required by the application to perform such
system calls.

.. code-block:: c++

   #include <l4/sys/ktrace.h>

   l4_msgtag_t ret = fiasco_tbuf_validate();
   if (!ret.label())
     {
       printf("Tracebuffer JDB capability not valid!\n");
       exit(1);
     }

    /* Clear the trace buffer */
    fiasco_tbuf_clear();

    /* Dump the tracebuffer uuencoded to the console */
    fiasco_tbuf_dump();


It is also possible to activate certain tracebuffer events from userland. To
list all available events, use the JDB ``O`` command.

.. code-block:: c++

   #include <l4/sys/debugger>

   // Enable logging of raised exceptions.
   l4_debugger_switch_log(L4_BASE_DEBUGGER_CAP, "Exceptions", 1);

   // Enable logging of triggered IRQ objects.
   l4_debugger_switch_log(L4_BASE_DEBUGGER_CAP, "IRQ-Object triggers", 1);


.. raw:: pdf

   PageBreak

Entering JDB from User Level
----------------------------

A user program can add calls to the ``l4_kd_enter()`` function to go into
the kernel debugger at that point in the execution of the program. This
can be useful for examining system state at certain points.

.. code-block:: c++

       #include <l4/sys/kdebug.h>

       l4_kd_enter("«text»");


Enters the kernel debugger with entry reason string «text».

.. Note:: JDB handles ``text`` distinctly from ``*text``.

Remote Control from User Level
------------------------------

Some non-interactive JDB commands may be executed from a user level program by
using ``l4_kd_enter()`` and providing a control sequence starting with ``"*#"``.

.. code-block:: c++

       #include <l4/sys/kdebug.h>

       l4_kd_enter("*#");

For example, to log the IPC traffic for a specific code sequence use:

.. code-block:: c++

       /* start IPC logging, including IPC results */
       l4_kd_enter("*#I*IR+");
       ...
       /* IPC here */
       ...
       /* stop IPC logging */
       l4_kd_enter("*#I-");


.. raw:: pdf

   PageBreak

JDB-related Command Line Switches
=================================

The following parameters can be attached to the kernel command line. Bootstrap
will parse these parameters and pass them to the kernel:

+---------------------+---------------------------------------------------------+
| Parameter           | Description                                             |
+=====================+=========================================================+
| ``-wait``           | Emit a debug trap after the kernel has been initialised |
|                     | and before user programs are started. This will usually |
|                     | enter the kernel debugger and show the debugger prompt. |
+---------------------+---------------------------------------------------------+
| ``-nojdb``          | Disables the builtin kernel debugger JDB.               |
+---------------------+---------------------------------------------------------+
| ``-noscreen``       | Disables output to VGA/Hercules console.                |
+---------------------+---------------------------------------------------------+
| ``-noserial``       | Disables output to serial console. If this switch is not|
|                     | given, kernel messages will output additionally to the  |
|                     | serial interface. If ``-nokdb`` is enabled, you can use |
|                     | a terminal program on the host to control Fiasco. If    |
|                     | you are connected to a remote GDB, messages are copied  |
|                     | to GDB’s console.                                       |
+---------------------+---------------------------------------------------------+
| ``-nohlt``          | Prevents the kernel from using the ``hlt`` instruction  |
|                     | while being idle.                                       |
|                     |                                                         |
|                     | **ARCH:** Only x86, amd64                               |
+---------------------+---------------------------------------------------------+
| ``-comspeed=n``     | Will set the rate of the serial interface to n          |
|                     | bits/second. 115200 baud is the default.                |
+---------------------+---------------------------------------------------------+
| ``-comport=n``      | Will use COMn for serial communication. COM1 is the     |
|                     | default. Possible values for n are 1, 2, 3, and 4. n is |
|                     | interpreted as an I/O port if n>4.                      |
+---------------------+---------------------------------------------------------+
| ``-esc``            | Enable esc hack. On every timer interrupt, ask the      |
|                     | keyboard if the Escape key was pressed. If so, do enter |
|                     | into kernel debugger. Applications (e.g. L4Linux) may be|
|                     | confused by dropped key events so better use            |
|                     | ``-serial_esc``.                                        |
+---------------------+---------------------------------------------------------+
| ``-kmemsize=n``     | Overwrite Fiasco’s heuristic for required kernel memory.|
|                     | Set the memory reserved for mapping trees, TCBs, and    |
|                     | other to ‘n’ MB.                                        |
+---------------------+---------------------------------------------------------+
| ``-watchdog``       | Enable watchdog. On every timer interrupt, tell the     |
|                     | watchdog that we are sill alive. If a task disables the |
|                     | interrupts and loops, the timer interrupt isn't called  |
|                     | anymore and after 2 seconds the watchdog releases an non|
|                     | maskable interrupt (NMI) which forces Fiasco to step    |
|                     | into the kernel debugger.                               |
+---------------------+---------------------------------------------------------+
| ``-loadcnt``        | Initialize a performance counter for counting all cycles|
|                     | the CPU is not halted. The counter is accessible from   |
|                     | userland via ``rdpmc(x)`` where ``x`` is 0 on P6/K7,    |
|                     | and P4. **TODO** rewrite OBSOLETE                       |
+---------------------+---------------------------------------------------------+
| ``-apic``           | Initialize the builtin Local APIC. If the Local APIC is |
|                     | disabled by the BIOS but available it is re-enabled.    |
+---------------------+---------------------------------------------------------+
| ``-serial_input``   | Enable the kernel to receive interrupts from the UART.  |
|                     | This is required to provide input on the kernel vlog    |
|                     | interface.                                              |
+---------------------+---------------------------------------------------------+
| ``-serial_esc``     | Enter JDB on serial receive interrupts. This is only    |
|                     | necessary if kdb was disabled by ``-nokdb``. Includes   |
|                     | ``-serial_input``.                                      |
+---------------------+---------------------------------------------------------+
| ``-outbuf=n``       | Set output buffer for ``l4_kd`` display functions to    |
|                     | n bytes. Default is 8192.                               |
+---------------------+---------------------------------------------------------+
| ``-tbuf_entries=n`` | Set number of lines to store in the debugging trace     |
|                     | buffer. Default is 16384.                               |
+---------------------+---------------------------------------------------------+
| ``-jdb_cmd=cmds``   | Execute JDB commands non-interactive at startup.        |
|                     | Example: use ``-jdb_cmd=I*IR+P*PR+`` to log all IPCs and|
|                     | page faults from startup.                               |
+---------------------+---------------------------------------------------------+


.. raw:: pdf

   PageBreak

Credits
=======

**Original Revision History**

- Derived from LN KDB manual written by Jochen Liedtke.
- Released as JDB manual Version 1.00 by Jan Glauber (2001), Technische
  Universität Dresden, Department of Computer Science.
- Updates by Frank Mehnert, Adam Lackorzynski, and Yann Le Du.
