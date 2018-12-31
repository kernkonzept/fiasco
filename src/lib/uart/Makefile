include $(srcdir)/Makeconf
include $(srcdir)/Makerules.global

vpath %.cc $(VPATH_LIBUART)

all: $(notdir $(TARGET))

$(OBJECTS): $(objbase)/.Compiler-config

$(notdir $(TARGET)): $(OBJECTS)
	$(AR_MESSAGE)
	$(VERBOSE)$(RM) $@
	$(VERBOSE)$(AR) $(ARFLAGS) $@ $^

-include $(wildcard .*.d)
