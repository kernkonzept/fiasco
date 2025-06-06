srcdir		?= NOT_SET
tooldir		:= $(srcdir)/../tool

ifneq ($(if $(SANDBOX_IN_PROGRESS),,$(SANDBOX_ENABLE)),)

.PHONY: $(MAKECMDGOALS) do-all-make-targets

do-all-make-targets:
	$(VERBOSE)SANDBOX_IN_PROGRESS=1 \
	   $(tooldir)/sandbox \
	              --sys-dir "$(or $(SANDBOX_SYSDIR),/)" \
	              --dir-rw "$(objbase)" \
	              --dir-ro "$(srcdir)/.." \
	              --cmd "$(MAKE) -C $(objbase) $(MAKECMDGOALS)"

$(MAKECMDGOALS): do-all-make-targets

VERBOSE ?= @
ifeq ($(V),1)
  VERBOSE :=
endif

else

.PHONY: all do-all test-all config textconfig menuconfig xconfig \
        oldconfig regenconfig mrproper doc help update nconfig \
        savedefconfig listnewconfig oldnoconfig oldaskconfig FORCE

all:

help:
	@echo "Possible targets are:"
	@echo "  menuconfig     - configure Fiasco (ncurses mode)"
	@echo "  config         - like menuconfig"
	@echo "  textconfig     - line-oriented config"
	@echo "  xconfig        - configure Fiasco (graphical mode)"
	@echo "  all            - Fiasco binary"
	@echo "  clean          - clear all auto-generated files in auto"
	@echo "  cleanall       - like clean + dependencies"
	@echo "  mrproper       - like cleanall + config files"
	@echo "  update         - update Fiasco and preprocess using svn"
	@echo "  DEPS           - dependencies between kernel object files"
	@echo "  DEPS.ps        - graphical (ps) representation of DEPS"
	@echo "  DEPS.svg       - graphical (svg) representation of DEPS"
	@echo "  DEPS.tred.ps   - transitive reduction of DEPS.ps"
	@echo "  DEPS.tred.svg  - transitive reduction of DEPS.svg"
	@echo "  doc            - doxygen HTML documentation into docs/"
	@echo "  report         - show host configuration information"
	@echo "  TAGS tags      - create tags files"


ifneq ($(srcdir),NOT_SET)
Makefile: $(srcdir)/templates/Makefile.builddir.templ
	perl -p -i -e '$$s = "$(srcdir)"; s/\@SRCDIR\@/$$s/' \
			< $< >$@
endif

ifneq ($(MAKECMDGOALS),help)

ifeq ($(srcdir),NOT_SET)
all $(filter config %config,$(MAKECMDGOALS)):
	@echo "======================================================================"
	@echo " Building Fiasco in the src directory is not possible!"
	@echo ""
	@echo " Go to the Fiasco root directory and create your build directory with"
	@echo "    cd .. && make BUILDDIR=build-dir"
	@echo "======================================================================"
	@exit 1

else # srcdir != NOT_SET

all: globalconfig.h globalconfig.tags

ifeq ($(filter config %config,$(MAKECMDGOALS)),)

-include globalconfig.out
-include globalconfig.tags

# use patsubst here to prevent confusion of syntax highlighting editors :-)
CONFIG_XARCH	:= $(patsubst "%",%,$(CONFIG_XARCH))
CONFIG_ABI	:= $(patsubst "%",%,$(CONFIG_ABI))

ifeq ("$(CONFIG_XARCH)","")

all: menuconfig
	@echo "========================================================="
	@echo "Now run make again to build!"
	@echo "========================================================="
	@exit 1

else # ! no XARCH
ifeq ("$(CONFIG_ABI)","")
all:
	@echo "========================================================="
	@echo "ERROR: No ABI version set (run 'make menuconfig')!"
	@echo "========================================================="
	@exit 1
else # ! no ABI

