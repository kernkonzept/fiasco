include $(srcdir)/Makeconf
include $(srcdir)/Makerules.global

vpath %.cc $(VPATH_LIBUART)

all: $(notdir $(TARGET))($(OBJECTS))

$(OBJECTS): $(objbase)/.Compiler-config

$(notdir $(TARGET)): $(OBJECTS)

-include $(wildcard .*.d)
