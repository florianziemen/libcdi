pkgconfigdir = $(libdir)/pkgconfig
cmakedir = $(libdir)/cmake

include_HEADERS =
nodist_include_HEADERS =
lib_LTLIBRARIES =
noinst_LTLIBRARIES = libcdiresunpack.la
EXTRA_PROGRAMS = make_fint
EXTRA_DIST = cdilib.c
nodist_pkgconfig_DATA =
nodist_cmake_DATA =

if FORTRAN_MOD_UC
mo_cdi_modfile = MO_CDI.$(FCMODEXT)
else
mo_cdi_modfile = mo_cdi.$(FCMODEXT)
endif

if ENABLE_CDI_LIB
include_HEADERS += cdi.h
lib_LTLIBRARIES += libcdi.la
nodist_pkgconfig_DATA += pkgconfig/cdi.pc
nodist_cmake_DATA += \
  cmake/cdi-config-version.cmake \
  cmake/cdi-config.cmake
# add cdi.inc only if cfortran interface is required
if USE_FC
include_HEADERS += cdi.inc
endif
if CREATE_ISOC
nodist_include_HEADERS += $(mo_cdi_modfile)
lib_LTLIBRARIES += libcdi_f2003.la
nodist_pkgconfig_DATA += pkgconfig/cdi_f2003.pc
endif
if USE_MPI
include_HEADERS += cdipio.h
lib_LTLIBRARIES += libcdipio.la
nodist_pkgconfig_DATA += pkgconfig/cdipio.pc
if USE_FC
include_HEADERS += cdipio.inc
endif
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

libcdiresunpack_la_SOURCES = resource_unpack.c

make_fint_SOURCES = make_fint.c
make_fint_LDADD = $(LIBOBJS)

libcdi_la_SOURCES = \
  async_worker.c \
  async_worker.h \
  basetime.c \
  basetime.h \
  binary.c \
  binary.h \
  calendar.c \
  calendar.h \
  cdf.c \
  cdf.h \
  cdf_config.h \
  cdf_int.c \
  cdf_int.h \
  cdf_lazy_grid.c \
  cdf_lazy_grid.h \
  cdf_read.c \
  cdf_util.c \
  cdf_util.h \
  cdf_write.c \
  cdi.h \
  cdi_across.c \
  cdi_across.h \
  cdi_att.c \
  cdi_att.h \
  cdi_cksum.c \
  cdi_cksum.h \
  cdi_error.c \
  cdi_fdb.c \
  cdi_fdb.h \
  cdi_int.c \
  cdi_int.h \
  cdi_key.c \
  cdi_key.h \
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
  gaussian_latitudes.c \
  get_num_missvals.c \
  get_num_missvals.h \
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
  stream_ieg.c \
  stream_ieg.h \
  stream_read.c \
  stream_record.c \
  stream_scan.c \
  stream_scan.h \
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
  vlist_var_key.c \
  vlist_var_pack.c \
  zaxis.c \
  zaxis.h

# cfortran.h is an optional part of libcdi
if USE_FC
libcdi_la_SOURCES += \
  cdiFortran.c \
  cfortran.h
endif

# these only contain code if grib_api is available
if HAVE_LIBGRIB_API
libcdi_la_SOURCES += \
  gribapi_utilities.c \
  stream_gribapi.c
endif

libcdi_la_LIBADD = $(UUID_C_LIB)

libcdi_f2003_la_SOURCES = mo_cdi.f90
libcdi_f2003_la_LIBADD = libcdi.la

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

if HAVE_PARALLEL_NC4
libcdipio_la_SOURCES += pio_cdf_int.c
endif

libcdipio_la_LIBADD = libcdi.la $(LIBRT) $(PPM_CORE_C_LIB) $(YAXT_C_LIB) $(MPI_C_LIB)

if MAINTAINER_MODE
# Update files in any configuration:
BUILT_SOURCES = \
  cdi.inc \
  cdiFortran.c \
  cdipio.inc \
  cdipioFortran.c \
  mo_cdi.f90
endif

# See https://www.gnu.org/software/automake/manual/html_node/Multiple-Outputs.html
make_fint_run = stem=`echo '$@' | sed -e 's|.*/||' -e 's|.inc||' -e 's|Fortran.c||'`;
if MAINTAINER_MODE
make_fint_run += ./make_fint -o $(@D) "$(srcdir)/$${stem}.h"
else
make_fint_run += \
  test -f "$${stem}.inc" && test -f "$${stem}Fortran.c" && exit 0; \
  trap "rm -rf .fint/$${stem}.lock .fint/$${stem}.stamp" 1 2 13 15; \
  if mkdir .fint/$${stem}.lock 2>/dev/null; then \
    rm -f .fint/$${stem}.stamp; ec=0; \
    test '.' != '$(srcdir)' && \
      for f in "$(srcdir)/$${stem}.inc" "$(srcdir)/$${stem}Fortran.c"; do \
        test -f $$f && \
          echo "The source directory is inconsistent: remove '$$f' to recover" >&2 && \
          ec=1 && break; done; \
    test $$ec != 0 || ./make_fint -o $(@D) "$(srcdir)/$${stem}.h" || ec=1; \
    test $$ec = 0 && touch .fint/$${stem}.stamp; \
    rm -rf .fint/$${stem}.lock; exit $$ec; \
  else \
    while test -d .fint/$${stem}.lock; do sleep 1; done; \
    test -f .fint/$${stem}.stamp || exit 1; fi
