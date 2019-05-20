# JDB User Manual - Fiasco Kernel Debugger

### Updated: May 21, 2019

## CONTENTS

1. General Remarks
   1. About JDB
   1. About Exception Handling
   1. Navigation and Abbreviations
   1. Status line
1. General Commands
   1. Enter JDB: Esc
   1. Go: g
   1. Help: h
   1. Command mode: *|mode
   1. Debug Message: Return
   1. JDB options: J
   1. Screen Output Buffer: B
1. Information about Kernel State
   1. System Report: Y.
   1. Kernel Information: k
   1. List Kernel Objects: Q
   1. Task list: s
   1. Thread Lists Ready|Present: l.
   1. Timeout List: lt
   1. Thread Control Block: t
   1. Display Thread ID of TCB address: t?.
   1. List IRQ threads: R.
   1. Show UTCB and vCPU State: z.
   1. Show Kernel Object: K.
   1. Mapping: m.
   1. Dump Page Tables: p.
1. Monitoring Kernel Events
   1. Monitor Extended Logging Events: O
   1. IPC Monitoring: I.
   1. Page Fault Monitoring: P.
   1. Trace Buffer: T.
      1. Key Description
      1. Kernel Tracing Events
      1. User Level Tracing Events
      1. Performance Monitoring
   1. Kernel Event Counters: C
1. Debugging and Architecture-Specific Commands
   1. Reboot: ^
   1. Dump Memory: d.
   1. Disassemble Memory: u
   1. Physical memory: A.
   1. Machine Specific Registers: M.
   1. Performance Monitor Events: M
   1. I/O Bitmap: r
   1. Port I/O: i/o
   1. Dump TLB Entries: x
   1. Dump Backtrace: bt
   1. Breakpoint: b.
   1. Single Stepping: S
   1. Halt Thread: H
   1. Special Go: j
1. JDB-related User Level Function Calls
   1. Assigning user-defined debug names for objects
   1. Adding Trace Buffer Events from User Level
   1. Trace Buffer Control from User Level
   1. Enterring JDB from User Level
   1. Remote Control from User Level
1. JDB-related Command Line Switches
1. Credits
   1. Original Revision History

## General Remarks

### About JDB

The Fiasco Kernel Debugger (JDB) is primarily a tool to test Fiasco and to fix
bugs in Fiasco. However it is also useful for testing higher layers.
Consequently,

- JDB always freezes the entire machine state when invoked. All interrupts are
    disabled, even the clock is halted. No kernel or user process proceeds
    while JDB is active.
- JDB is a stand-alone debugger. It neither use parts of Fiasco nor other
    device drivers. JDB includes simple device drivers for keyboard, display,
    and the serial interfaces. These device drivers are *not* interrupt driven.

Although developed and traditionally packaged together with Fiasco, the Fiasco
Kernel Debugger is *not* part of the Fiasco μ-kernel. Fiasco can run without
JDB. The only connection between Fiasco and JDB is that JDB intimately knows
Fiasco's data structures.

### About Exception Handling

JDB is invoked by exceptions, the particular exception is necessarily
architecture-specific. Some exceptions are handled normally by Fiasco, for
example a page fault. All other exceptions are distributed by Fiasco either to
JDB or to the current user-level thread. The corresponding algorithm is:

exception dispatch:
if exception occurred in kernel
  then invoke JDB with kernel error
elif current thread defined an exception handler
  then invoke user-level exception handler by upcall
else
  invoke JDB with user error
fi.

Summarizing: JDB is used for kernel errors. It is used for user errors only as
long as no debugger is installed at user level.

### Navigation and Abbreviations

Some key bindings correspond to different actions based on context and based on
object type. Below are some common bindings.

```
Esc (Escape)              = enter JDB (from running program)
Esc (Escape)              = return to top level context (from within JDB)
Home                      = go up/back one level (in many contexts)
Enter                     = select (depends on context)
CR (Carriage Return)      = select (depends on context)
PgUp, PgDn, Home, End     = navigate page left, down, up, right
H, J, K, L                = navigate page left, down, up, right
←, ↑, ↑, →  (cursor keys) = navigate left, down, up, right
h, j, k, l  (cursor keys) = navigate left, down, up, right
```

Some common abbreviations for object listing are shown here.

```
R = Reference count
C = Capability
C = CPU
D = Debug IDentifier
DID = Debug IDentifier
```

### Status line

In many commands the bottom of the screen shows a status bar suggesting further
context-specific commands.

## General Commands

### Enter JDB: Esc

Enter the kernel debugger from a running program. Requires that the kernel be
built with the CONFIG_JDB option.

To enable enterring JDB using the *Escape* key refer to the flags -serial_esc
and -esc.

For another way of enterring JDB refer to enter_kdebug().


### Go: g

Exits JDB and resumes execution after/at the instruction which invoked JDB.

### Help: h

Shows a screen with a short description of JDB commands.

### Command mode: *|mode

Switch between long and short command mode.

