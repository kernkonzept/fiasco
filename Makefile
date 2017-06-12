#
#
# GLOBAL Makefile for FIASCO

# enviroment
DFLBUILDDIR	:= build
ALLBUILDDIR	:= build-all
RANDBUILDDIR	:= build-rand
TEMPLDIR	:= src/templates
MAKEFILETEMPL	:= $(TEMPLDIR)/Makefile.builddir.templ
MANSUBDIRS	:= man
INSTALLSUBDIRS	:= $(MANSUBDIRS)
CLEANSUBDIRS	:= $(MANSUBDIRS) $(wildcard $(DFLBUILDDIR))
CONFIG_FILE	:= $(TEMPLDIR)/globalconfig.out
TEST_TEMPLATES	:= $(patsubst $(CONFIG_FILE).%,%,$(wildcard $(CONFIG_FILE).*))
TEST_TEMPLATES  := $(if $(TEMPLATE_FILTER),$(filter $(TEMPLATE_FILTER),$(TEST_TEMPLATES)),$(TEST_TEMPLATES))
DFL_TEMPLATE	:= ia32-1

buildmakefile = mkdir -p "$(1)";                                        \
		perl -p -e '$$s = "$(CURDIR)/src"; s/\@SRCDIR\@/$$s/'   \
			< $(MAKEFILETEMPL) > $(1)/Makefile

ifneq ($(strip $(B)),)
BUILDDIR := $(B)
endif
ifneq ($(strip $(BUILDDIR)),)
builddir:
	@echo "Creating build directory \"$(BUILDDIR)\"..."
	@if [ -e "$(BUILDDIR)" ]; then			\
		echo "Already exists, aborting.";	\
		exit 1;					\
	fi
	@$(call buildmakefile,$(BUILDDIR))
	@if [ -f "$(TEMPLDIR)/globalconfig.out.$(T)" ]; then		\
		echo "Copying template configuration $(T)";		\
		cp $(TEMPLDIR)/globalconfig.out.$(T) $(BUILDDIR)/globalconfig.out;		\
	fi
	@echo "done."
endif

ifneq ($(strip $(O)),)
builddir:
	@if [ ! -e "$(O)" ]; then			\
	  echo "build directory does not exist: '$(O)'";\
		exit 1;					\
	fi
	@$(call buildmakefile,$(O))
	@$(MAKE) -C $(O)

%:
	@if [ ! -e "$(O)" ]; then			\
	  echo "build directory does not exist: '$(O)'";\
		exit 1;					\
	fi
	@$(call buildmakefile,$(O))
	@$(MAKE) -C $(O) $@
endif

ifneq ($(strip $(T)),)
this:
	set -e;							      \
	test -f $(TEMPLDIR)/globalconfig.out.$(T);		      \
	bdir=T-$(DFLBUILDDIR)-$(T);				      \
	rm -rf $$bdir;						      \
	$(call buildmakefile,T-$(DFLBUILDDIR)-$(T));		      \
	cp $(TEMPLDIR)/globalconfig.out.$(T) $$bdir/globalconfig.out; \
	$(MAKE) -C $$bdir olddefconfig;                               \
	$(MAKE) -C $$bdir
endif

$(DFLBUILDDIR): fiasco.builddir.create
	$(MAKE) -C $@

all:	fiasco man

clean cleanall:
	set -e; for i in $(CLEANSUBDIRS); do $(MAKE) -C $$i $@; done

purge: cleanall
	$(RM) -r $(DFLBUILDDIR)

man:
	set -e; for i in $(MANSUBDIRS); do $(MAKE) -C $$i; done

fiasco.builddir.create:
	@[ -e $(DFLBUILDDIR)/Makefile ] ||			 \
		($(call buildmakefile,$(DFLBUILDDIR)))
	@[ -f $(DFLBUILDDIR)/globalconfig.out ] || {		 \
		cp  $(TEMPLDIR)/globalconfig.out.$(DFL_TEMPLATE) \
			$(DFLBUILDDIR)/globalconfig.out;	 \
		$(MAKE) -C $(DFLBUILDDIR) olddefconfig;	         \
	}

config $(filter config %config,$(MAKECMDGOALS)): fiasco.builddir.create
	$(MAKE) -C $(DFLBUILDDIR) $@

fiasco: fiasco.builddir.create
	$(MAKE) -C $(DFLBUILDDIR)

