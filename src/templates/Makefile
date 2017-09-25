# sample configurations

SAMPLE_CONFIGS := $(subst globalconfig.out.,,$(wildcard globalconfig.out.*))

srcdir = $(shell pwd)/../

all:

runconfig = export INCLUDE_PPC32=y;                      \
	    export INCLUDE_SPARC=y;                      \
            make $(1)

copyin   = cp globalconfig.out.$(1) globalconfig.out
#copyback = sed -e '3,4d' $(1) > globalconfig.out.$(2)
copyback = cp $(1) globalconfig.out.$(2)

cleanup = make mrproper && \
	  $(RM) -r globalconfig.out.old globalconfig.h \
	           defconfig Kconfig include

testconfig:
	if [ "$$T" != "" ]; then                          \
	   $(call copyin,$$T);                            \
	   $(call runconfig,menuconfig,$$T);              \
	   $(call copyback,globalconfig.out,$$T);         \
	else echo "use: make T=configname testconfig"; fi
	$(call cleanup)

refresh:
	for cfg in $(SAMPLE_CONFIGS); do                       \
	  echo "********************************************"; \
	  echo "** $$cfg **";                                  \
	  echo "********************************************"; \
	  $(call copyin,$$cfg);                                \
	  $(call runconfig,olddefconfig,$$cfg);                   \
	  $(call runconfig,savedefconfig,$$cfg);               \
	  $(call copyback,defconfig,$$cfg);                    \
	done
	$(call cleanup)

%:
	$(MAKE) -f $(srcdir)/Makefile \
	  srcdir=$(srcdir) objbase=$(shell pwd) "$@"


.NOTPARALLEL:
.PHONY: refresh testconfig