### Debug Message: Return

Shows the CPU, instruction pointer and trap type for the entry into JDB.

### JDB options: J

```
JS Use full screen for JDB. Adapts the height of the screen to the height of
the terminal window by detecting its current size using escape sequences. Some
terminals may not support the necessary escape sequence and can be set using
'Jh' and 'Jw'.
```
```
Jh Set the JDB screen height. Also used to revert from full screen.
```
```
Jw Set the JDB screen width.
```
```
Jc Set the color of the debug prompt.
```
```
Jd-/Jd+ Disable/enable direct console.
```

### Screen Output Buffer: B

The output buffer stores all output written to the Fiasco kernel debugger
console.

```
B Show complete output buffer.
```
```
Bn Show last n lines of output buffer.
```

## Information about Kernel State

### System Report: Y.

Generate a system report or 'Yc' for a compressed system report.

This generates a screen dump equivalent to running the following JDB commands:
- List Kernel Objects
- List Threads
- List Timeouts
- show Kernel Information and subcommands
- Trace Buffer,
- Console Output
- show Thread Control Block for all threads.

```
Y{c}|report <compressed> system report
```

### Kernel Information: k

Displays some global Fiasco kernel information. Some common subcommands are
included here.

ARCH: The list of avaliable subcommands is architecture-specific.

```
kh Short help about available subcommands.
```
```
kf Interprets the content of the kernel info page.
```
```
ki Miscellaneous kernel info.
```
```
km Kernel memory footprint.
```

### List Kernel Objects: Q

List kernel objects. Show information and explore the relationships between
different objects. For each object the listing shows the debug ID, object
pointer, object type, and debug name string (if available). The remainder
of the line shows object-specific information.

Typical object information:

```
DID object_address [object_type] {debug_name} ... object-specific information
```

The status line shows possibilities for drilling down deeper and also shows
object-specific keys.

```
Enter Display further information about some object types.
Space Cycle through different sublists of object types.
Tab Link to related or parent objects.
/ Filter the displayed information using a regular expression.
```

The following shows some examples of object-specific information:

```
... object-specific information
D= Debug identifier (DID)
C= CPU thread is running on
R= Object Reference count
S=D:xx DID of task providing dataspace
L= Gate or IRQ internal object ID
I= IRQ pin (0 if not applicable)
Soft|Hard = IRQ chip type
F= IRQ flags
T= DID of thread triggered by IRQ
Q= 0|1 is queued or not

TODO what does 'Q' show for factory; what are <current> and <limit>?

[Factory] c=<current> l=<limit>
[Task   ] R=n
[Thread ] C=n R=n <thread state> <cur:current thread>
[Thread ] C=n S=D:<dataspace DID> R=n
[Gate   ] L=<object ID> D=<bound thread DID>
[IRQ ipc] I=<pin> Soft|Hard F=<flags> L=<object ID> T=<DID> Q=<queued?>
[IRQ sem] I=0 Soft F=<flags> Q=<queued?>
```

### Task list: s

Show task list. For each object the listing shows the debug ID, object pointer,
object type, debug name string (if available) and reference count.

The status line shows possibilities for drilling down deeper.

```
Enter Display task UTCB addresses and memory usage.
o=objspace p=ptab m=mappings Drill down for object space, page table and
mapping database.
```

### Thread Lists Ready|Present: l.

Display the *present list* ('lp' command) or the *ready list* ('lr' command).

For each thread the following information is displayed: debug ID, current CPU,
debug name (if available), priority, debug ID of the dataspace provider, debug
ID of thread being waited on (if any), timeout, thread state.

```
lp Display list of thread present.
lr Display list of threads with state set to ready to run.
```
```
Cursor keys Scroll in the list.
Enter Show the TCB of the selected thread.
Home Navigate back to the list.
Space Cycle through the sort mode: Unsorted, sorted by thread priority, sorted
by thread identifier, and sorted by dataspace.
Tab Switches to the thread in the list that the current thread is waiting for.
```

Note that the thread name prefixed with '#' refers to the L4Re thread typically
serving as default page fault handler, exception handler, and region manager
for other threads in the system. For example:

```
19 810902f8 [Task   ] {hello           } R=3
1a 80f480f8 [Thread ] {#hello          } C=0 S=D:19 R=1
1d 80f4a0f8 [Thread ] {hello           } C=0 S=D:19 R=1
```

### Timeout List: lt

Display all current active timeouts.

```
Cursor keys Scroll in the list.
Enter Show the TCB of the owner of the selected timeout.
```

### Thread Control Block: t

Displays the commented thread control block (TCB), the thread's kernel stack,
and the thread's general purpose registers.