checkallseq:
	@error=0;						      \
	$(RM) -r $(ALLBUILDDIR);				      \
	for X in $(TEST_TEMPLATES); do				      \
		echo -e "\n= Building configuration: $$X\n\n";	      \
		$(call buildmakefile,$(ALLBUILDDIR)/$$X);	      \
		cp $(TEMPLDIR)/globalconfig.out.$$X		      \
		   $(ALLBUILDDIR)/$$X/globalconfig.out;		      \
		if $(MAKE) -C $(ALLBUILDDIR)/$$X; then	              \
			[ -z "$(KEEP_BUILD_DIRS)" ] &&		      \
			   $(RM) -r $(ALLBUILDDIR)/$$X;		      \
		else						      \
			error=$$?;				      \
			failed="$$failed $$X";			      \
		fi						      \
	done;							      \
	rmdir $(ALLBUILDDIR) >/dev/null 2>&1;			      \
	[ "$$failed" ] && echo -e "\nFailed configurations:$$failed"; \
	exit $$error;

checkall l4check:
	$(RM) -r $(ALLBUILDDIR)
	$(MAKE) dobuildparallel SHELL=bash

.PHONY: dobuildparallel checkallp

dobuildparallel: $(addprefix $(ALLBUILDDIR)/,$(TEST_TEMPLATES))
	@error=0;							      \
	echo "======================================================";        \
	for d in $(TEST_TEMPLATES); do                                        \
	  if [ -e $(ALLBUILDDIR)/$$d/build.failed ]; then                     \
	    error=1; failed="$$failed $$d";                                   \
	  fi;                                                                 \
	done;                                                                 \
	for f in $$failed; do echo "====== Failed Build Log: $$f ======";     \
	  tail -60 $(ALLBUILDDIR)/$$f/build.log;                              \
	done;                                                                 \
	rmdir $(ALLBUILDDIR) >/dev/null 2>&1;			              \
	[ "$$failed" ] && echo -e "\nFailed configurations:$$failed";         \
	exit $$error;

$(addprefix $(ALLBUILDDIR)/,$(TEST_TEMPLATES)):
	$(call buildmakefile,$@)
	cp $(TEMPLDIR)/globalconfig.out.$(patsubst $(ALLBUILDDIR)/%,%,$@)     \
	   $@/globalconfig.out
	INCLUDE_PPC32=y INCLUDE_SPARC=y                                       \
	$(MAKE) -C $@ olddefconfig 2>&1 | tee -a $@/build.log
	INCLUDE_PPC32=y INCLUDE_SPARC=y                                       \
	$(MAKE) -C $@ 2>&1 | tee -a $@/build.log;                             \
	buildexitcode=$${PIPESTATUS[0]};                                      \
	grep -2 ": warning: " $@/build.log > $@/warnings.log;                 \
	test -s $@/warnings.log                                               \
	  && mv $@/warnings.log $(ALLBUILDDIR)/warnings.$(@F);                \
	if [ $$buildexitcode = 0 -o -e $@/buildcheck.ignore ];                \
	then [ -z "$(KEEP_BUILD_DIRS)" ] && $(RM) -r $@;                      \
	else echo $$buildexitcode > $@/build.failed; fi; true

list:
	@echo "Templates:"
	@echo $(TEST_TEMPLATES)

RANDBUILDMAXTIME   = $(shell $$((2 * 3600)))
RANDBUILDONERUN    = 50
RANDBUILDDIRS      = $(foreach idx,$(shell seq $(RANDBUILDONERUN)),$(RANDBUILDDIR)/b-$(idx))
RANDBUILDAGAINDIRS = $(wildcard $(RANDBUILDDIR)/failed-*)

.PHONY: $(RANDBUILDAGAINDIRS)

randcheck:
	$(RM) -r $(RANDBUILDDIR)
	$(call buildmakefile,$(RANDBUILDDIR)/build-templ);
	starttime=$$(date +%s);                                              \
	if [ -z "$$RANDBUILDTIME" ]; then d=0; else d=$$RANDBUILDTIME; fi;   \
	while [ "$$d" = 0                                         \
	        -o $$(date +%s) -lt $$(($$starttime + $$d)) ]; do \
	  $(MAKE) dobuildrandparallel SHELL=bash;                            \
	  if [ $$(ls $(RANDBUILDDIR) | grep -c failed-)                      \
	       -gt $$(($(RANDBUILDONERUN) / 2)) ];                           \
	  then break; fi;                                                    \
	done;                                                                \
	echo "$$(ls $(RANDBUILDDIR) | grep -c failed-) directories"          \
	     "failed to build.";                                             \
	echo "Build time: $$((($$(date +%s) - $$starttime) / 60)) minutes."


randchecktimed:
	@$(MAKE) randcheck RANDBUILDTIME=$(RANDBUILDMAXTIME)

dobuildrandparallel: $(RANDBUILDDIRS)

