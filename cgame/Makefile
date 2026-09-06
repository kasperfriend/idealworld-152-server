LIBSUBDIR=io
LIBOBJ = io/pollio.o
SUBDIR=gs 
all: cmlib lib solib collision $(SUBDIR)

# cgame/libcommon.a, built from cgame/common - gs links against it
cmlib: FORCE
	cd common; make

$(SUBDIR): cmlib FORCE
	cd $@; make

lib:	$(LIBSUBDIR) gslib
	rm -f libonline.a
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