endif

# We have to provide the recipe fo non-maintainer mode literally to let make
# know that it contains a recursive make call:
cdiFortran.c cdi.inc: @MAINTAINER_MODE_TRUE@ $(srcdir)/cdi.h make_fint
@MAINTAINER_MODE_FALSE@	@$(MKDIR_P) .fint; \
@MAINTAINER_MODE_FALSE@	trap 'rm -rf .fint/make.lock .fint/make.stamp' 1 2 13 15; \
@MAINTAINER_MODE_FALSE@	if mkdir .fint/make.lock 2>/dev/null; then \
@MAINTAINER_MODE_FALSE@	  rm -f .fint/make.stamp; \
@MAINTAINER_MODE_FALSE@	  $(MAKE) $(AM_MAKEFLAGS) make_fint; ec=$$?; \
@MAINTAINER_MODE_FALSE@	  test $$ec = 0 && touch .fint/make.stamp; \
@MAINTAINER_MODE_FALSE@	rm -rf .fint/make.lock; exit $$ec; \
@MAINTAINER_MODE_FALSE@	else \
@MAINTAINER_MODE_FALSE@	  while test -d .fint/make.lock; do sleep 1; done; \
@MAINTAINER_MODE_FALSE@	  test -f .fint/make.stamp || exit 1; fi
	$(AM_V_GEN)$(make_fint_run)
cdi.inc: @MAINTAINER_MODE_TRUE@ cdiFortran.c

cdipioFortran.c cdipio.inc: @MAINTAINER_MODE_TRUE@ $(srcdir)/cdipio.h make_fint
@MAINTAINER_MODE_FALSE@	@$(MKDIR_P) .fint; trap 'rm -rf .fint/make.lock .fint/make.stamp' 1 2 13 15; \
@MAINTAINER_MODE_FALSE@	if mkdir .fint/make.lock 2>/dev/null; then \
@MAINTAINER_MODE_FALSE@	  rm -f .fint/make.stamp; \
@MAINTAINER_MODE_FALSE@	  $(MAKE) $(AM_MAKEFLAGS) make_fint; ec=$$?; \
@MAINTAINER_MODE_FALSE@	  test $$ec = 0 && touch .fint/make.stamp; \
@MAINTAINER_MODE_FALSE@	rm -rf .fint/make.lock; exit $$ec; \
@MAINTAINER_MODE_FALSE@	else \
@MAINTAINER_MODE_FALSE@	  while test -d .fint/make.lock; do sleep 1; done; \
@MAINTAINER_MODE_FALSE@	  test -f .fint/make.stamp || exit 1; fi
	$(AM_V_GEN)$(make_fint_run)
cdipio.inc: @MAINTAINER_MODE_TRUE@ cdipioFortran.c

mo_cdi.f90: @MAINTAINER_MODE_TRUE@ $(srcdir)/cdi.h $(top_srcdir)/interfaces/f2003/bindGen.rb
	$(AM_V_GEN)$(RUBY) $(top_srcdir)/interfaces/f2003/bindGen.rb $(srcdir)/cdi.h $@

if MAINTAINER_MODE
# Update files in $(srcdir) when building out-of-source:
$(srcdir)/cdiFortran.c $(srcdir)/cdi.inc:
$(srcdir)/cdipioFortran.c $(srcdir)/cdipio.inc:
$(srcdir)/mo_cdi.f90:
endif

$(mo_cdi_modfile): mo_cdi.lo
	$(AM_V_at)test -f $@ || { rm -f $<; $(MAKE) $(AM_MAKEFLAGS) $<; }

cmake/cdi-config.cmake: cmake/cdi-config.cmake.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

cmake/cdi-config-version.cmake: cmake/cdi-config-version.cmake.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

pkgconfig/cdi.pc: pkgconfig/cdi.pc.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

pkgconfig/cdipio.pc: pkgconfig/cdipio.pc.in ../config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

pkgconfig/cdi_f2003.pc: pkgconfig/cdi_f2003.pc.in ../config.status
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

cdilib.c:
	(cd $(top_srcdir)/src ; ./make_cdilib )

MOSTLYCLEANFILES = make_fint
CLEANFILES =
if CREATE_ISOC
CLEANFILES += $(mo_cdi_modfile)
endif

clean-local:
@MAINTAINER_MODE_FALSE@	-rm -rf .fint
	-test '.' = '$(srcdir)' || rm -f cdi.inc cdiFortran.c cdipio.inc cdipioFortran.c mo_cdi.f90

install-exec-hook:
	-@rmdir "$(DESTDIR)$(libdir)"
install-data-hook:
	-@rmdir "$(DESTDIR)$(includedir)"
