libcdi_f2003_ladir = $(includedir)

include_HEADERS =
nodist_libcdi_f2003_la_HEADERS =
lib_LTLIBRARIES =
noinst_LTLIBRARIES = libcdiresunpack.la
LOCALTARGETS  =
PKGCONFIG_FILES =

if FORTRAN_MOD_UC
mo_cdi_modfile = MO_CDI.$(FCMODEXT)
else
mo_cdi_modfile = mo_cdi.$(FCMODEXT)
endif

if ENABLE_CDI_LIB
include_HEADERS += cdi.h
lib_LTLIBRARIES += libcdi.la
LOCALTARGETS += pkgconfig/cdi.pc
PKGCONFIG_FILES += pkgconfig/cdi.pc
# add cdi.inc only if cfortran interface is required
if USE_FC
include_HEADERS += cdi.inc
endif
if CREATE_ISOC
nodist_libcdi_f2003_la_HEADERS += $(mo_cdi_modfile)
lib_LTLIBRARIES += libcdi_f2003.la
LOCALTARGETS += pkgconfig/cdi_f2003.pc
PKGCONFIG_FILES += pkgconfig/cdi_f2003.pc
endif
if USE_MPI
include_HEADERS += cdipio.h cdipio.inc
lib_LTLIBRARIES += libcdipio.la
LOCALTARGETS += pkgconfig/cdipio.pc
PKGCONFIG_FILES += pkgconfig/cdipio.pc
endif
else
noinst_LTLIBRARIES += libcdi.la
if CREATE_ISOC
noinst_LTLIBRARIES += libcdi_f2003.la
endif
if USE_MPI
noinst_LTLIBRARIES += libcdipio.la
endif
endif

AM_CFLAGS = $(PPM_CORE_C_INCLUDE) $(YAXT_C_INCLUDE) $(MPI_C_INCLUDE)

libcdi_la_SOURCES = \
  basetime.c \
  basetime.h \
  binary.c \
  binary.h \
  calendar.c \
  calendar.h \
  cdf.c \
  cdf.h \
  cdf_int.c \
  cdf_int.h \
  cdf_lazy_grid.c \
  cdf_lazy_grid.h \
  cdf_read.c \
  cdf_util.c \
  cdf_util.h \
  cdf_write.c \
  cdi.h \
  cdi_att.c \
  cdi_att.h \
  cdi_cksum.c \
  cdi_cksum.h \
  cdi_error.c \
  cdi_int.c \
  cdi_int.h \
  cdi_limits.h \
  cdi_util.c \
  cdi_uuid.h \
  cgribex.h \
  cgribexlib.c \
  cksum.c \
  cksum.h \
  datetime.h \
  dmemory.c \
  dmemory.h \
  dtypes.h \
  error.c \
  error.h \
  exse.h \
  extra.h \
  extralib.c \
  file.c \
  file.h \
  gaussgrid.c \
  gaussgrid.h \
  grb_read.c \
  grb_write.c \
  gribapi.c \
  gribapi.h \
  gribapi_utilities.h \
  grid.c \
  grid.h \
  ieg.h \
  ieglib.c \
  input_file.c \
  input_file.h \
  institution.c \
  institution.h \
  iterator.c \
  iterator.h \
  iterator_fallback.c \
  iterator_fallback.h \
  iterator_grib.c \
  iterator_grib.h \
  model.c \
  model.h \
  namespace.c \
  namespace.h \
  referenceCounting.c \
  referenceCounting.h \
  resource_handle.c \
  resource_handle.h \
  serialize.c \
  serialize.h \
  service.h \
  servicelib.c \
  stream.c \
  stream_cdf.h \
  stream_cdf_i.c \
  stream_cdf_o.c \
  stream_cdf_postdef.h \
  stream_cdf_time.c \
  stream_cgribex.c \
  stream_cgribex.h \
  stream_ext.c \
  stream_ext.h \
  stream_grb.c \
  stream_grb.h \
  stream_gribapi.h \
  stream_history.c \
  stream_ieg.c \
  stream_ieg.h \
  stream_read.c \
  stream_record.c \
  stream_srv.c \
  stream_srv.h \
  stream_var.c \
  stream_write.c \
  subtype.c \
  subtype.h \
  swap.c \
  swap.h \
  table.c \
  table.h \
  tablepar.h \
  taxis.c \
  taxis.h \
  timebase.c \
  timebase.h \
  tsteps.c \
  util.c \
  varscan.c \
  varscan.h \
  version.c \
  vlist.c \
  vlist.h \
  vlist_var.c \
  vlist_var.h \
  zaxis.c \
  zaxis.h

