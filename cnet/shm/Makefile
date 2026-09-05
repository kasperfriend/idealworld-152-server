
TOP_SRCDIR = ..

SINGLE_THREAD = false
DEBUG_VERSION = true

include ../mk/gcc.defs.mk
#INCLUDES += 
LDFLAGS  += -w  -O0 -g -ggdb -D_SHM_32_
DEFINES += -DUSE_BDB -DUSE_WDB -DMPPC_4WAY -DUSE_TRANSACTION -D_FILE_OFFSET_BITS=64 -D_SHM_32_
CFLAGS += -w -g -ggdb -O0

OBJS = kmallocator.o kmalloc.o shmmgrimpl.o  ptmalloctor.o ptmalloc.o lock.o 
CLEAN += test libshmmgr.a test.o shminit.o

all : libshmmgr.a test

libshmmgr.a: $(OBJS) shminit.o

test: $(OBJS) test.o
	$(LD) $(LDFLAGS) -o $@ $(OBJS) test.o

include ../mk/gcc.rules.mk

