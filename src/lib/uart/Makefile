include $(srcdir)/Makeconf
include $(srcdir)/Makerules.global

vpath %.cc $(VPATH_LIBUART)

all: $(notdir $(TARGET))

$(OBJECTS): $(objbase)/.Compiler-config

# Allow to use `#ifndef UART_WITHOUT_INPUT..#endif` in UART objects.
$(foreach o,$(basename $(OBJECTS)),\
  $(eval CXXFLAGS_$o += $(if $(CONFIG_INPUT),,-DUART_WITHOUT_INPUT)))

$(notdir $(TARGET)): $(OBJECTS)
	$(AR_MESSAGE)
	$(VERBOSE)$(RM) $@
	$(VERBOSE)$(AR) $(ARFLAGS) $@ $^

-include $(wildcard .*.d)
