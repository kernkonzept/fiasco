# -*- makefile -*-
# Boot Makefile

# Add future prerequisites somewhere among the list of libraries.
# You really should not have to add anything in the $(LD) line.

include $(srcdir)/boot/$(CONFIG_XARCH)/Makerules.BOOT.$(CONFIG_XARCH)

clean-BOOT:
		$(VERBOSE)$(RM) kernel kernel.o $(BOOT_CLEAN)

cleanall-BOOT:
		$(VERBOSE)$(RM) fiasco
