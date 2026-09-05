IOPATH=/pwsrc/antihype152V127/cnet
BASEPATH=/pwsrc/antihype152V127/cgame

INC=-I$(BASEPATH)/include -I$(BASEPATH) -I$(IOPATH)/inc
IOLIB_OBJ=$(BASEPATH)/libgs/gs/*.o $(BASEPATH)/libgs/io/*.o $(BASEPATH)/libgs/db/*.o /pwsrc/antihype152V127/cskill/skill/*.o /pwsrc/antihype152V127/cskill/skills/*.o $(BASEPATH)/libgs/log/*.o
CMLIB=$(BASEPATH)/libcommon.a $(BASEPATH)/libonline.a $(IOLIB_OBJ) $(BASEPATH)/collision/libTrace.a
DEF = -DLINUX -D_DEBUG  -D__THREAD_SPIN_LOCK__ 
#DEF += -D_CHECK_MEM_ALLOC
#DEF += -D__USE_ICPC__  
#DEF += -D__TEST_PERFORMANCE__
DEF += -D__USER__=\"AntiHypeTeam\"

THREAD = -D_REENTRANT -D_THREAD_SAFE 
THREADLIB = -pthread  
PCRELIB = -lpcre
ALLLIB = $(THREADLIB) $(PCRELIB) /usr/lib/libcrypto.a
CFLAGS  = -Wall #-pipe
CPPFLAGS = -Wall #-pipe
OPTIMIZE = -O0
#-O2 -ipo
CC=gcc   $(DEF) $(OPTIMIZE) $(THREAD) $(CFLAGS) -g -ggdb
CPP=g++ $(DEF) $(OPTIMIZE) $(THREAD) $(CPPFLAGS) -g -ggdb
#-pedantic
LD=g++ -g  $(OPTIMIZE) $(THREADLIB) 
AR=ar crs 
ARX=ar x

#
# include dependency files if they exist
#

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