```
t Displays the thread control block of the current thread.
```
```
txxx Displays the thread control block of thread xxx. xxx is the global
debugger ID and is hexadecimal.
```
```
t+/– Enable/disable automatic displaying of current thread control block
when entering kernel debugger.
```
```
thread control block The fields of the thread control block (TCB) are labeled.
```
```
kernel stack The kernel stack of the displayed thread is dumped from the
current stack top (first line) down to the stack bottom. Use the cursor keys to
scroll inside the TCB borders of the displayed thread. Press the *Enter* key
to dump the address at the cursor in memory mode. Press the *Space* key to
disassemble the address at the cursor (requires the kernel to be configured
with CONFIG_JDB_DISASM).
```
```
When entering kernel mode the processor first pushes an architecture-specific
trap frame onto the kernel stack. The stack pointer, instruction pointer,
a subset of registers and flags are displayed on the bottom line.
```
```
registers are displayed if the *currently active* thread is displayed; this is
the last thread running before entering JDB. For this thread JDB gets the
registers from the trap frame on the stack. Depending on their state other
threads might not have saved all their registers on the stack.
```

#### Thread Control Block Detail

The thread control block display contains very detailed information. Some of
the key fields are annotated here. The details are architecture specific and
may change in the future however this should serve as a useful starting point.

```
1  thread  :  20 <0xfcd5f000>      CPU: 0:0        prio: 02
2  state   : 8008 rcv_wait,fpu
3  wait for:   20  polling:        rcv descr: 00000000
4  lcked by:                       timeout  :
5  cpu time:  67.000 ms            timeslice: 10000/18446744073709551615 us
6  pager   : [C:   3] D:  1d       task     : D:  1c
7  exc-hndl: [C:   3] D:  1d       UTCB     : fcd56200/b3000200
8  vCPU    : ---
9  vCPU    : ---
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
```

```
line 1  thread: DID <object_address> CPU: current_cpu:home_cpu priority
```

```
line 2  thread state: bit_mask bit_state_decode

These are some common thread states:

ready             - can be scheduled
send              - waiting to send
rcv_wait          - waiting for a message
dead              - inactive (not in any q)
suspended         - must not execute user code
migrate           - must finish migration
resched           - needs rescheduling
fpu               - fpu owner
alien             - not allowed to do system call
dealien           - alien allowed to do next system call only
exc_progr         - exception in progress
lock_wait         - wating for a lock
vcpu              - vcpu is enabled for thread
vcpu_user         - is a vcpu user thread
vcpu_fpu_disabled - fpu disabled in vcpu
vcpu_ext          - extended vcpu thread (guest extentions)
```

```
line 3
wait for: DID of thread being waited on, self = sleep
polling: DID of send partner, waiting for receiver while inside ipc call
```
```
line 4  lcked: not used, timeout: wait duration of current timeout
line 5  cpu time spent in thread
```
```
line 6
pager: C: capability slot of the pager and DID
task: DID of parent task
```
```
line 7
exc-hndl: C: capability slot of thread's exception handler and DID
UTCB : kernel_address/user_address
```
```
line 8 VCPU:
line 9 VCPU:
if vCPU not present: ---
if vCPU present:

pointer to vCPU state: kernel_address/user_address
  S= DID of guest address space
  c= vcpu current state
  s= saved vcpu state
  sf= sticky flags
    e.g. c=FuDEpi s=FuDEpi sf=-
    e.g. upper case is set, lower case is clear, '-' if none
  e-ip=
  e-sp=
  host exit handler: entry instruction pointer and entry stack pointer
line 10
```
```
line 11 registers, current instruction pointer (disassembled if avail.)
line 12 registers, next instruction (disassembled if avail.)
line 13 registers
line 14 registers
line 15 information line
line 16 Architecture-specific subset of registers
line 17 This is the TCB and kernel stack, e.g. thread object starting at the
current stack pointer.
line 18 kernel stack
line 19 kernel stack
line 20 kernel stack

kernel stack continues below ...
```

### Display Thread ID of TCB address: t?.

