#!/usr/bin/env python

import string,re,sys
import getopt
import pdb

def gen_esc_sym(*literals):
    return gen_escaped_symbols(*literals)

def gen_escaped_symbols(*literals):
    return tuple(map(lambda x: re.escape(x),literals))

context_marker = "class context_t %s %s size (\d+) vtable self  id \d+ %s" % \
                 gen_escaped_symbols("{", "/*", "*/")

d_members_context = ["_state",
                     "ready_next",
                     "ready_prev",
                     "kernel_sp",
                     "_space_context",
                     "_stack_link",
                     "_donatee",
                     "_lock_cnt",
                     "_thread_lock",
                     "_sched",
                     "_fpu_regs"]

sender_marker = "class sender_t %s %s size (\d+) vtable self  id \d+ %s" % \
                gen_escaped_symbols("{","/*","*/")

d_members_sender = ["_id",
                    "_send_partner",
                    "sender_next",
                    "sender_prev"]


receiver_marker = "class receiver_t : public context_t %s id \d+ %s %s %s size \d+ vtable class context_t %s id \d+ %s  id \d+ %s" % gen_escaped_symbols("/*","*/","{","/*","/*","*/","*/")



d_members_receiver = ["_partner",
                      "_receive_regs",
                      "_pagein_request",
                      "_pagein_applicant",
                      "_sender_first",
                      "_timeout"]



thread_marker = "class thread_t : public receiver_t %s id \d+ %s, public sender_t %s id \d+ %s %s bitpos (\d+) %s %s %s size (\d+) vtable class context_t %s id \d+ %s  id \d+ %s" % gen_escaped_symbols("/*","*/","/*", "*/", "/*","*/","{", "/*", "/*","*/","*/")

d_members_thread = ["_space",
                    "_thread_lock",             
#                    "_timeout",
                    "_pager",
                    "_preempter",
                    "_ext_preempter",
                    "present_next",
                    "present_prev",
                    "_irq",
                    "_idt",
                    "_idt_limit",
                    "_target_desc",
                    "_pagein_error_code",
                    "_vm_window0",
                    "_vm_window1",
                    "_recover_jmpbuf",
                    "_pf_timeout",
                    "_last_pf_address",
                    "_last_pf_error_code",
                    "_magic"]

class_members = {"context"  : d_members_context,
                 "receiver" : d_members_receiver,
                 "sender"   : d_members_sender,
                 "thread"   : d_members_thread}

f_members = []

verbose_output = 0

def do_print(*args):
    assert len(args) > 0
    if not verbose_output:
        return
    for i in range(len(args) -1):
        print args[i],
    print args[-1]

             
def extract_members(sections):
    section_members = {}
    for k , lines in sections.items():
        do_print( "extract members for %s, %d members to go" % (k, len(class_members[k])))
        line_nr = 0
        section_members[k] = {}
        for member in class_members[k]:
            assert line_nr < len(lines)
            p = re.compile("(?:\s|%s)(?:%s)?%s.*?; %s bitsize (\d+), bitpos (\d+) %s" % (gen_esc_sym("(","*")+ (member,) + gen_esc_sym("/*","*/")))
            while 1:
                try:
                    m = p.search(lines[line_nr])
                except:
                    print "ERROR: member %s not found for unit %s" % (member,k)
                    print "use --verbose for more debug output"
                    sys.exit(1)
                    assert 0
#                    pdb.set_trace()

                if m:
                    do_print( "found %s for %s %s %s @ %d" % (member, k, m.group(1), m.group(2), line_nr))
                    section_members[k][member] = (int(m.group(1)), int(m.group(2)))
                    break
                line_nr = line_nr + 1
    return section_members

def extract_sections(input_file):
    lines = open(input_file).readlines()

    section_keys = {"context" : context_marker,
                    "receiver" : receiver_marker,
                    "sender" : sender_marker,
                    "thread" : thread_marker}

    sections = {}
    line_nr = 0

    while section_keys and line_nr < len(lines) :
        marker_pat = []
        curr_section = []
        do_print( "generating new pattern, %d parts, line_nr %d (%d)" % \
              (len(section_keys), line_nr, len(lines)))
        for k,p in section_keys.items():
            marker_pat.append("(?P<%s>%s)" % (k,p))
        pat = re.compile("|".join(marker_pat))

        section_finished = 0
        while line_nr < len(lines) and not section_finished:
            m = pat.match(lines[line_nr])
            if m:
                d = m.groupdict()
                for k,v in d.items():
                    if v:
                        do_print("found section %s @ %d" % (k,line_nr))
                        curr_section.append(v)
                        del section_keys[k]
                        break
                else:
                    assert 0

                line_nr = line_nr + 1
                while 1:
                    curr_section.append(lines[line_nr])
                    m = re.match("%s;" % gen_escaped_symbols("}"), lines[line_nr])
                    if m:
                        sections[k] = curr_section
                        curr_section = []
                        do_print( "finished section %s @ %d" % (k,line_nr))
                        section_finished = 1
                        break
                    line_nr = line_nr + 1
            else:                
                line_nr = line_nr + 1
    return sections

def dump_members(prefix, offset, members):
    member_list = members.items()
    member_list.sort(lambda x, y: cmp(x[1][1], y[1][1]))

    for m in member_list:
        member = m[0].upper()
        while member[0] == "_":
            member = member[1:]
        name = "%s%s" % (prefix,member)
        if len(name) < 40:
             name = "%s%s" % (name, " " * (30 -len(name)))
        print "#define %s %3d" % (name, offset / 8 + m[1][1]/ 8)
    
def main(input_file):
    sections = extract_sections(input_file)

    assert len(sections) == 4
    section_members = extract_members(sections)

    dump_members("OFS__THREAD__", 0, section_members["context"])
    dump_members("OFS__THREAD__", 0, section_members["receiver"])

    m = re.match(thread_marker, sections["thread"][0])
    assert m

    sender_offset = int(m.group(1))
    thread_size   = int(m.group(2))
    
    print "#define CAST_thread_TO_sender   " , sender_offset / 8

    dump_members("OFS__THREAD__",sender_offset, section_members["sender"])
    dump_members("OFS__THREAD__",0, section_members["thread"])

    print "#define OFS__THREAD__MAX " , thread_size 

def usage():
    print "usage: %s [options]" % sys.argv[0]
    print "Options:"
    print "   --help              Display this information"
    print "   --verbose           Verbose output for debugging"
    
if __name__ == "__main__":
    input_file = "thread.debug"
    try:
        opts, args = getopt.getopt(sys.argv[1:], "", ["verbose","help","file="])
    except getopt.GetoptError, err:
        print err.msg
        usage();
        sys.exit(1)

    if args:
        usage()
        sys.exit(1)
        
    for o, a in opts:
        if o == "--verbose":
            verbose_output = 1
            continue
        if o == "--file":
            input_file = a
            continue
        if o == "--help":
            usage()
            sys.exit(0)
    main(input_file)
