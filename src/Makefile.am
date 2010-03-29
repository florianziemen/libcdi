## Process this file with automake to produce Makefile.in
#
EXTRA_DIST = make_cdilib mo_cdi.f90
#
#BUILT_SOURCES = cdilib.c
#
lib_LIBRARIES = libcdi.a
#
include_HEADERS = cdi.h cdi.inc
#
nodistdir = .
nodist_include_HEADERS =
if CREATE_ISOC
  nodist_include_HEADERS += mo_cdi.$(FCMODEXT)
endif
#
libcdi_a_SOURCES = 	 \
        cdiFortran.c   	 \
	cdi_error.c      \
	cdi_util.c       \
	taxis.c          \
	error.c        	 \
	version.c      	 \
	dmemory.c      	 \
	model.c        	 \
	institution.c  	 \
	table.c        	 \
	gaussgrid.c      \
	util.c         	 \
	varscan.c      	 \
	calendar.c 	 \
	timebase.c 	 \
	vlist.c 	 \
	vlist_var.c 	 \
	vlist_att.c 	 \
	basetime.c     	 \
	stream_history.c \
	stream_cgribex.c \
	stream_gribapi.c \
	stream_grb.c     \
	stream_cdf.c     \
	stream_srv.c     \
	stream_ext.c     \
	stream_ieg.c     \
	grid.c           \
	zaxis.c		 \
        stream.c         \
	stream_var.c     \
	stream_record.c  \
	tsteps.c         \
	stream_int.c     \
	servicelib.c     \
	extralib.c       \
	ieglib.c         \
	cdf.c            \
	cdf_int.c	 \
	file.c         	 \
	binary.c	 \
        swap.c       	 \
	cgribexlib.c  	 \
	gribapi.c  	 \
	cfortran.h       \
	binary.h	 \
	cdf.h	 	 \
	cdf_int.h	 \
	datetime.h	 \
	dmemory.h	 \
	dtypes.h	 \
	error.h	 	 \
	extra.h	 	 \
	ieg.h	 	 \
	file.h	 	 \
	cdi.h	 	 \
	cdi_limits.h	 \
	calendar.h	 \
	timebase.h	 \
	vlist.h	         \
	taxis.h	         \
	stream_cgribex.h \
	stream_gribapi.h \
	stream_grb.h     \
	stream_cdf.h	 \
	stream_srv.h	 \
	stream_ext.h	 \
	stream_ieg.h	 \
	stream_int.h	 \
	basetime.h	 \
	cgribex.h	 \
	gribapi.h	 \
	grid.h	 	 \
	service.h	 \
	swap.h	 	 \
	table.h	 	 \
	tablepar.h	 \
	gaussgrid.h	 \
	varscan.h        \
	pio_dbuffer.c    \
	pio_queue.c      \
	pio.c            \
	pio_posixasynch.c\
	pio_posixfpguardsendrecv.c \
	pio.h            \
	pio_impl.h 
#
cdiFortran.o: cdiFortran.c
	source='$<' object='$@' libtool=no \
	depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' \
	$(CCDEPMODE) $(depcomp) \
	$(COMPILE) $(CFINT) $(REAL4) -c `test -f '$<' || echo '$(srcdir)/'`$<
#
cdilib.c:
	$(top_srcdir)/src/make_cdilib $(top_srcdir)/src
#
cdilib.o: cdilib.c
	$(COMPILE) $(CFINT) $(REAL4) -c $<
#
all-local: $(LOCALTARGETS) 

LOCALTARGETS = cdilib.o

if CREATE_ISOC
LOCALTARGETS += mo_cdi.o
endif
#
install-data-local: $(LOCALTARGETS)
	 @for f in $(LOCALTARGETS) ; do \
	   echo " $(INSTALL_DATA) $$f \"$(DESTDIR)$(libdir)/$$f\""; \
	   $(INSTALL_DATA) $$f "$(DESTDIR)$(libdir)/$$f" ; \
	 done
#
mo_cdi.o: mo_cdi.f90
	$(FC) $(FCFLAGS) -c $(FCFLAGS_f90) mo_cdi.f90
#
mo_cdi.$(FCMODEXT): mo_cdi.f90
	$(FC) $(FCFLAGS) -c $(FCFLAGS_f90) mo_cdi.f90
#
CLEANFILES  = `ls *~`
CLEANFILES += cdilib.c
if CREATE_ISOC
CLEANFILES += mo_cdi.$(FCMODEXT)
endif
