
TOP_SRCDIR = ..

SINGLE_THREAD = false 
DEBUG_VERSION = false

include ../mk/gcc.defs.mk

OBJS = gauthserver.o state.o stubs.o gauthd.o gauthd_clients.o
DEFINES += -DUSE_EPOLL -ggdb

# stubs.cxx includes the generated proxy glue, which reuses the client class
# declarations kept with gdeliveryd; gauthd_clients.cpp provides the matching
# (inert) definitions so gauthd does not have to link gdeliveryd.
INCLUDES += -I$(TOP_SRCDIR)/gdeliveryd -I$(TOP_SRCDIR)/include

all : gauthd

gauthd : $(SHAREOBJ) $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(SHAREOBJ) $(OBJS) $(SHARE_SOBJ)

include ../mk/gcc.rules.mk