# cfortran.h is an optional part of libcdi
libcdi_la_USE_FC_extra_sources = \
  cdiFortran.c \
  cfortran.h

if USE_FC
libcdi_la_SOURCES += \
  $(libcdi_la_USE_FC_extra_sources)
endif

# these only contain code if grib_api is available
libcdi_la_HAVE_LIBGRIB_API_extra_sources = \
  gribapi_utilities.c \
  stream_gribapi.c

if HAVE_LIBGRIB_API
libcdi_la_SOURCES += \
  $(libcdi_la_HAVE_LIBGRIB_API_extra_sources)
endif

libcdi_la_LIBADD = $(UUID_C_LIB)

libcdipio_la_SOURCES = \
  cdipio.h \
  cdipioFortran.c \
  cfortran.h \
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
  pio_xmap_cache.h \
  resource_unpack.c \
  resource_unpack.h

libcdipio_la_HAVE_PARALLEL_NC4_extra_sources = pio_cdf_int.c

if USE_MPI
if HAVE_PARALLEL_NC4
libcdipio_la_SOURCES += $(libcdipio_la_HAVE_PARALLEL_NC4_extra_sources)
endif
endif

libcdipio_la_LIBADD = libcdi.la $(LIBRT) $(PPM_CORE_C_LIB) $(YAXT_C_LIB) $(MPI_C_LIB)

libcdi_f2003_la_SOURCES = mo_cdi.f90
libcdi_f2003_la_LIBADD = libcdi.la

libcdiresunpack_la_SOURCES = resource_unpack.c

mo_cdi.f90: @MAINTAINER_MODE_TRUE@ $(top_srcdir)/src/cdi.h $(top_srcdir)/interfaces/f2003/bindGen.rb
	$(AM_V_GEN)$(RUBY) $(top_srcdir)/interfaces/f2003/bindGen.rb $(top_srcdir)/src/cdi.h $@

$(mo_cdi_modfile): mo_cdi.lo
	@if test ! -f $@; then rm -f $<; $(MAKE) $<; fi

pkgconfig/cdi.pc: pkgconfig/cdi.pc.in ../config.status
	(cd .. ; ./config.status src/pkgconfig/cdi.pc)

pkgconfig/cdipio.pc: pkgconfig/cdipio.pc.in ../config.status
	(cd .. ; ./config.status src/pkgconfig/cdipio.pc)

pkgconfig/cdi_f2003.pc: pkgconfig/cdi_f2003.pc.in ../config.status
	(cd .. ; ./config.status src/pkgconfig/cdi_f2003.pc)

cdilib.c:
	(cd $(top_srcdir)/src ; ./make_cdilib )

all-local: $(LOCALTARGETS)
CLEANFILES = `ls *~`
if CREATE_ISOC
CLEANFILES += $(mo_cdi_modfile)
endif
if ENABLE_CDI_LIB
CLEANFILES += pkgconfig/cdi.pc
if USE_MPI
CLEANFILES += pkgconfig/cdipio.pc
endif
if CREATE_ISOC
CLEANFILES += pkgconfig/cdi_f2003.pc
endif
endif

if ENABLE_CDI_LIB
install-exec-local: $(PKGCONFIG_FILES)
	$(mkinstalldirs) "$(DESTDIR)$(libdir)/pkgconfig"
	@for pkgconfigfile in $(PKGCONFIG_FILES) ; do \
		echo $(install_sh_DATA) $$pkgconfigfile \
		"$(DESTDIR)$(libdir)/$$pkgconfigfile" ; \
		$(install_sh_DATA) $$pkgconfigfile \
		"$(DESTDIR)$(libdir)/$$pkgconfigfile" ; \
	done

uninstall-local:
	@for pkgconfigfile in $(PKGCONFIG_FILES) ; do \
		echo rm -f "$(DESTDIR)$(libdir)/$$pkgconfigfile" ; \
		rm -f "$(DESTDIR)$(libdir)/$$pkgconfigfile" ; \
	done
	-@rmdir "$(DESTDIR)$(libdir)/pkgconfig"

endif

install-exec-hook:
	-@rmdir "$(DESTDIR)$(libdir)"
install-data-hook:
	-@rmdir "$(DESTDIR)$(includedir)"

EXTRA_DIST = \
  $(libcdi_la_HAVE_LIBGRIB_API_extra_sources) \
  $(libcdi_la_USE_FC_extra_sources) \
  $(libcdipio_la_HAVE_PARALLEL_NC4_extra_sources) \
  cdilib.c
