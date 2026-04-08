
include_HEADERS = cdipio.h
if ENABLE_CDI_LIB
lib_LTLIBRARIES = libcdipio.la
if ENABLE_MPI
if ENABLE_CF_INTERFACE
include_HEADERS += cdipio.inc
endif ENABLE_CF_INTERFACE
endif ENABLE_MPI
else !ENABLE_CDI_LIB
if ENABLE_MPI
noinst_LTLIBRARIES = libcdipio.la
endif ENABLE_MPI
endif !ENABLE_CDI_LIB

nodist_pkgconfig_DATA = pkgconfig/cdipio.pc
pkgconfigdir = $(libdir)/pkgconfig

libcdipio_la_SOURCES = \
					   cdipio.h \
					   pio.c \
					   pio.h \
					   pio_cdf_int.h \
					   pio_client.c \
					   pio_client.h \
					   pio_comm.c \
					   pio_comm.h \
					   pio_conf.c \
					   pio_conf.h \
					   pio_dbuffer.c \
					   pio_dbuffer.h \
					   pio_dist_grid.c \
					   pio_dist_grid.h \
					   pio_id_set.h \
					   pio_idxlist_cache.c \
					   pio_idxlist_cache.h \
					   pio_impl.h \
					   pio_interface.c \
					   pio_interface.h \
					   pio_mpi_fw_at_all.c \
					   pio_mpi_fw_at_reblock.c \
					   pio_mpi_fw_ordered.c \
					   pio_mpinonb.c \
					   pio_posixasynch.c \
					   pio_posixfpguardsendrecv.c \
					   pio_posixnonb.c \
					   pio_record_send.c \
					   pio_roles.c \
					   pio_rpc.c \
					   pio_rpc.h \
					   pio_serialize.c \
					   pio_serialize.h \
					   pio_server.c \
					   pio_server.h \
					   pio_util.c \
					   pio_util.h \
					   pio_xmap_cache.c \
					   pio_xmap_cache.h 
		

if ENABLE_CF_INTERFACE
libcdipio_la_SOURCES += \
						cdipioFortran.c \
						cfortran.h
endif ENABLE_CF_INTERFACE

if HAVE_PARALLEL_NC4
libcdipio_la_SOURCES += pio_cdf_int.c
endif HAVE_PARALLEL_NC4

libcdipio_la_LIBADD = $(top_builddir)/src/libcdi.la $(top_builddir)/src/libcdiresunpack.la $(LIBRT) $(PPM_CORE_C_LIB) $(YAXT_C_LIB) $(MPI_C_LIB)

AM_CPPFLAGS = $(PPM_CORE_C_INCLUDE) $(YAXT_C_INCLUDE) $(MPI_C_INCLUDE) -I$(top_srcdir)/src
if ENABLE_MPI
# TODO: this needs a refactoring but for now we set this macro here and not in
# config.h because the macro is used in several files that do not include the
# header file directly.
AM_CPPFLAGS += -DUSE_MPI
endif


if MAINTAINER_MODE

cdipio.inc: cdipio.h $(top_srcdir)/src/make_fint.c $(top_srcdir)/src/cdi.inc
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) AM_CFLAGS=-static $(top_builddir)/src/make_fint
	$(AM_V_GEN)$(top_builddir)/libtool --mode=execute ./$(top_builddir)/src/make_fint -o $(@D) $<

cdipioFortran.c: cdipio.inc
	$(AM_V_at)test -f $@ || rm -f $<
	$(AM_V_at)test -f $@ || $(MAKE) $(AM_MAKEFLAGS) $<

endif MAINTAINER_MODE

pkgconfig/cdipio.pc: $(top_builddir)/config.status $(srcdir)/pkgconfig/cdipio.pc.in
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@


MAINTAINERCLEANFILES = \
					   cdipio.inc \
					   cdipioFortran.c

include $(top_srcdir)/config/lt_overrides_Makefile.inc
