## Process this file with automake to produce Makefile.in
#
EXTRA_DIST = cdilib.c $(top_srcdir)/interfaces/f2003/bindGen.rb

noinst_LTLIBRARIES = libcdiresunpack.la

include_HEADERS =
lib_LTLIBRARIES =

if ENABLE_CDI_LIB
  lib_LTLIBRARIES += libcdi.la
  include_HEADERS += cdi.h cdi_datetime.h
# add cdi.inc only if cfortran interface is required
if USE_FC
  include_HEADERS += cdi.inc
endif
if CREATE_ISOC
  lib_LTLIBRARIES += libcdi_f2003.la
endif
else
  noinst_LTLIBRARIES += libcdi.la
if CREATE_ISOC
  noinst_LTLIBRARIES += libcdi_f2003.la
endif
endif

AM_CFLAGS = $(PPM_CORE_CFLAGS) $(YAXT_CFLAGS) $(MPI_C_INCLUDE)

libcdi_la_SOURCES = 	 \
	async_worker.c   \
	async_worker.h	 \
	basetime.c     	 \
	basetime.h	 \
	binary.c	 \
	binary.h	 \
	calendar.c 	 \
	calendar.h	 \
	cdf.c            \
	cdf.h	 	 \
	cdf_config.h	 \
	cdf_int.c	 \
	cdf_int.h	 \
	cdf_util.c	 \
	cdf_util.h	 \
	cdi.h	 	 \
	cdi_datetime.h   \
	cdi_error.c      \
	cdi_limits.h	 \
	cdi_util.c       \
	cgribex.h	 \
	cgribexlib.c  	 \
	cksum.c		 \
	cksum.h		 \
	cdi_cksum.c	 \
	cdi_cksum.h 	 \
	cdi_uuid.h	 \
	datetime.h	 \
	dmemory.c      	 \
	dmemory.h	 \
	dtypes.h	 \
	error.c        	 \
	error.h	 	 \
	exse.h	 	 \
	extra.h	 	 \
	extralib.c       \
	file.c         	 \
	file.h	 	 \
	get_num_missvals.c \
	get_num_missvals.h \
	gaussian_latitudes.c \
	gribapi.c  	 \
	gribapi.h	 \
	gribapi_utilities.h \
	grid.c           \
	grid.h	 	 \
	ieg.h	 	 \
	ieglib.c         \
	input_file.c	 \
	input_file.h     \
	institution.c  	 \
	institution.h  	 \
	julian_date.c  	 \
	julian_date.h  	 \
	model.c        	 \
	model.h        	 \
	namespace.c      \
	namespace.h      \
	serialize.h	\
	serialize.c	\
	referenceCounting.c \
	referenceCounting.h \
	resource_handle.c\
	resource_handle.h\
	service.h	 \
	servicelib.c     \
	stream_cdf_i.c   \
	stream_cdf_o.c   \
	stream_cdf_time.c \
	stream_cdf.h	 \
	stream_cgribex.c \
	stream_cgribex.h \
	stream_ext.c     \
	stream_ext.h	 \
	stream_grb.c     \
	stream_grb.h     \
	stream_gribapi.h \
	stream_ieg.c     \
	stream_ieg.h	 \
	cdi_int.c        \
	cdi_int.h	 \
	cdi_across.c        \
	cdi_across.h	 \
	cdi_fdb.c        \
	cdi_fdb.h	 \
	stream_scan.c    \
	stream_scan.h	 \
	stream_record.c  \
	stream_srv.c     \
	stream_srv.h	 \
	stream_var.c     \
        grb_write.c      \
        grb_read.c       \
        cdf_write.c      \
        cdf_read.c       \
        cdf_lazy_grid.c  \
        cdf_lazy_grid.h  \
        subtype.c        \
        subtype.h        \
	swap.h	 	 \
	table.c        	 \
	table.h	 	 \
	tablepar.h	 \
	taxis.c          \
	taxis.h	         \
	timebase.c 	 \
	timebase.h	 \
	tsteps.c         \
	util.c         	 \
	varscan.c      	 \
	varscan.h        \
	version.c      	 \
	vlist.c 	 \
	vlist.h	         \
	cdi_key.c 	 \
	cdi_key.h 	 \
	cdi_att.c 	 \
	cdi_att.h 	 \
	vlist_var.c 	 \
	vlist_var.h	 \
	vlist_var_pack.c \
	vlist_var_key.c  \
	zaxis.c		 \
	zaxis.h		 \
	stream.c         \
	stream_write.c   \
	stream_read.c    \
	swap.c           \
	iterator.c          \
	iterator.h          \
	iterator_fallback.c \
	iterator_fallback.h \
	iterator_grib.c     \
	iterator_grib.h

