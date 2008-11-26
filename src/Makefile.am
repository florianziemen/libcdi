## Process this file with automake to produce Makefile.in
#
lib_LIBRARIES = libcdi.a
#
include_HEADERS = cdi.h cdi.inc
#
libcdi_a_SOURCES = 	 \
        cdiFortran.c   	 \
	cdi_error.c      \
	taxis.c          \
	error.c        	 \
	version.c      	 \
	dmemory.c      	 \
	model.c        	 \
	institution.c  	 \
	table.c        	 \
	util.c         	 \
	varscan.c      	 \
	calendar.c 	 \
	timebase.c 	 \
	vlist.c 	 \
	vlist_var.c 	 \
	vlist_att.c 	 \
	basetime.c     	 \
	stream_history.c \
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
	griblib.c  	 \
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
	stream_grb.h	 \
	stream_cdf.h	 \
	stream_srv.h	 \
	stream_ext.h	 \
	stream_ieg.h	 \
	stream_int.h	 \
	basetime.h	 \
	grib.h	 	 \
	grid.h	 	 \
	service.h	 \
	swap.h	 	 \
	table.h	 	 \
	tablepar.h	 \
	util.h	 	 \
	varscan.h
#
cdiFortran.o: cdiFortran.c
	source='$<' object='$@' libtool=no \
	depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' \
	$(CCDEPMODE) $(depcomp) \
	$(COMPILE) $(CFINT) $(REAL4) -c `test -f '$<' || echo '$(srcdir)/'`$<
#
CLEANFILES  = `ls *~`
