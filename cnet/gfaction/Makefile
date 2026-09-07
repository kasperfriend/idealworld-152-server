
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
DEBUG_VERSION = true
include ../mk/gcc.defs.mk

INCLUDES += -I./operations
LOG_DIR = ../logclient
INCLUDES += -I$(TOP_SRCDIR)/logclient -I$(TOP_SRCDIR)/log_inl -I$(TOP_SRCDIR)/include

DEFINES += -O3 -DUSE_EPOLL -DUSE_LOGCLIENT
DEFINES += -D__USER__=\"$(USER)\"
LDFLAGS += -O3

PCRELIB ?= -lpcre

OUTEROBJS = $(CO_DIR)/matcher.o
CLEAN += $(CO_DIR)/matcher.o

OBJS = gfactionserver.o gproviderserver.o gfactiondbclient.o uniquenameclient.o state.o stubs.o gfaction.o factiondb.o \
		./operations/operwrapper.o ./operations/operation.o ./operations/privilege.o gfs_io.o settings.o 


all : gfactiond

gfactiond : $(OBJS) $(SHAREOBJ) $(OUTEROBJS) $(LOGSTUB) $(LOGOBJ)
	$(LD) $(LDFLAGS) -o $@ $(SHAREOBJ) $(OBJS) $(OUTEROBJS) $(SHARE_SOBJ) $(LOGSTUB) $(LOGOBJ) $(PCRELIB)

include ../mk/gcc.rules.mk