# cfortran.h is an optional part of libcdi
libcdi_la_USE_FC_extra_sources = \
	cfortran.h       \
	cdiFortran.c

# these only contain code if grib_api is available
libcdi_la_HAVE_LIBGRIB_API_extra_sources = \
	gribapi_utilities.c stream_gribapi.c

if USE_FC
libcdi_la_SOURCES += 	 \
	$(libcdi_la_USE_FC_extra_sources)
endif

if HAVE_LIBGRIB_API
libcdi_la_SOURCES += 	 \
	$(libcdi_la_HAVE_LIBGRIB_API_extra_sources)
endif

EXTRA_DIST += \
	$(libcdi_la_USE_FC_extra_sources) \
	$(libcdi_la_HAVE_LIBGRIB_API_extra_sources) \
	$(libcdipio_la_HAVE_PARALLEL_NC4_extra_sources)



libcdi_f2003_la_SOURCES = mo_cdi.f90
libcdi_f2003_ladir = $(includedir)
nodist_libcdi_f2003_la_HEADERS =
if CREATE_ISOC
if FORTRAN_MOD_UC
nodist_libcdi_f2003_la_HEADERS += MO_CDI.$(FCMODEXT)
else
nodist_libcdi_f2003_la_HEADERS += mo_cdi.$(FCMODEXT)
endif
endif
libcdi_f2003_la_LIBADD = libcdi.la

libcdiresunpack_la_SOURCES = \
	resource_unpack.c

if USE_MPI
if ENABLE_CDI_LIB
lib_LTLIBRARIES += libcdipio.la
include_HEADERS += cdipio.h cdipio.inc
else
noinst_LTLIBRARIES += libcdipio.la
endif
endif

libcdipio_la_SOURCES =							\
	cdipio.h							\
	cdipioFortran.c							\
	cfortran.h							\
	pio.c								\
	pio.h								\
	pio_comm.c							\
	pio_comm.h							\
	pio_conf.c							\
	pio_conf.h							\
	pio_dbuffer.c							\
	pio_id_set.h							\
	pio_impl.h							\
	pio_interface.c							\
	pio_interface.h							\
	pio_mpinonb.c							\
	pio_mpi_fw_ordered.c						\
	pio_mpi_fw_at_all.c						\
	pio_mpi_fw_at_reblock.c						\
	pio_record_send.c						\
	pio_posixasynch.c						\
	pio_posixfpguardsendrecv.c					\
	pio_posixnonb.c							\
	pio_list_set.c							\
	resource_unpack.h						\
	resource_unpack.c						\
	pio_client.c							\
	pio_client.h							\
	pio_roles.c							\
	pio_rpc.c							\
	pio_rpc.h							\
	pio_server.c							\
	pio_server.h							\
	pio_serialize.h							\
	pio_serialize.c							\
	pio_util.c							\
	pio_util.h							\
	pio_cdf_int.h

libcdipio_la_HAVE_PARALLEL_NC4_extra_sources =				\
	pio_cdf_int.c

if USE_MPI
if HAVE_PARALLEL_NC4
libcdipio_la_SOURCES += $(libcdipio_la_HAVE_PARALLEL_NC4_extra_sources)
endif
endif

libcdipio_la_LIBADD   = libcdi.la $(PPM_CORE_LIBS) $(YAXT_LIBS) $(MPI_C_LIB)

#
#cdilib.c:
#	$(top_srcdir)/src/make_cdilib $(top_srcdir)/src
#
#cdilib.o: cdilib.c
#	$(COMPILE) -c $<

LOCALTARGETS  =
if ENABLE_CDI_LIB
LOCALTARGETS += cmake/libcdi/cdi-config.cmake
LOCALTARGETS += cmake/libcdi/cdi-config-version.cmake
LOCALTARGETS += pkgconfig/cdi.pc
if USE_MPI
LOCALTARGETS += pkgconfig/cdipio.pc
endif
if CREATE_ISOC
LOCALTARGETS += pkgconfig/cdi_f2003.pc
endif
endif

#
#	cat $(top_srcdir)/src/cdi.h |sed -e 's/SizeType/size_t/g' -e s/DateType/int64_t/g > cditmp.h
mo_cdi.f90: @MAINTAINER_MODE_TRUE@ $(top_srcdir)/src/cdi.h $(top_srcdir)/interfaces/f2003/bindGen.rb
	cat $(top_srcdir)/src/cdi.h |sed -e 's/SizeType/int/g' -e s/DateType/int/g > cditmp.h
	$(AM_V_GEN)$(RUBY) $(top_srcdir)/interfaces/f2003/bindGen.rb cditmp.h $@
	rm cditmp.h