Used to find the thread ID corresponding to a TCB address. The command prompts
for a TCB address (e.g. from within a thread's TCB dump).

### List IRQ threads: R.

```
Rl list all IRQ threads
```
```
Ra attach specific IRQ to Jdb
```
TODO What does it mean to attach an IRQ to JDB? what is the usecase for 'Ra'?

### Show UTCB and vCPU State: z.

This command dumps the address and the contents of the UTCB of a thread and
also the vCPU state if applicable for the thread.

```
z Display the UTCB of the current thread
```
```
zxxx Display the UTCB of thread with debug ID xxx.
```
```
zP<kobj_ptr> Display the UTCB of thread with kernel object address <kobj_ptr>
```

### Show Kernel Object: K.

Show information about the specified kernel object.

TODO 'K.' this doesn't seem very useful, it just shows something like
0x400a5c40 [type=const char* cxx::_dyn::name_of() [with T = Factory]]
or for task it does the same as s + Enter, and for thread the same as t.

```
K<kobj_ptr> show information for kernel object
```

### Mapping: m.

Parse the Fiasco mapping database.

```
mxxxxx Starts parsing the mapping subtree corresponding to physical page
frame xxxxx000. The *CursorUp* and *CursorDown* keys can be used to
proceed to the next or previous page.
```

### Dump Page Tables: p.

Displays page tables and virtual memory.

```
p displays the higher-level page table (page directory) of the current address
space.

The cursor keys, *PgUp*, *PgDn* and *Home* can be used to move the dump cursor
and display further memory. Pressing the *Enter* key when the cursor points to
a valid lower-level page table switches to this table. In the same way, the
*Enter* key there switches to the data page. The *Home* key always “returns” to
the lower- or higher-level page table, respectively.
```
```
pxxx displays the higher-level page table of task 'xxx'.
```
```
The *Space* key changes between page table mode and raw mode.
```
```
Page-table entry formats:
```
- Nil entry.
xxxx--r User-level, read-only, pointing to physical address 0xxxx000.
xxxx--w User-level, read/write, pointing to physical address 0xxxx000.
xxxx--R Kernel, read-only, pointing to physical address 0xxxx000.
xxxx--W Kernel, read/write, pointing to physical address 0xxxx000.
xx/4--{r,w,R,W} 4M-page entry, pointing to physical address 0xx×4MB.

## Monitoring Kernel Events

### Monitor Extended Logging Events: O

Select from a list of extended logging events which can be activated or
deactivated for monitoring. For a complete list of available events type 'O'
without any argument.

The JDB extended logging feature can be completely disabled at compile time and
must be enabled using the configuration option *JDB_LOGGING*.

```
Press the *Enter* key to toggle individual events and navigate using the
following:

↑, ↓, j, k, PgUp, PgDn, Home, End
```

Unlike the 'P' command and the 'I' command, enabling the extended logging
feature adds a small additional overhead at runtime (and even smaller when
events are individually deactivated).

### IPC Monitoring: I.

Monitors IPC operations. IPCs are monitored *before* they are handled by
Fiasco.

Some abbreviations used in ipc log events are shown here.

```
ipc => C:capability DID:debug identifier L:internal object ID TO:timeout
```
TODO improve description of what the ipc message fields are
```
I+ switches *single* IPC monitoring on. Whenever an IPC operation is invoked the
following information is displayed:
message type, current thread debug ID, operation type (call, send, wait, wait
for, reply and wait), [raw capability used for destination], destination thread
debug ID (if specified), [protocol ID ORed with the message flags], message
words 0 and 1 and the instruction pointer are displayed. The system stops until
a key is hit on the debug console. Pressing the 'i' key invokes the full JDB
menu; any other key resumes normal operation, i.e. starts normal IPC handling.
```
```
I– switches IPC monitoring off (both *single* and *traced*).
```
```
I* switches *traced* IPC monitoring on. Instead of presenting any single IPC,
all IPCs are monitored in a trace buffer. When JDB is next enterred the entries
can be displayed using the *dump trace* command 'T'.
```
```
IR+ switches monitoring of IPC system call results on. To log these events,
IPC logging must be enabled either by 'I+' or by 'I*'.
```
```
IR– switches monitoring of IPC system call results off.
```
```
IT+ Use special log format containing both IPC data and IPC result. The
format is used together with the L4 userland tracing library.
```
TODO what is this special log format for tracing 'IT+'?
TODO what is this reference to L4 userland tracing library?
```
IT– Switch back to normal IPC log format.
```
```
Ir... restricts the monitored IPCs. IPCs that do not meet all specified
restrictions are ignored by the monitoring system.
```
```
... txxx restricts IPCs to thread xxx.
... Txxx restricts IPCs to threads !=xxx.
... axxx restricts IPCs to task xxx.
... Axxx restricts IPCs to tasks !=xxx.
... s Only IPCs containing a send part are monitored, i.e. call, send, reply
    and wait.
... – All IPC restrictions are reset.
```

### Page Fault Monitoring: P.

Monitors page faults. Page faults are monitored *before* they are handled by
Fiasco.

```
P+ Switches *single* page fault monitoring on. Whenever a page fault occurs,
page fault address, instruction pointer and thread number are displayed and
the system stops until a key is hit on the debug console. Pressing the
'i' key invokes the full JDB menu; any other key resumes normal operation, i.e.
starts normal page fault handling.
```
```
P– Switches page fault monitoring off (both *single* and *traced*).
```
```
P* Switches *traced* page fault monitoring on. Instead of presenting any single
page fault all page faults are monitored in a trace buffer. When JDB is invoked
the next time, the entries can be displayed by the *dump trace* command 'T'.
```
```
PR+ Enables monitoring of the return code of the page fault handler. To log
these events, PF logging must be enabled either by 'P+' or by 'P*'.
```
```
PR– Disables monitoring of the page fault handler return code.
```
```
Pr... Restricts the monitored page fault. Page faults that do not meet all
specified restrictions are ignored by the monitoring system.
```
```
... txxx Restricts page faults to thread xxx
... Txxx Restricts page faults to threads !=xxx.
... x[yyyyyyyy,zzzzzzzz] Only page faults with fault addresses inside the
    interval [yyyyyyyy,zzzzzzzz] (if yyyyyyyy≤zzzzzzzz) or outside the interval
    [zzzzzzzz,yyyyyyyy] (if yyyyyyyy>zzzzzzzz) are monitored.
... – All page fault restrictions are reset.
```

### Trace Buffer: T.

Shows the trace buffer. Trace entry messages are stored in a trace buffer
together with timing information and up to two performance counter values. The
trace buffer size, usage percentage and performance counter information is
shown at the top of the screen. The buffer size can be modified using the
'-tbuf_entries=' command line switch.

ARCH: Timestamp resolution is architecture-specific

```
T Enters trace buffer dump. The newest information is displayed at the
top. Trace buffer entries get timestamped when they are entered into the
buffer. Accordingly, they can be displayed in various timing modes.
```
```
index Entries are shown with their sequence number in the buffer.
```
```
tsc diff Entries are shown with their time difference (in 40 bit hexadecimal
notation or μs, ms, or s – switchable by *Space* key) to their displayed
predecessor entry.
```
```
tsc rel Entries are shown with their time difference relative to the reference
element.
```
```
tsc start Entries are shown with their time difference relative to the system
start.
```
```
kclock rel The kernel clock with their difference relative to the reference
element is shown.
```
```
kclock The absolute kernel clock is shown.
```
```
pmc1 diff Entries are shown with the differences of their first performance
counter.
```
```
pmc2 diff Entries are shown with the differences of their second performance
counter.
```
```
pmc1 rel Entries are shown with the difference of their first performance
counter relative to the reference element.
```
```
pmc2 rel Entries are shown with the difference of their second performance
counter relative to the reference element.
```

#### Key Description

```
←, →  Cycle between displaying the event index and the different timing modes.
```
TODO check that the current implementation matches the difference clock mode and clock representations documented here.
TODO improve explanation of the different timer representations for Space key
TODO why does 'c rel' show the letter 'c' after the time value?
```
Space Cycle between displaying raw timestamp counter values and hexadecimal
timer representation.
```
```
h, l Scroll horizontally.
```
```
↑, ↓, j, k, PgUp, PgDn, Home, End Navigate within trace buffer.
```
```
c Clear the trace buffer.
```
```
Enter The code the event occured at is disassembled. Requires the kernel to be
configured with CONFIG_JDB_DISASM otherwise only the instruction pointer is
shown. The *Home* key then returns to the trace buffer dump.
```
```
r Set the *reference entry* used when displaying relative time/counter values.
```
```
jr Jump to the reference element.
```
```
s0-9 Set mark0 .. mark9. Marks are highlighted.
```
```
j0-9 Jump to mark0 .. mark9.
```
```
/,?,n The trace buffer can be searched for a regular expression (similar to
*less*).
```
```
F Filter the tracebuffer view by specifying a regular expression.
```
```
Tab Toggle the displaying thread names on/off.
```
TODO what does D=dump do?
TODO can we replace the seldom used D= and P= with hints for left/right arrow, space and/or tab?

#### Kernel Tracing Events

Kernel events:

- IPCs and IPC results (see 'I*' command),
- page faults and page fault results (see 'P*' command),
- other logging events (see 'O' command),
- break points (see 'b*' command)

#### User Level Tracing Events

User level programs can add events to the trace buffer and perform a few simple
control operations such as trace buffer clear and dump.

See the JDB user level function call section in this document. See also
enter_kdebug() and fiasco_tbuf_log().

#### Performance Monitoring

The following options are only accessible while the trace buffer dump mode is
active and if the CPU supports performance counters.

ARCH: Only x86, amd64

TODO which ARCHs support perf counters? maybe arm has basic support

```
P Used to activate, deactivate and change performance monitoring while you are
in the trace buffer dump. (This command does not work outside the trace buffer
dump. Performance monitoring is not available on 486 processors. P5, P6, and K7
processors have two counters. Not all events are available for each counter.
```
```
P[num]+ Turns performance monitoring for counter 'num' on (kernel and user-mode
activities)
```
```
P[num]– Turns performance monitoring for counter 'num' off
```
```
P[num]u Turns performance monitoring on (only user-mode activities)
```
```
P[num]k Turns performance monitoring on (only kernel-mode activities)
```
```
P[num]e Count on edge. This measures not only the fraction of time spent in a
particular state, but also the average length of time spent in such a state
(for example, the time spent waiting for an interrupt to be served). This
option makes only sense with selected events (see hardware documentation).
```
```
P[num]d Count on duration
```
```
P[num]? Select a performance monitoring event from list for counter 'num'.
```

### Kernel Event Counters: C

Kernel event counters are a low-overhead form of tracing and must first be
enabled using the configuration option *CONFIG_JDB_ACCOUNTING*. There are
counters for IPCs, context switches, page faults, and other events.

```
Cl Show kernel event counters.
```
```
Cr Reset kernel event counters.
```

## Debugging and Architecture-Specific Commands

### Reboot: ^

The '^' key (european keyboard: Shift-6) resets the machine.

### Dump Memory: d.

Displays physical and virtual memory.

```
dxxxxxxxx displays memory beginning from address xxxxxxxx. The dump is
32-bit-word oriented so that addresses are always truncated to 4-byte aligned
addresses.

The cursor keys, *PgUp*, *PgDn* and *Home* can be used to move the dump cursor
and display further memory. The *Enter* key can be used to jump to the address
at the cursor, the *Home* key returns if possible. This command always displays
virtual memory of the current address space. Pages that are not mapped to
physical memory are shown as .........
```

```
dtyyy xxxxxxxx displays virtual memory of task 'yyy' beginning from address
xxxxxxxx.
```
```
dtyyyyyyyy xxxxxxxx displays virtual memory of a task specifying the low dword
of the thread id beginning from address xxxxxxxx.
```
```
Memory can be displayed in various modes. The *Space* key switches
between these modes:
```
```
d-mode 32-bit words are shown as dwords, uppermost nibble leftmost, e.g.
00000002 for the value 2. Values 0 and FFFFFFFF are displayed specially: 0 and
-1.
```
```
b-mode 32-bit words are shown bytewise, uppermost byte rightmost, e.g.
02000000 for the value 2. Values 0 and FFFFFFFF are not treated
differently.
```
```
c-mode Bytes are shown as ASCII characters. Unprintable characters are shown
as.
```
The view offers several keys to change the view:

```
Space Changes display modes between 'd', 'b' and 'c' mode, see above.
```
```
Return Goes to the address under the cursor.
```
```
Tab Whether to show contents of adapter memory or not. Reading adapter
memory may cause unwanted behaviour. Default is not to show adapter
memory.
```
```
e Memory values can be edited. When pressing the key, a new value for
the address where the cursor points to can be entered.
```
```
c Pressing the key highlights all values which are up to 0x100000 below
or above the current value. Only works in d-mode.
```
```
u Changes into disassembler view with the value under the cursor preselected as
the address as well as the current task (requires kernel to be configured with
CONFIG_JDB_DISASM).
```

### Disassemble Memory: u

Disassembles virtual memory. Requires the kernel to be configured with
CONFIG_JDB_DISASM.

```
uxxxxxxxx disassembles memory beginning from address xxxxxxxx. The cursor keys,
*PgUp* and *PgDn* can be used to move one line backwards or forwards or to jump
one page backwards or forwards, respectively. This command always disassembles
virtual memory of the current address space. Pages that are not mapped to
physical memory are shown as .........
```
```
utyyy xxxxxxxx disassembles virtual memory of task yyy beginning from address
xxxxxxxx.
```
```
utyyyyyyyy xxxxxxxx disassembles virtual memory of a task specifying the low
dword of the thread id beginning from address xxxxxxxx.
```

### Physical memory: A.

Display or modify physical memory.

ARCH: Only x86, amd64

```
Arxxxxxxxx Read 32 bits from physical address xxxxxxxx and display the result
```
```
Awxxxxxxxx yyyyyyyy write 32 bits contained in yyyyyyyy to address xxxxxxxx
```

### Machine Specific Registers: M.

Display and modify machine status registers.

ARCH: Only x86, amd64

```
Mrxxxxxxxx read 32 bits from the machine specific register xxxxxxxx and display
the result
```
```
Mwxxxxxxxx yyyyyyyy write 32 bits yyyyyyyy to the machine specific register
xxxxxxxx
```

### Performance Monitor Events: M

Display performance monitor cycle and event counters.

ARCH: Only arm, aarch64
NOTE: Mutually exclusive with 'M.' command for x86, amd64

### I/O Bitmap: r

Displays mapped ports, gives information about pages mapped for the I/O bitmap,
and shows the I/O port counter (used for determining privileged tasks).

ARCH: Only x86, amd64

```
r display the I/O bitmap for the current task.
```
```
rxxx display the I/O bitmap for task xxx.
```

### Port I/O: i/o

ARCH: Only x86, amd64

I/O from/to ports of the IO ports address space.

```
i{1,2,4}xxxx Reads (in) from the specified 1-, 2-, or 4-byte port.
```
```
ipxxxxxxxx Reads (in) from the specified PCI configuration register. It is not
required to set bit 32 of xxxxxxxx. PCI configuaration registers are always
4-byte registers. Their address 'xxxxxxxx' must always be 4-byte aligned.
```
```
o{1,2,4}xxxx{yy...} Writes (out) yy... to the specified 1-, 2-, or 4-byte port.
```
```
opxxxxxxxx Writes (out) yyyyyyyy to the specified PCI configuration register.
It is not required to set bit 32 of xxxxxxxx. PCI configuaration registers are
always 4-byte registers. Their address 'xxxxxxxx' must always be 4-byte
aligned.
```

### Dump TLB Entries: x

Dump the TLB entries.

ARCH: Only mips
TODO mips dump TLB doesn't work? need compiler switch?

### Dump Backtrace: bt

Displays the backtrace of a thread.

ARCH: Not all architectures.

TODO which architectures actually support it?

```
bt Display the user-backtrace of the last thread running (currently active
thread) before entering JDB. Instruction pointers are shown from newest
at the top down to oldest.
```
```
bttxxx Display the user-backtrace of thread xxx. Also shows the kernel
backtrace of the selected thread.
```
```
bttxxxxxxxx Display the user-backtrace specifying the low dword of thread id
```
```
btxxxxxxxx Display the backtrace of the current thread interpreting address
xxxxxxxx as framepointer.
```

### Breakpoint: b.

Sets/resets four global breakpoints for kernel-mode and user mode.

```
bl Displays the breakpoints and breakpoint restrictions
```
```
bi... Sets an instruction breakpoint.
b{w,a,p}{1,2,4}... Sets a data-write breakpoint (w) or a data-access breakpoint
(a) for a 1-, 2- or 4-byte variable or sets a breakpoint for in/out to a 1-, 2-
or 4-byte i/o port (p)
```
```
...xxxxxxxx xxxxxxxx is the absolute breakpoint address and given as a
hexadecimal number where leading zeros can be omitted.
```

```
b–[bpn] Resets a system-global breakpoint. The number of the breakpoint 'bpn'
(1..4) has to be specified
```
```
b+[bpn] Enter kernel debugger when breakpoint matched (default action)
```
```
b*[bpn] Don't enter kernel debugger when breakpoint matched, instead create a
tracebuffer entry
```
```
br[bpn]... Restricts a breakpoint. The breakpoint exception invokes JDB only
when all set restrictions are met. Otherwise, the breakpoint exception is
ignored. The number of the breakpoint 'bpn', which should be restricted
must be specified.
```
```
... txxx Restricts breakpoints to thread xxx.
... Txxx Restricts breakpoints to threads !=xxx.
... axxx Restricts breakpoints to task xxx.
... Axxx Restricts breakpoints to tasks !=xxx.
... e[reg]
    [yyyyyyyy,zzzzzzzz] The specified register 'reg' must have a value in the
       specified interval (if yyyyyyyy≤zzzzzzzz) or outside the interval
       [zzzzzzzz,yyyyyyyy] (if yyyyyyyy>zzzzzzzz).
... {1,2,4}xxxxxxxx
    [yyyyyyyy,zzzzzzzz] The specified 1-, 2- or 4-byte variable must have a
      value in the specified interval (if yyyyyyyy≤zzzzzzzz) or outside the
      interval [zzzzzzzz,yyyyyyyy] (if yyyyyyyy>zzzzzzzz).
... – All breakpoint restrictions of the breakpoint with number 'bpn' are reset.
```

Note:
Breakpoint restrictions are *not* reset when the breakpoint address or type are
changed or when the breakpoint is reset (b-). Breakpoint restrictions can be
explicitly reset (br-). However, changes of the restrictions do not require
prior reset. br{t,T}, br{e..} and br{1,2,4} restrictions are logically anded.
However, setting a restriction overwrites a prior restriction of that type.

### Single Stepping: S

If the single step mode is activated, the processor enters the kernel debugger
after each instruction. Note: On int 0xxx, the whole system call is executed.

ARCH: Only x86, amd64

```
S+ enables single stepping mode
```
```
S– disables single stepping mode
```

### Halt Thread: H

Halt a specific thread. This can be useful for instance if a thread stays in an
endless loop raising page faults or is filling the trace buffer.

ARCH: Not all architectures.

### Special Go: j

Resumes execution after/at the instruction which invoked JDB.

ARCH: Only x86, amd64

```
jb Continue until next branch (call, ret, jmp, int, iret instruction). This
mode works using the single step mode of the processor.
```
```
jr Continue until current function returns (until ret or iret instruction at
the current code level). This mode uses the single step mode of the
processor.
```
```
js Single Step (in difference to 'S+', we do not enter the single step mode)
```

## JDB-related User Level Function Calls

### Assigning user-defined debug names for objects

As a convenience when debugging some capabilities are by default created with a
name string which is displayed in JDB. Names can also be assigned to other
capabilities in the system as an aid to debugging.

- #include <l4/sys/debugger.h>
```
    l4_debugger_set_object_name()
```

### Adding Trace Buffer Events from User Level

Trace buffer events can be added from a user level program using the following
functions.

TODO Does the enter_kdebug() call enter into JDB as well or does it leave automatically?
TODO What registers does enter_kdebug("*+text") save on other ARCH?

User level trace events:

- #include <l4/sys/kdebug.h>
```
    enter_kdebug("*text");
    Creates a trace buffer entry containing the string 'text'.
```

- #include <l4/sys/kdebug.h>
```
    enter_kdebug("*+text");
    Creates a trace buffer entry containing the string 'text'. The values of the
    registers EAX, ECX, and EDX are stored together with the message.
```

- #include <l4/sys/ktrace.h>
```
    fiasco_tbuf_log("text");
    Creates a trace buffer entry containing the string 'text'. In contrast to
    using the sequence 'enter_kdebug("*text")', the string 'text' may be
    created at runtime.

    fiasco_tbuf_log_3val("text", val1, val2, val3);
    The string 'text' and three hexadecimal values are stored into a single
    trace buffer entry.

    fiasco_tbuf_log_binary(unsigned char const *data);
    Creates a trace buffer entry containing the binary 'data'.
```

### Trace Buffer Control from User Level

Some trace buffer control commands are callable from a user level program.
These can be used to clear and dump the trace buffer and obtain counts of
system events (e.g. context switches, page faults, irqs, etc).

- #include <l4/sys/ktrace.h>
```
    fiasco_tbuf_clear();
    fiasco_tbuf_dump();
    fiasco_tbuf_get_status();
    fiasco_tbuf_get_status_phys();
```

TODO How are these fiasco_tbuf_get_status() related to the 'C' counters? is it
for additional control from userland?


### Enterring JDB from User Level

A user program can add calls to the enter_kdebug() function to go into the
kernel debugger at that point in the execution of the program. This can be
useful for examining system state at certain points.

- #include <l4/sys/kdebug.h>
```
    enter_kdebug("text");
    Enters the kernel debugger with entry reason string 'text'.
    Note: JDB handles "text" distinctly from "*text".
```

### Remote Control from User Level

Some non-interactive JDB commands may be executed from a user level program by
using enter_kdebug() and providing a control sequence starting with "*#".

- #include <l4/sys/kdebug.h>
```
    enter_kdebug("*#");
```

For example, to log the IPC traffic for a specific code sequence use:

```
    /* start IPC logging, including IPC results */
    enter_kdebug("*#I*IR+");
    ...
    /* IPC here */
    ...
    /* stop IPC logging */
    enter_kdebug("*#I-");
```

## JDB-related Command Line Switches

TODO review these ==> Frank is looking into it
```
-wait Emit a debug trap after the kernel has been initialised and before user
programs are started. This will usually enter the kernel debugger and show the
debugger prompt.
```
```
-nojdb Disables the builtin kernel debugger JDB.
```
```
-nokdb Disables the GDB stub. Should Fiasco raise an exception, it will just
call the builtin jdb.
```
```
-noscreen Disables output to VGA/Hercules console.
```
```
-noserial Disables output to serial console. If this switch is not given,
kernel messages will output additionally to the serial interface. If '-nokdb'
is enabled, you can use a terminal program on the host to control Fiasco. If
you are connected to a remote GDB, messages are copied to GDB's console.
```
```
-comspeed=n Will set the rate of the serial interface to n bytes/second. 115200
baud is the default.
```
```
-comport=n Will use COMn for serial communication. COM1 is the default.
Possible values for n are 1, 2, 3, and 4. n is interpreted as an I/O port if n
> 4.
```
```
-hercules Redirect kernel messages to the Hercules (or other MGA-compatible)
console.
```
```
-esc Enable esc hack. On every timer interrupt, ask the keyboard if the Escape
key was pressed. If so, do enter into kernel debugger. Applications (e.g.
L4Linux) may be confused by dropped key events so better use -serial_esc.
```
```
-kmemsize=n Overwrite Fiasco's heuristic for required kernel memory. Set the
memory reserved for mapping trees, TCBs, and other to 'n' MB.
```
```
-watchdog Enable watchdog. On every timer interrupt, tell the watchdog that we
are sill alive. If a task disables the interrupts and loops, the timer
interrupt isn't called anymore and after 2 seconds the watchdog releases an non
maskable interrupt (NMI) which forces Fiasco to step into the kernel debugger.
Requires at least an Intel PPro or AMD K7 Model 2.
TODO rewrite OBSOLETE
```
```
-loadcnt Initialize a performance counter for counting all cycles the CPU is
not halted. The counter is accessible from userland via rdpmc(x) where x is 0
on P6/K7, and P4.
TODO rewrite OBSOLETE
```
```
-apic Initialize the builtin Local APIC. If the Local APIC is disabled by the
BIOS but available it is re-enabled.
```
```
-serial_esc Enter jdb on serial receive interrupts. This is only necessary if
kdb was disabled by '-nokdb'.
```
```
-tbuf_entries=n Set number of lines to store in the debugging trace buffer.
Default is 16384.
```
```
-outbuf=n Set output buffer for kddisplay functions to n bytes. Default is 8192.
```
```
-jdbcmd=cmds Execute Jdb commands non-interactive at startup. Example: use
-jdbcmd=I*IR+P*PR+to log all IPCs and page faults from startup.
```

## Credits

### Original Revision History

- Derived from LN KDB manual written by Jochen Liedtke.
- Released as JDB manual Version 1.00 by Jan Glauber (2001), Technische
Universität Dresden, Department of Computer Science.
- Updates by Frank Mehnert, Adam Lackorzynski, and Yann Le Du.
