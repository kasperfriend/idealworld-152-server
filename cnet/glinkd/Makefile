
TOP_SRCDIR = ..

# SINGLE_THREAD used to be "true" here.  That mode cannot be built from
# source any more: without -D_REENTRANT_ cnet/common/thread.cpp does not
# define the ONET/GNET thread pool statics and cnet/io/pollio.cpp does not
# define PollControl::writer, both of which the shared logclient code
# (cnet/logclient/log.cpp) references unconditionally - the link only worked
# back then because the checked in objects had been compiled with
# -D_REENTRANT_ anyway.  The other daemons (gauthd, gamedbd, gamed) already
# build this way.
SINGLE_THREAD = false
DEBUG_VERSION = false

include ../mk/gcc.defs.mk
LOG_DIR = ../logclient
INCLUDES += -I$(TOP_SRCDIR)/logclient -I$(TOP_SRCDIR)/log_inl -I$(TOP_SRCDIR)/include
DEFINES += -DUSE_HASH_MAP -DUSE_EPOLL -DUSE_LOGCLIENT -O3 
DEFINES += -D__USER__=\"$(USER)\"
LDFLAGS += -DUSE_HASH_MAP -O3 

OBJS = glinkserver.o gdeliveryclient.o gproviderserver.o state.o stubs.o glinkd.o gfactionclient.o

all : glinkd

glinkd : $(SHAREOBJ) $(OBJS) $(SHARE_SOBJ)  $(LOGSTUB) $(LOGOBJ)
	$(LD) -s $(LDFLAGS) -o $@ $(SHAREOBJ) $(OBJS) $(SHARE_SOBJ) $(LOGSTUB) $(LOGOBJ)

include ../mk/gcc.rules.mk

