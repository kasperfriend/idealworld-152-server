
LIB_PATH=$(BASEPATH)/libgs

all: FORCE clean
	@mkdir -p gs io db sk log
	cd gs;ar x $(IOPATH)/gamed/libgsPro2.a 
	cd io;ar x $(IOPATH)/io/libgsio.a 
	cd db;ar x $(IOPATH)/gdbclient/libdbCli.a 
	cd sk;ar x $(CSKILLPATH)/skill/libskill.a 
	cd log;ar x $(IOPATH)/logclient/liblogCli.a

FORCE:

clean: FORCE
	@mkdir -p gs io db sk log
	rm -f *.o;rm -f .lib.o.list
	cd io;rm -f *.o
	cd gs;rm -f *.o
	cd db;rm -f *.o
	cd sk;rm -f *.o
	cd log;rm -f *.o
	

include ../Rules.make
