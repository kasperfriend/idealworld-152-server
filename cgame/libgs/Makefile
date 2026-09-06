
LIB_PATH=$(BASEPATH)/libgs

all: FORCE clean
	cd gs;ar x $(IOPATH)/gamed/libgsPro2.a 
	cd io;ar x $(IOPATH)/io/libgsio.a 
	cd db;ar x $(IOPATH)/gdbclient/libdbCli.a 
	cd sk;ar x $(CSKILLPATH)/skill/libskill.a 
	#cd log;ar x $(IOPATH)/liblogCli.a

FORCE:

clean: FORCE
	rm -f *.o;rm -f .lib.o.list
	cd io;rm -f *.o
	cd gs;rm -f *.o
	cd db;rm -f *.o
	cd sk;rm -f *.o
	#cd log;rm -f *.o
	

include ../Rules.make
