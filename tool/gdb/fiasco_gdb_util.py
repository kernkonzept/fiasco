# Some useful utilities to be used with Fiasco.OC
# by adam@l4re.org
#

import gdb
import re
import os

class Toff(gdb.Command):

  print_all = False

  def __init__(self):
    super (Toff, self).__init__ ('fiasco-offsets', gdb.COMMAND_DATA)

  def print_members(self, t, indent = 0):
    for f in sorted(t.fields(), key=lambda x: getattr(x, 'bitpos', -1)):
      if f.type.code in [gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION]:
        if hasattr(f, 'bitpos'):
          self.print_type(f.type, indent + 1, f.name,
                          "/* %d+%d */" % (f.bitpos // 8, f.type.sizeof))
        elif self.print_all:
          self.print_type(f.type, indent + 1, f.name,
                          "/* %d */" % (f.type.sizeof))

      elif hasattr(f, 'bitpos'):
        print('  ' * indent + '  %s %s; /* %d+%d */' % (f.type, f.name,
                                                        f.bitpos // 8,
                                                        f.type.sizeof))
      elif self.print_all:
        print('  ' * indent + '  %s %s; /* %d */' % (f.type, f.name,
                                                     f.type.sizeof))

  def print_type(self, t, indent = 0, sname = None, t_desc = ""):
    if t.code in [gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION]:
      s = "struct" if t.code is gdb.TYPE_CODE_STRUCT else "union"
      if t.name == None:
        print('  ' * indent + "%s {" % (s))
      else:
        print('  ' * indent + "%s %s {" % (s, t.name))

    self.print_members(t, indent)
    if t.code in [ gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION]:
      if sname is None:
        print('  ' * indent + "}; %s" % (t_desc))
      else:
        print('  ' * indent + "} %s; %s" % (sname, t_desc))

  def invoke(self, arg, from_tty):
    argv = gdb.string_to_argv(arg)
    if len(argv) < 1:
      raise gdb.GdbError('fiasco-offsets needs a type.')
    self.print_type(gdb.lookup_type(argv[0]))

Toff()

class Log_table(gdb.Command):
  def __init__(self):
    super (Log_table, self).__init__('fiasco-libftbuf-gen',
                                     gdb.COMMAND_DATA, gdb.COMPLETE_FILENAME)

  db = {}

  def invoke(self, arg, from_tty):
    argv = gdb.string_to_argv(arg)
    if len(argv) < 1:
      raise gdb.GdbError('fiasco-libftbuf-gen: Need at least one image file.')

    for f in argv:
      self.query_one(f)

    self.write_out()

  def query_one(self, file):
    print("Processing %s" % (file));
    gdb.execute("file %s" % (file))
    log_table     = gdb.execute("info address _log_table", False, True)
    log_table_end = gdb.execute("info address _log_table_end", False, True)

    # is there any more direct way of getting the addresses?
    regexp = re.compile(' (is at|at address) (0x\w+)')
    m_start = regexp.search(log_table)
    m_end   = regexp.search(log_table_end)
    if not m_start or not m_end:
      raise gdb.GdbError("Failed to get _log_table and/or _log_table_end")

    log_table       = int(m_start.group(2), 0)
    log_table_end   = int(m_end.group(2), 0)
    log_table_entry = gdb.lookup_type("Tb_log_table_entry")

    for e in range(log_table, log_table_end, log_table_entry.sizeof):
      fullname = gdb.parse_and_eval("((Tb_log_table_entry *)%d)->name" % e)
      # advance to next string go get the shortname
      tag = fullname
      while True:
        v = gdb.parse_and_eval("*(char *)%d" % tag);
        tag += 1
        if v == 0:
          break

      tag_s = tag.string()
      fn_s = fullname.string()

      if hasattr(self.db, tag_s) and self.db[tag_s] != fn_s:
        raise gdb.GdbError("Mismatch, should not happen (%s vs %s for %s)" % (
                           self.db[tag_s], fn_s, tag_s))

      self.db[tag_s] = fn_s

  def write_out(self):
    libftbuf_base_dir = os.getenv("LIBFTBUF_BASE_DIR")
    if not libftbuf_base_dir:
      raise gdb.GdbError("Error: Need to set envvar LIBFTBUF_BASE_DIR")
    f_header = libftbuf_base_dir + '/include/kernel_inc.h'
    f_c      = libftbuf_base_dir + '/lib/src/data_inc.c'

    print("Writing", f_header)
    with open(f_header, 'w') as f:
      f.write("/* This code is auto-generated from Fiasco binaries */\n")
      f.write("#pragma once\n")
      f.write("\n")
      f.write("#ifndef __LIBFTBUF_KERNEL_INC_OK\n")
      f.write("#error Do not include this header directly, use l4/libftbuf/ftbuf.h.\n")
      f.write("#endif\n")
      f.write("\n")
      f.write("#include <l4/sys/ktrace_events.h>\n")
      f.write("\n")
      f.write("struct libftbuf_tb_entries_dyn_names_t\n")
      f.write("{\n")
      f.write("  const char *name;\n")
      f.write("  const char *tag;\n")
      f.write("};\n")
      f.write("\n")
      f.write("extern struct libftbuf_tb_entries_dyn_names_t\n")
      f.write("  libftbuf_tb_entries_dyn_names[];\n");
      f.write("\n")
      f.write("enum libftbuf_tb_entry_dyn_event\n")
      f.write("{\n")
      first = True
      for k in sorted(self.db.keys()):
        k = k.replace(" ", "_")
        if first:
          first = False
          f.write("  Ftbuf_event_%s = l4_ktrace_tbuf_dynentries,\n" % (k))
        else:
          f.write("  Ftbuf_event_%s,\n" % (k))
      f.write("  Ftbuf_event_max\n")
      f.write("};\n")


    print("Writing", f_c)
    with open(f_c, 'w') as f:
      f.write("/* This code is auto-generated from Fiasco binaries */\n")
      f.write("\n")
      f.write("#define __LIBFTBUF_KERNEL_INC_OK\n")
      f.write("#include <l4/libftbuf/kernel_inc.h>\n")
      f.write("\n")
      f.write("struct libftbuf_tb_entries_dyn_names_t libftbuf_tb_entries_dyn_names[] =\n")
      f.write("{\n")
      for k in sorted(self.db.keys()):
        f.write("  { \"%s\", \"%s\" },\n" % (self.db[k], k))
      f.write("};\n")

Log_table()


class Fiasco_tbuf(gdb.Command):

  base_block_size = 0
  tb_entry_size = 0
  typedefs = {}
  ktrace_events_file = "ktrace_events.h"

  ktrace_shortnames = {
       "Context::Drq_log": "drq",
       "Context::Vcpu_log": "vcpu",
       "Factory::Log_entry": "factory",
       "Ipc_gate::Log_ipc_gate_invoke": "gate",
       "Irq_base::Irq_log": "irq",
       "Kobject::Log_destroy": "destroy",
       "Mu_log::Map_log": "map",
       "Mu_log::Unmap_log": "unmap",
       "Rcu::Log_rcu": "rcu",
       "Task::Log_unmap": "tunmap",
       "Tb_entry_bp" : "bp",
       "Tb_entry_ctx_sw": "context_switch",
       "Tb_entry_ipc": "ipc",
       "Tb_entry_ipc_res": "ipc_res",
       "Tb_entry_ipc_trace": "ipc_trace",
       "Tb_entry_empty": "empty",
       "Tb_entry_ke": "ke",
       "Tb_entry_ke_bin": "ke_bin",
       "Tb_entry_ke_reg": "ke_reg",
       "Tb_entry_pf": "pf",
       "Tb_entry_sched": "sched",
       "Tb_entry_trap": "trap",
       "Tb_entry_union": "fullsize",
       "Thread::Log_exc_invalid": "ieh",
       "Thread::Log_pf_invalid": "ipfh",
       "Thread::Log_thread_exregs": "exregs",
       "Thread::Migration_log": "migration",
       "Timer_tick::Log": "timer",
       "Vm_svm::Log_vm_svm_exit": "svm",
     }

  # Non-simple types
  known_types_map = {
       "Cap_index"              : "unsigned long",
       "Cpu_number"             : "unsigned",
       "Context::Drq_log::Type" : "unsigned",
       "L4_msg_tag"             : "unsigned long",
       "L4_obj_ref"             : "unsigned long",
       "L4_timeout_pair"        : "unsigned",
       "L4_error"               : "unsigned long",
       "cxx::Type_info"         : "unsigned long",
     }

  printlog_buf_current = 0
  printlog_buf = [ "", "", "" ]

  def __init__(self):
    super (Fiasco_tbuf, self).__init__("fiasco-gen-ktrace-events",
                                       gdb.COMMAND_DATA)

  def printlog(self, s):
    print(s, end=' ')
    self.printlog_buf[self.printlog_buf_current] += s

  def printlogi(self, indentlevel, s):
    ins = ' ' * (indentlevel * 1)
    print("%s%s" % (ins, s), end=' ')
    self.printlog_buf[self.printlog_buf_current] += ins + s

  def printlog_set_current_section(self, section):
    self.printlog_buf_current = section

  def printlog_write(self):
    with open(self.ktrace_events_file, 'w') as f:
      f.write(self.printlog_buf[0])
      f.write(self.printlog_buf[1])
      f.write(self.printlog_buf[2])

  def convert_name_to_c(self, name):
    return "L4_ktrace_t__" + name.replace("::", "__")

  def handle_type_pointer(self, t):
    rt = str(t)

    if rt != "void" and rt != "char":
      rt = self.convert_name_to_c(rt)
      self.typedefs[rt] = "void"
    return rt

  def handle_type(self, t):
    if t.name in self.known_types_map:
      # check that our conversion map is right
      t2 = gdb.lookup_type(self.known_types_map[t.name]);
      if t.sizeof != t2.sizeof:
        raise gdb.GdbError("%s(%d) -> %s(%d) is not valid." % (
                           t.name, t.sizeof, t2.name, t2.sizeof))

      st__ = self.convert_name_to_c(t.name)
      self.typedefs[st__] = self.known_types_map[t.name]
      return st__

    if t.name == "bool":
      return "char"

    rtbasic = str(gdb.types.get_basic_type(t))

    if str(rtbasic) != t.name:
      n = self.convert_name_to_c(t.name)
      self.typedefs[n] = rtbasic
      return n
    return t.name

  def print_members(self, t, prepad, postpad = False, indent = 2):
    first = True
    behind_last_member = 0
    padidx = 1
    cur_size = 0
    for f in sorted(t.fields(), key=lambda x: getattr(x, 'bitpos', -1)):
      if f.name == "Tb_entry":
        continue

      if hasattr(f, 'bitpos'):
        byteoff = f.bitpos // 8
        if byteoff * 8 != f.bitpos:
          raise gdb.GdbError("Don't know how to handle bitfields, hack me")

        if prepad:
          prepad = False
          if self.base_block_size != 0 and byteoff != self.base_block_size:
            # Add padding
            padding = byteoff - self.base_block_size
            self.printlogi(indent, 'char __pre_pad[%d];\n' % padding)
        elif cur_size < byteoff:
          padding = byteoff - cur_size
          self.printlogi(indent, 'char __pad_%d[%d];\n' % (padidx, padding))
          padidx += 1


        behind_last_member = byteoff + f.type.sizeof
        if f.type.code == gdb.TYPE_CODE_ARRAY:
          tc = self.handle_type(f.type.target().unqualified())
          c = "%s %s[%d]" % (tc, f.name, f.type.range()[1] + 1)
          self.printlogi(indent, '%s; /* %d+%d */\n' % (c, byteoff,
                                                        f.type.sizeof))
        elif f.type.code == gdb.TYPE_CODE_PTR:
          tc = self.handle_type_pointer(f.type.target().unqualified())
          self.printlogi(indent, '%s *%s; /* %d+%d */\n'
                        % (tc, f.name, byteoff, f.type.sizeof))

        elif (f.type.code in [gdb.TYPE_CODE_UNION, gdb.TYPE_CODE_STRUCT]
              and str(f.type.unqualified()) not in self.known_types_map):
          s = "struct" if f.type.code is gdb.TYPE_CODE_STRUCT else "union"
          self.printlogi(indent, '%s __attribute__((__packed__)) {\n' % (s))
          self.print_members(f.type, False, False, indent + 2)
          self.printlogi(indent, '} %s; /* %d+%d */\n' % (
                         f.name, byteoff, f.type.sizeof))
        else:
          tc = self.handle_type(f.type.unqualified())
          self.printlogi(indent, '%s %s; /* %d+%d */\n'
                         % (tc, f.name, byteoff, f.type.sizeof))

        cur_size = byteoff + f.type.sizeof

    if postpad:
      if behind_last_member > self.tb_entry_size:
        raise gdb.GdbError("Error: %s is too big (expected <= %d)" % (
                           t.name, self.tb_entry_size))
      sz = self.tb_entry_size - behind_last_member
      self.printlogi(indent, 'char __post_pad[%d]; /* %d+%d */\n'
                     % (sz, behind_last_member, sz))

    return behind_last_member

  def print_single_struct(self, t, sname):
    self.printlogi(4, "struct __attribute__((__packed__))\n")
    self.printlogi(4, "{\n")
    self.print_members(t, True, sname == "fullsize", 6)
    self.printlogi(4, "} %s; /* %d */\n" % (sname, t.sizeof))

  def gen_ktrace_events(self, tbentry_types):
    self.printlog("/* Note, automatically generated from Fiasco binary */\n")
    self.printlog("#pragma once\n")
    self.printlog("\n")

    t = gdb.lookup_type("Tbuf_entry_fixed")
    if t.code is gdb.TYPE_CODE_ENUM:
      self.printlog("enum L4_ktrace_tbuf_entry_fixed\n")
      self.printlog("{\n")
      for f in t.fields():
        self.printlog("  l4_ktrace_%s = %d,\n" % (str.lower(f.name),
                                                  f.enumval))
      self.printlog("};\n")
    else:
      raise gdb.GdbError("Missing Tbuf_entry_fixed, old Fiasco?")

    # Unfortunately we are not able to extract Tb_entry::Tb_entry_size
    # so apply this guess:
    mword = gdb.lookup_type("Mword")
    self.tb_entry_size = 128 if mword.sizeof == 8 else 64

    print("Guessed Tb_entry size:", self.tb_entry_size)

    self.printlog_set_current_section(2)

    self.printlog("\n")
    self.printlog("typedef struct __attribute__((packed))\n")
    self.printlog("{\n")
    self.base_block_size = self.print_members(gdb.lookup_type("Tb_entry"), False)

    self.printlog("  union __attribute__((__packed__))\n")
    self.printlog("  {\n")

    for i in sorted(tbentry_types, key=lambda t: t.name):
      if i.name in self.ktrace_shortnames:
        self.print_single_struct(i, self.ktrace_shortnames[i.name])
      else:
        raise gdb.GdbError("Missing '%s' in internal knowledge " \
                           "base. Please add." % (i.name))

    self.printlog("  } m;\n")
    self.printlog("} l4_tracebuffer_entry_t;\n")

    self.printlog_set_current_section(1)
    self.printlog("\n")
    for i in sorted(self.typedefs.keys()):
      self.printlog("typedef %s %s;\n" % (self.typedefs[i], i))

    self.printlog_write()

  def get_tbentry_classes(self):
    print("Querying Tb_entry types. This might take a while.")
    # Is there any faster way of doing this?
    output = gdb.execute("info types", False, True)
    regexp = re.compile('^(\S+);$') # should fetch all we need
    types = []
    for line in output.split('\n'):
      m = regexp.match(line)
      if m:
        t = gdb.lookup_type(m.group(1))
        if "Tb_entry" in t and t["Tb_entry"].is_base_class:
          types.append(t)

    return types

  def invoke(self, arg, from_tty):
    tbentry_types = self.get_tbentry_classes()
    self.gen_ktrace_events(tbentry_types)

Fiasco_tbuf()
