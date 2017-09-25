# scan the L4 global configuration file
L4DIR	?= ../../..
#include $(L4DIR)/mk/Makeconf

TOOL = ../src/preprocess

all:: test

ALL_TESTS = $(TOOL_TESTS)
TOOL_TESTS = mapping mapping_inline random random_inline extern_c static \
  noinline explicit operator template template_inline c-preproc \
  inline inline_noinline \
  parser parser_noinline \
  multifile variable line line_not line_nh interface comment_in_string \
  default_args drop_single1 drop_single2 drop_single3 drop_multi1 \
  drop_multi2 implement_template implement_default implement_default_err \
  nested_class template_base_class func_defs

mapping_inline_src = mapping
mapping_inline_flags = -i

random_inline_src = random
random_inline_flags = -i

static_flags = -i

inline_flags = -i

inline_noinline_src = inline
inline_noinline_flags =

noinline_flags = -i

template_inline_src = template
template_inline_flags = -i

parser_flags = -i

parser_noinline_src = parser
parser_noinline_flags = 

multifile_src = multifile1 multifile2
multifile_flags = -i
multifile_extra = multifile-part1.cc multifile-part2.cc

line_not_src = line
line_not_flags = -l

line_nh_src = line
line_nh_flags = -L

interface_missing = interface.h
interface_extra = interfacepublic.h

drop_single1_src   = dropsection
drop_single1_flags = -s -e "bax"

drop_single2_src   = dropsection
drop_single2_flags = -s -e "bax ixbix"

drop_single3_src   = dropsection
drop_single3_flags = -s -e "bax aba"

drop_multi1_src    = dropsection dropsection-ext
drop_multi1_flags  = -s -e "bax aba"
drop_multi1_extra  = drop_multi1-ext.cc

drop_multi2_src    = dropsection dropsection-ext
drop_multi2_flags  = -s -e "bax aba ext"
drop_multi2_extra  = drop_multi2-ext.cc

ifdef_src          = ifdef
ifdef_flags        = -s -e "true"

ifdef1_src         = ifdef1
ifdef1_flags       = -s -e "true"
ifdef1_extra       = ifdef1-more.cpp

implement_default_redirect     = 2>implement_default.stderr
implement_default_extra        = implement_default.stderr

implement_default_err_redirect = 2>implement_default_err.stderr; test $$? -ne 0
implement_default_err_extra    = implement_default_err.stderr
implement_default_err_missing += implement_default_err.h
implement_default_err_missing += implement_default_err_i.h
implement_default_err_missing += implement_default_err.cc

random.cpp: combine.pl
	perl $< > $@.new
	mv $@.new $@

clean::
	rm -f random.cpp

test_rules: Makefile
	rm -f $@.new
	for test in $(TOOL_TESTS); \
	  do \
	    echo "ifndef $${test}_src" >> $@.new; \
	    echo "$${test}_src = $${test}" >> $@.new; \
	    echo "endif" >> $@.new; \
	    echo "ifndef $${test}_flags" >> $@.new; \
	    echo "$${test}_flags = " >> $@.new; \
	    echo "endif" >> $@.new; \
	    echo "ifndef $${test}_redirect" >> $@.new; \
	    echo "$${test}_redirect = " >> $@.new;\
	    echo "endif" >> $@.new; \
	    echo "$${test}.cc: \$$(addsuffix .cpp, \$$($${test}_src)) \$$(TOOL); \$$(TOOL) \$$($${test}_flags) -c $${test} \$$(filter-out \$$(TOOL), \$$^) \$$($${test}_redirect)" >> $@.new; \
	    echo "ifdef $${test}_extra" >> $@.new; \
	    echo "clean:: ; \$$(RM) \$$($${test}_extra)" >> $@.new; \
	    echo "endif" >> $@.new; \
	  done
	mv $@.new $@

include test_rules

clean::
	rm -f $(addsuffix .cc, $(TOOL_TESTS)) \
	  $(addsuffix .h, $(TOOL_TESTS)) \
	  $(addsuffix _i.h, $(TOOL_TESTS)) 

.PHONY: test
test: $(addsuffix .t.ok, $(ALL_TESTS)) 

%.t.ok: %.cc
	@echo -n "Running test $* ... "
ifeq ($(RECREATE_OUTPUT),1)
	@cp $(filter-out $($*_missing),$*.h $*_i.h $*.cc) $($*_extra) verify/
endif
	@ERROR=0; \
	for i in $(filter-out $($*_missing),$*.h $*_i.h $*.cc) $($*_extra); \
	  do \
	    if ! diff -u verify $$i; then ERROR=1; fi; \
	  done; \
	test $$ERROR -eq 0
	@echo "OK"
	@touch $@

.PHONY: init
init:
	$(MAKE) test RECREATE_OUTPUT=1

install:
	@echo Not installing tests.

clean:: 
	$(RM) $(ALL) $(OBJS) *.t.ok

cleanall::      clean
	$(RM) *~ .*.d test_rules

#include $(DEPS)
