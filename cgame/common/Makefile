# cmlib (abase / ONET) support library.
#
# cgame/libcommon.a used to ship as a prebuilt 32 bit archive.  gs links
# against it for a handful of primitives (asserts, crc, spinlocks, the timer
# wheel, the config reader, the worker pool and the small object allocator),
# so the archive is rebuilt from the sources in this directory - see the
# comment in each file for the contract it implements.

CMOBJ = ASSERT.o verbose.o crc.o base64.o interlocked.o spinlock.o \
	strtok.o arandomgen.o amemory.o timer.o conf.o threadpool.o

all:	$(CMOBJ)
	rm -f ../libcommon.a
	$(AR) ../libcommon.a $(CMOBJ)

.c.o:
	$(CC) -c $(INC) $< -o $@

.cc.o:
	$(CPP) -c $(INC) $< -o $@

.cxx.o:
	$(CPP) -c $(INC) $< -o $@

.cpp.o:
	$(CPP) -c $(INC) $< -o $@

# cmtest needs a main() of its own, it is not part of the archive
CMTEST = cmtest.o
CMTESTLIBS = -lpthread

test:	$(CMOBJ) $(CMTEST)
	rm -f cmtest
	$(LD) $(CMTEST) ../libcommon.a $(CMTESTLIBS) -o cmtest
	./cmtest

cmtest:	all

FORCE:

clean:	FORCE
	rm -f *.o ../libcommon.a cmtest cmtest.conf cmtest2.conf .depend

depend: FORCE
	make dep;

include ../Rules.make
