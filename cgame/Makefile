LIBSUBDIR=io
LIBOBJ = io/pollio.o
SUBDIR=gs 
all: lib solib collision $(SUBDIR)

$(SUBDIR): FORCE
	cd $@; make

lib:	$(LIBSUBDIR) gslib
	ar crs libonline.a $(LIBOBJ) 

collision: FORCE
	cd collision; make
	
solib: FORCE
	cd gs; make solib

gslib:	FORCE
	cd libgs;make
	
$(LIBSUBDIR): FORCE
	cd $@; make 
FORCE:

include Rules.make
