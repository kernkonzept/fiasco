srcdir = ../..

all:

%:
	$(MAKE) -I $(srcdir) -I .. -f $(srcdir)/Makefile \
	  srcdir=$(srcdir) MODULES_FILE=Modules.wrappers $@
