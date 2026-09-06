# Portable Rules.make - auto-detects repo layout
RULES_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BASEPATH := $(abspath $(RULES_DIR))
IOPATH := $(abspath $(BASEPATH)/../cnet)
CSKILLPATH := $(abspath $(BASEPATH)/../cskill)

INC=-I$(BASEPATH)/include -I$(BASEPATH) -I$(IOPATH)/inc -I$(IOPATH) -I$(CSKILLPATH)
IOLIB_OBJ=$(BASEPATH)/libgs/gs/*.o $(BASEPATH)/libgs/io/*.o $(BASEPATH)/libgs/db/*.o $(CSKILLPATH)/skill/*.o $(CSKILLPATH)/skills/*.o $(BASEPATH)/libgs/log/*.o
CMLIB=$(BASEPATH)/libcommon.a $(BASEPATH)/libonline.a $(IOLIB_OBJ) $(BASEPATH)/collision/libTrace.a
DEF = -DLINUX -D_DEBUG  -D__THREAD_SPIN_LOCK__
DEF += -D__USER__=\"AntiHypeTeam\"

THREAD = -D_REENTRANT -D_THREAD_SAFE
THREADLIB = -pthread
PCRELIB = -lpcre
ALLLIB = $(THREADLIB) $(PCRELIB) -lcrypto
CFLAGS  = -Wall -fpermissive -Wno-narrowing -Wno-deprecated-declarations -include cstring -include cstdio -include cstdlib -include cstdint -include iconv.h -include climits -include ctime
CPPFLAGS = -Wall -fpermissive -Wno-narrowing -Wno-deprecated-declarations -include cstring -include cstdio -include cstdlib -include cstdint -include iconv.h -include climits -include ctime
OPTIMIZE = -O0
CC=gcc   $(DEF) $(OPTIMIZE) $(THREAD) $(CFLAGS) -g -ggdb -m64
CPP=g++ $(DEF) $(OPTIMIZE) $(THREAD) $(CPPFLAGS) -g -ggdb -m64
LD=g++ -g  $(OPTIMIZE) $(THREADLIB) -m64
AR=ar crs
ARX=ar x

ifneq ($(wildcard .depend),)
include .depend
endif

ifeq ($(TERM),cygwin)
THREADLIB = -lpthread
CMLIB += /usr/lib/libgmon.a
DEF += -D__CYGWIN__
endif

dep:
	$(CC) -MM $(INC)  -c *.c* > .depend
