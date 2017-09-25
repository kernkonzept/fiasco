#
# Makefile for the preprocess sources
#

# scan the L4 global configuration file
L4DIR	?= ../../..
include $(L4DIR)/mk/Makeconf

-include Makeconf.local

TARGET = preprocess

all:: $(TARGET)

install:: all
	-$(INSTALL) -d $(DROPS_STDDIR)/tool/bin
	$(INSTALL) -m 755 $(TARGET) $(DROPS_STDDIR)/tool/bin

clean:: 
#	$(RM) $(TARGET) $(OBJS) tmp.*

cleanall::      clean
	$(RM) *~ .*.d

#include $(DEPS)