# 
# At this point, globalconfig.out is up-to-date.  Update Modules and
# .Modules.deps, then restart using Makefile.sub1, Makefile.sub2.
#

.PRECIOUS: globalconfig.tags

# Read Make configuration
include $(srcdir)/Makeconf

include $(MODULES_FILE)
MODULES_FILES += $(srcdir)/Modules.generic

ifdef SUBSYSTEMS
 _modules_read_ = true
endif

ifdef _modules_read_
GENERATED_MODULES = $(foreach subsys, $(SUBSYSTEMS), \
		      $(INTERFACES_$(subsys)))
ALL = $(foreach subsys, $(SUBSYSTEMS), $($(subsys)) $($(subsys)_EXTRA))

$(foreach m, $(GENERATED_MODULES), auto/stamp-$(m).ready): $(MODULES_FILES)

.PRECIOUS: .Modules.deps
.Modules.deps: $(MODULES_FILES) globalconfig.h globalconfig.tags | auto
	@echo "Cleaning up build directory"
	$(VERBOSE)$(RM_R) *.lds *.o fiasco *.d .*.d .*.d.new *.d.new
	$(VERBOSE)$(RM_R) auto/*.cc auto/*.h auto/*.S auto/stamp-*.ready
	$(VERBOSE)$(RM) dump_tcboffsets
	@echo "Creating $@"
	@($(foreach mod, $(GENERATED_MODULES),				\
	    echo 'auto/stamp-$(mod).ready:				\
		  $(addsuffix .cpp,$(call eval_impl,$(mod)))';		\
	    $(foreach cpp, $(call eval_impl,$(mod)), echo '$(cpp).cpp:';) \
	    echo '$(patsubst %,auto/%.cc,$(call eval_impl,$(mod)))'	\
		 'auto/$(mod).h auto/$(mod)_i.h:			\
		  auto/stamp-$(mod).ready ;				\
	          @[ -e $$@ ] || { $$(RM) $$<; $$(MAKE) $$<; }';	\
	  )) > $@.new
	@($(foreach subsys, $(SUBSYSTEMS),				      \
	    echo 'IFDEPS += $(addprefix ., $(addsuffix .cc.d,		      \
			      $(foreach in,$(INTERFACES_$(subsys)),	      \
				$(call eval_impl,$(in)))))' ;		      \
	    echo 'CXXSRC_$(subsys) += $(addsuffix .cc,			      \
					$(foreach in,$(INTERFACES_$(subsys)), \
					  $(call eval_impl,$(in))))';	      \
	    echo 'OBJ_$(subsys) += $$(CXXSRC_$(subsys):.cc=.o)		      \
		                   $$(CSRC_$(subsys):.c=.o)		      \
		                   $$(ASSRC_$(subsys):.S=.o)' ; ) ) >> $@.new
	@echo "GENERATED_MODULES = $(GENERATED_MODULES)" >> $@.new
	@echo "ALL = $(ALL)" >> $@.new
	@echo "_modules_deps_read_ = true" >> $@.new
	@mv $@.new $@

endif # _modules_read_

# 
# Makefile.sub1: Create source files.
# 

.PHONY: create-sources
create-sources: $(MODULES_FILES) globalconfig.h globalconfig.tags .Modules.deps source
	$(MAKE) srcdir=$(srcdir) objbase=$(objbase) -f $(srcdir)/Makefile.sub1 
auto/stamp-%.ready: $(MODULES_FILES) globalconfig.h globalconfig.tags .Modules.deps 
	$(MAKE) srcdir=$(srcdir) objbase=$(objbase) -f $(srcdir)/Makefile.sub1 $@

DEPS_FILES=DEPS DEPS.a4 DEPS.tred

#
# Makefile.sub2: Create everything else.
#
all doc $(addsuffix .ps,$(DEPS_FILES)) $(addsuffix .svg,$(DEPS_FILES)) TAGS tags compile_commands.json: \
  $(MODULES_FILES) .Modules.deps create-sources globalconfig.h globalconfig.tags
	$(MAKE) srcdir=$(srcdir) objbase=$(objbase) -f $(srcdir)/Makefile.sub2 $@

%.o %_t: $(MODULES_FILES) .Modules.deps create-sources globalconfig.h globalconfig.tags
	$(MAKE) srcdir=$(srcdir) objbase=$(objbase) -f $(srcdir)/Makefile.sub2 $@


# Divert any target we do not explicitly mention in this Makefile to
# Makefile.sub2.  (Unfortunately 1, this does not work for file
# targets that already exist in this directory.  Unfortunately 2,
# .DEFAULT does not accept prerequisites, so we must "make
# create-sources" manually.)
.DEFAULT: 
	$(MAKE) create-sources
	$(MAKE) srcdir=$(srcdir) objbase=$(objbase) -f $(srcdir)/Makefile.sub2 $@

# Well, we need to provide some empty rules for some targets to
# prevent the above catch-all from running amok.
Makerules.local $(srcdir)/Makeconf.local $(objbase)/Makeconf.local \
   $(objbase)/.Host-config: ;

%: %.o				# delete implicit rule

endif # ! no ABI
endif # ! no XARCH
endif # ! config xconfig menuconfig oldconfig

auto:
	@mkdir -p auto

source:
	test -e source || ln -sf $(srcdir) source


BSP_DIR            := $(wildcard $(srcdir)/kern/*/bsp)
KCONFIG_FILE       := Kconfig
KCONFIG_SRC_FILE   := $(srcdir)/Kconfig
KCONFIG_BSP_FILES  := $(shell find $(BSP_DIR) -name Kconfig -follow -print)
KCONFIG_ARCH_FILES := $(wildcard $(srcdir)/kern/*/Kconfig)
KCONFIG_PART_FILES := $(sort $(KCONFIG_ARCH_FILES)) $(sort $(KCONFIG_BSP_FILES))

kconfig_call = $(MAKE) -C $(tooldir)/kconfig O=$(objbase) \
	       Kconfig=$(KCONFIG_FILE) \
	       KCONFIG_AUTOHEADER=globalconfig.h \
	       KCONFIG_TRISTATE=include/config/tristate.conf \
	       KCONFIG_CONFIG=globalconfig.out \
	       KCONFIG_AUTOCONFIG=include/config/auto.conf \
	       MENUCONFIG_COLOR=blackbg \
	       INCLUDE_PPC32=$(INCLUDE_PPC32) \
	       INCLUDE_SPARC=$(INCLUDE_SPARC) \
	       fiasco_srcdir=$(srcdir)/..

update_globalconfig_tags = \
	     @$(tooldir)/config-tags globalconfig.out > globalconfig.tags

# save config file dependencies to detect BSP removals
.$(KCONFIG_FILE).parts: FORCE
	@echo '$(KCONFIG_PART_FILES)' | cmp -s - $@ || echo '$(KCONFIG_PART_FILES)' > $@

$(KCONFIG_FILE): $(KCONFIG_SRC_FILE) $(KCONFIG_PART_FILES) .$(KCONFIG_FILE).parts \
                 $(srcdir)/Makefile
	@$(tooldir)/gen_kconfig $(KCONFIG_SRC_FILE) $(KCONFIG_FILE) \
	                        $(KCONFIG_PART_FILES)

globalconfig.out: $(KCONFIG_FILE)
	+$(VERBOSE)$(kconfig_call) oldconfig

globalconfig.h globalconfig.tags: globalconfig.out
	+$(VERBOSE)$(kconfig_call) syncconfig
	+$(VERBOSE)$(update_globalconfig_tags)

config: $(KCONFIG_FILE)
	+$(VERBOSE)$(kconfig_call) menuconfig
	+$(VERBOSE)$(kconfig_call) syncconfig
	+$(VERBOSE)$(update_globalconfig_tags)

textconfig: $(KCONFIG_FILE)
	+$(VERBOSE)$(kconfig_call) config syncconfig
	+$(VERBOSE)$(update_globalconfig_tags)

menuconfig oldconfig xconfig gconfig nconfig randconfig allyesconfig allnoconfig: $(KCONFIG_FILE)
	+$(VERBOSE)$(kconfig_call) $@
	+$(VERBOSE)$(kconfig_call) syncconfig
	+$(VERBOSE)$(update_globalconfig_tags)

olddefconfig oldnoconfig: $(KCONFIG_FILE)
	+$(VERBOSE)$(kconfig_call) $@
	+$(VERBOSE)$(update_globalconfig_tags)

oldaskconfig: $(KCONFIG_FILE)
	+$(VERBOSE)$(kconfig_call) config
	+$(VERBOSE)$(update_globalconfig_tags)

listnewconfig savedefconfig: $(KCONFIG_FILE)
	+$(VERBOSE)$(kconfig_call) $@

ifneq ($(filter clean cleanall mrproper,$(MAKECMDGOALS)),)

# Try to suck in clean targets from subsystems' Makefile fragments
ifdef _modules_read_
MAKERULES_SUBSYS = $(foreach subsys, $(SUBSYSTEMS), $(firstword $(wildcard $(addsuffix /Makerules.$(subsys),$(addprefix $(srcdir)/,$(VPATH)) $(srcdir)))))
-include $(MAKERULES_SUBSYS)
endif

.DEFAULT:

.PHONY: clean cleanall mrproper \
	$(foreach subsys, $(SUBSYSTEMS), clean-$(subsys)) \
	$(foreach subsys, $(SUBSYSTEMS), cleanall-$(subsys))

clean: $(foreach subsys, $(SUBSYSTEMS), clean-$(subsys))
	$(VERBOSE)$(RM) $(strip $(ALL))
	$(VERBOSE)$(RM) *.o fiasco fiasco.map
	$(VERBOSE)$(RM) auto/*.cc auto/*.h auto/*.S auto/stamp-*.ready
	$(VERBOSE)$(RM) .Clean-auto .$(KCONFIG_FILE).parts

cleanall: clean $(foreach subsys, $(SUBSYSTEMS), cleanall-$(subsys))
	$(foreach subdir, $(SUBDIRS), $(VERBOSE)$(RM) $(subdir)/*.d $(subdir)/.*.d)
	$(VERBOSE)$(RM) *.d .*.d .*.d.new *.d.new *~ *.diag
	$(VERBOSE)$(RM) globalconfig.h globalconfig.tags globalconfig.h.old Circular
	$(VERBOSE)$(RM) .Modules.deps source .Compiler-config
	$(VERBOSE)$(RM_R) scripts

mrproper: cleanall
	$(VERBOSE)$(RM_R) globalconfig.out Modules.* DEPS*
	$(VERBOSE)$(RM_R) auto docs config

endif # clean, cleanall, mrproper

update:
	cd $(srcdir)/.. && svn update
	cd $(dir $(PREPROCESS))/.. && svn update

diag:
	@echo "Collected diagnostics during build:"
	@cat *.diag 2>/dev/null || echo "  None"

endif # srcdir != NOT_SET

endif # MAKECMDGOALS != help


define _ver_fun
	@echo "$(strip $1) $(or $2,-v):"
	@$1 $(or $2,-v) $3 || true
	@echo

endef

define _ver_fun_var
	@echo -n "$1: "
	$(call _ver_fun,$($1),$2,$3)
endef

_ver_fun_vars=$(foreach v,$1,$(call _ver_fun_var,$v,$2,$3))

report:
	$(call _ver_fun,make)
	$(call _ver_fun_vars,CC CXX HOST_CC HOST_CXX,-v, 2>&1)
	$(call _ver_fun_var,LD)

endif
