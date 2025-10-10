# -*- makefile -*-
# Boot Makefile

include $(srcdir)/boot/$(CONFIG_XARCH)/Makerules.BOOT.$(CONFIG_XARCH)

clean-BOOT:
		$(VERBOSE)$(RM) kernel kernel.o $(BOOT_CLEAN)

cleanall-BOOT:
		$(VERBOSE)$(RM) fiasco