if FORTRAN_MOD_UC
MO_CDI.$(FCMODEXT): mo_cdi.lo
else
mo_cdi.$(FCMODEXT): mo_cdi.lo
endif
	@if test ! -f $@; then rm -f $<; $(MAKE) $<; fi
#
cmake/libcdi/cdi-config.cmake: cmake/libcdi/cdi-config.cmake.in ../config.status
	(cd .. ; ./config.status src/cmake/libcdi/cdi-config.cmake)

cmake/libcdi/cdi-config-version.cmake: cmake/libcdi/cdi-config-version.cmake.in ../config.status
	(cd .. ; ./config.status src/cmake/libcdi/cdi-config-version.cmake)

pkgconfig/cdi.pc: pkgconfig/cdi.pc.in ../config.status
	(cd .. ; ./config.status src/pkgconfig/cdi.pc)

pkgconfig/cdipio.pc: pkgconfig/cdipio.pc.in ../config.status
	(cd .. ; ./config.status src/pkgconfig/cdipio.pc)

pkgconfig/cdi_f2003.pc: pkgconfig/cdi_f2003.pc.in ../config.status
	(cd .. ; ./config.status src/pkgconfig/cdi_f2003.pc)

cdilib.c: 
	(cd $(top_srcdir)/src ; ./make_cdilib )
#
all-local: $(LOCALTARGETS)
#
CLEANFILES  := `ls *~`
#CLEANFILES += cdilib.c
if CREATE_ISOC
if FORTRAN_MOD_UC
CLEANFILES += MO_CDI.$(FCMODEXT)
else
CLEANFILES += mo_cdi.$(FCMODEXT)
endif
endif

if ENABLE_CDI_LIB
CMAKECONFIG_FILES = cmake/libcdi/cdi-config.cmake
CLEANFILES += cmake/libcdi/cdi-config.cmake
CMAKECONFIG_FILES += cmake/libcdi/cdi-config-version.cmake
CLEANFILES += cmake/libcdi/cdi-config-version.cmake
PKGCONFIG_FILES = pkgconfig/cdi.pc
CLEANFILES += pkgconfig/cdi.pc
if USE_MPI
PKGCONFIG_FILES += pkgconfig/cdipio.pc
CLEANFILES += pkgconfig/cdipio.pc
endif
if CREATE_ISOC
PKGCONFIG_FILES += pkgconfig/cdi_f2003.pc
CLEANFILES += pkgconfig/cdi_f2003.pc
endif

install-exec-local: $(PKGCONFIG_FILES) $(CMAKECONFIG_FILES)
	$(mkinstalldirs) "$(DESTDIR)$(libdir)/pkgconfig"
	@for pkgconfigfile in $(PKGCONFIG_FILES) ; do \
		echo $(install_sh_DATA) $$pkgconfigfile "$(DESTDIR)$(libdir)/$$pkgconfigfile" ; \
		$(install_sh_DATA) $$pkgconfigfile "$(DESTDIR)$(libdir)/$$pkgconfigfile" ; \
	done
	$(mkinstalldirs) "$(DESTDIR)$(libdir)/cmake"
	@for cmakeconfigfile in $(CMAKECONFIG_FILES) ; do \
		echo $(install_sh_DATA) $$cmakeconfigfile "$(DESTDIR)$(libdir)/$$cmakeconfigfile" ; \
		$(install_sh_DATA) $$cmakeconfigfile "$(DESTDIR)$(libdir)/$$cmakeconfigfile" ; \
	done

uninstall-local:
	@for pkgconfigfile in $(PKGCONFIG_FILES) ; do \
		echo rm -f "$(DESTDIR)$(libdir)/$$pkgconfigfile" ; \
		rm -f "$(DESTDIR)$(libdir)/$$pkgconfigfile" ; \
	done
	-@rmdir "$(DESTDIR)$(libdir)/pkgconfig"
	@for cmakeconfigfile in $(CMAKECONFIG_FILES) ; do \
		echo rm -f "$(DESTDIR)$(libdir)/$$cmakeconfigfile" ; \
		rm -f "$(DESTDIR)$(libdir)/$$cmakeconfigfile" ; \
	done
	-@rmdir "$(DESTDIR)$(libdir)/cmake"

endif

install-exec-hook:
	-@rmdir "$(DESTDIR)$(libdir)"
install-data-hook:
	-@rmdir "$(DESTDIR)$(includedir)"

