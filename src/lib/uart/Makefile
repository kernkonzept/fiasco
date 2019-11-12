include $(srcdir)/Makeconf
include $(srcdir)/Makerules.global

vpath %.cc $(VPATH_LIBUART)

all: $(notdir $(TARGET))

$(OBJECTS): $(objbase)/.Compiler-config

# Allow to use `#ifdef CONFIG_INPUT..#endif` in UART objects.
$(foreach o,$(basename $(OBJECTS)),\
  $(eval CXXFLAGS_$o += -include "$(objbase)/globalconfig.h"))

$(notdir $(TARGET)): $(OBJECTS)
	$(AR_MESSAGE)
	$(VERBOSE)$(RM) $@
	$(VERBOSE)$(AR) $(ARFLAGS) $@ $^

-include $(wildcard .*.d)