define rand_build_a_dir
	$(MAKE) -C $(1) 2>&1 | tee -a $(1)/build.log;           \
	buildexitcode=$${PIPESTATUS[0]};                        \
	grep -2 ": warning: " $(1)/build.log > $(1)/warn.log;   \
	test -s $(1)/warn.log                                   \
	  && mv $(1)/warn.log $(RANDBUILDDIR)/warnings-$(2);    \
	if [ $$buildexitcode = 0 ]; then                        \
	  cp $(1)/globalconfig.out $(RANDBUILDDIR)/ok-$(2);     \
	  rm -r $(1);                                           \
	elif [ -n "$(3)" ]; then                                \
	  [ -n "$$STOP_ON_ERROR" ] && exit 1;                   \
	  mv $(1) $(RANDBUILDDIR)/failed-$(2);                  \
	fi
endef

$(RANDBUILDDIRS):
	cp -a $(RANDBUILDDIR)/build-templ $@
	until                                                 \
	  INCLUDE_PPC32=y INCLUDE_SPARC=y                     \
	    $(MAKE) -C $@ randconfig | tee -a $@/build.log;   \
	  INCLUDE_PPC32=y INCLUDE_SPARC=y                     \
	    $(MAKE) -C $@ oldconfig  | tee -a $@/build.log;   \
	do [ $${PIPESTATUS[0]} != 0 ]; done
	+fn=$$(cat $@/globalconfig.out                        \
	      | grep -e "^CONFIG_" | sort | sha1sum           \
	      | cut -f1 -d\   );                              \
	if [ -e "ok-$$fn" -o -e "failed-$$fn" ]; then         \
	  echo "Configuration $$fn already checked."          \
	  continue;                                           \
	fi;                                                   \
	$(call rand_build_a_dir,$@,$$fn,1)

$(RANDBUILDAGAINDIRS):
	+$(call rand_build_a_dir,$@,$(patsubst $(RANDBUILDDIR)/failed-%,%,$@))

randcheckstop:
	$(MAKE) STOP_ON_ERROR=1 randcheck

randcheckagain_bash: $(RANDBUILDAGAINDIRS)

randcheckagain:
	$(MAKE) randcheckagain_bash SHELL=bash
	@echo "Processed $(words $(RANDBUILDAGAINDIRS)) build directories."
	@echo "$$(ls -d $(RANDBUILDDIR)/failed-* | wc -l) directories remain to fail."

randcheckagainstop:
	$(MAKE) STOP_ON_ERROR=1 randcheckagain

randcheckstat:
	@find $(RANDBUILDDIR) -name 'ok-*' | tool/configstat
	@echo "Building: $$(ls $(RANDBUILDDIR) | grep -c b-)    " \
	      "Ok: $$(ls $(RANDBUILDDIR) | grep -c ok-)    "      \
	      "Failed: $$(ls $(RANDBUILDDIR) | grep -c failed-)"

randcheckstatloop:
	@while true; do \
	  $(MAKE) --no-print-directory randcheckstat; \
	  sleep 5; \
	done

remotemake:
	@r=$(word 1,$(REMOTEOPTS)); h=$${r%:*}; p=$${r#*:};                           \
	ssh $$h mkdir -p $$p;                                                         \
	rsync -aP --delete Makefile src tool $$r;                                              \
	ssh $$h "cd $$p && make $(wordlist 2,$(words $(REMOTEOPTS)),$(REMOTEOPTS))";  \
	echo "Returned from $$r."

help:
	@echo
	@echo "fiasco                    Builds the default configuration ($(DFL_TEMPLATE))"
	@echo "T=template                Build a certain configuration"
	@echo "checkall                  Build all template configurations in one go"
	@echo "list                      List templates"
	@echo
	@echo "config menuconfig xconfig oldconfig"
	@echo "                          Configure kernel in \"$(DFLBUILDDIR)\""
	@echo "$(DFLBUILDDIR)                     Build kernel in \"$(DFLBUILDDIR)\""
	@echo "clean cleanall            clean or cleanall in \"$(CLEANSUBDIRS)\""
	@echo "purge                     cleanall, remove \"$(DFLBUILDDIR)\" and build helper"
	@echo
	@echo "Creating a custom kernel:"
	@echo
	@echo "  Create a build directory with:"
	@echo "     make BUILDDIR=builddir"
	@echo "  Then build the kernel:"
	@echo "     cd builddir"
	@echo "     make config"
	@echo "     make"
	@echo
	@echo "Call \"make help\" in the build directory for more information on build targets."
	@echo
	@echo "Default target: $(DFLBUILDDIR)"
	@echo

.PHONY:	man install clean cleanall fiasco.builddir.create fiasco \
	l4check checkall config oldconfig menuconfig nconfig xconfig \
	randcheck randcheckstop randcheckagain_bash randcheckstat \
	randcheckstatloop help
