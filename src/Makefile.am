lib_LTLIBRARIES =
check_LTLIBRARIES =
noinst_LTLIBRARIES =
EXTRA_PROGRAMS = make_fint
EXTRA_DIST = \
  cdilib.c \
  make_cdilib


if FC_MOD_UPPERCASE
mo_cdi_mod = MO_CDI.$(FCMODEXT)
else !FC_MOD_UPPERCASE
mo_cdi_mod = mo_cdi.$(FCMODEXT)
endif !FC_MOD_UPPERCASE

if ENABLE_CDI_LIB
include_HEADERS = \
  calendar.h \
  cdi.h \
  cdi_datetime.h \
  julian_date.h
nodist_include_HEADERS =
lib_LTLIBRARIES += libcdi.la
pkgconfigdir = $(libdir)/pkgconfig
nodist_pkgconfig_DATA = pkgconfig/cdi.pc
cmakedir = $(libdir)/cmake/cdi
nodist_cmake_DATA = \
  cmake/cdi/cdi-config-version.cmake \
  cmake/cdi/cdi-config.cmake
if ENABLE_CF_INTERFACE
include_HEADERS += cdi.inc
endif ENABLE_CF_INTERFACE
if ENABLE_ISOC_INTERFACE
nodist_include_HEADERS += $(mo_cdi_mod)
lib_LTLIBRARIES += libcdi_f2003.la
nodist_pkgconfig_DATA += pkgconfig/cdi_f2003.pc
endif ENABLE_ISOC_INTERFACE
else !ENABLE_CDI_LIB
noinst_LTLIBRARIES += libcdi.la
if ENABLE_ISOC_INTERFACE
noinst_LTLIBRARIES += libcdi_f2003.la
endif ENABLE_ISOC_INTERFACE
endif !ENABLE_CDI_LIB


if with_on_demand_check_programs
check_LTLIBRARIES += libcdiresunpack.la
else !with_on_demand_check_programs
noinst_LTLIBRARIES += libcdiresunpack.la
endif !with_on_demand_check_programs

AM_CPPFLAGS = $(PPM_CORE_C_INCLUDE) $(YAXT_C_INCLUDE) $(MPI_C_INCLUDE)
if ENABLE_MPI
# TODO: this needs a refactoring but for now we set this macro here and not in
# config.h because the macro is used in several files that do not include the
# header file directly.
AM_CPPFLAGS += -DUSE_MPI
endif

libcdiresunpack_la_SOURCES = \
  resource_unpack.c \
  resource_unpack.h
libcdiresunpack_la_LIBADD = libcdi.la

make_fint_SOURCES = make_fint.c
# the following must be passed via AM_CFLAGS, or the generic libtool recipes won't
# be used (but a custom, non-libtool one instead)
# make_fint_CFLAGS = -static
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
  cdf_filter.c \
  cdf_filter.h \
  cdf_int.c \
  cdf_int.h \
  cdf_lazy_grid.c \
  cdf_lazy_grid.h \
  cdf_read.c \
  cdf_records.c \
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
  cdi_datetime.c \
  cdi_datetime.h \
  cdi_error.c \
  cdi_fdb.c \
  cdi_fdb.h \
  cdi_get_config.c \
  cdi_int.c \
  cdi_int.h \
  cdi_key.c \
  cdi_key.h \
  cdi_limits.h \
  cdi_query.c \
  cdi_util.c \
  cdi_uuid.h \
  cgribex.h \
  cgribexlib.c \
  cksum.c \
  cksum.h \
  dmemory.c \
  dmemory.h \
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
  julian_date.c \
  julian_date.h \
  model.c \
  model.h \
  namespace.c \
  namespace.h \
  normalize_month.h \
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

if ENABLE_CF_INTERFACE
libcdi_la_SOURCES += \
  cdiFortran.c \
  cfortran.h
endif ENABLE_CF_INTERFACE

if HAVE_LIBGRIB_API
libcdi_la_SOURCES += \
  gribapi_utilities.c \
  stream_gribapi.c
endif HAVE_LIBGRIB_API

libcdi_f2003_la_SOURCES = mo_cdi.f90
libcdi_f2003_la_LIBADD = libcdi.la

if MAINTAINER_MODE
cdi.inc: cdi.h make_fint.c
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) AM_CFLAGS=-static make_fint
	$(AM_V_GEN)$(top_builddir)/libtool --mode=execute ./make_fint -o $(@D) $<
cdiFortran.c: cdi.inc
	$(AM_V_at)test -f $@ || rm -f $<
	$(AM_V_at)test -f $@ || $(MAKE) $(AM_MAKEFLAGS) $<

mo_cdi.f90: cdi.h $(top_srcdir)/interfaces/f2003/bindGen.rb
	$(AM_V_GEN)$(RUBY) $(top_srcdir)/interfaces/f2003/bindGen.rb $< $@

cdilib.c: $(libcdi_la_SOURCES) $(srcdir)/make_cdilib
	$(AM_V_GEN)CPP='$(CPP)' $(srcdir)/make_cdilib $(srcdir)
else !MAINTAINER_MODE
if ENABLE_MPI
cdiFortran.c cdi.inc cdipioFortran.c cdipio.inc mo_cdi.f90 cdilib.c:
	@:;{ \
	  echo "ERROR: cannot generate '$@' when the maintainer mode is disabled:"; \
          echo "       re-configure with the '--enable-maintainer-mode' option"; \
	} >&2; exit 1
else !ENABLE_MPI
cdiFortran.c cdi.inc mo_cdi.f90 cdilib.c:
	@:;{ \
	  echo "ERROR: cannot generate '$@' when the maintainer mode is disabled:"; \
          echo "       re-configure with the '--enable-maintainer-mode' option"; \
	} >&2; exit 1
endif !ENABLE_MPI
endif !MAINTAINER_MODE

$(mo_cdi_mod): mo_cdi.lo
	$(AM_V_at)test -f $@ || rm -f $<
	$(AM_V_at)test -f $@ || $(MAKE) $(AM_MAKEFLAGS) $<

cmake/cdi/cdi-config.cmake: $(top_builddir)/config.status cmake/cdi/cdi-config.cmake.in
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

cmake/cdi/cdi-config-version.cmake: $(top_builddir)/config.status cmake/cdi/cdi-config-version.cmake.in
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

pkgconfig/cdi.pc: $(top_builddir)/config.status $(srcdir)/pkgconfig/cdi.pc.in
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

pkgconfig/cdi_f2003.pc: $(top_builddir)/config.status $(srcdir)/pkgconfig/cdi_f2003.pc.in
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@

MOSTLYCLEANFILES = make_fint
if ENABLE_ISOC_INTERFACE
MOSTLYCLEANFILES += $(mo_cdi_mod)
endif

MAINTAINERCLEANFILES = \
  cdi.inc \
  cdiFortran.c \
  cdilib.c \
  mo_cdi.f90

include $(top_srcdir)/config/lt_overrides_Makefile.inc
