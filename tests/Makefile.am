TESTS = \
  calendar_test1 \
  cksum_verify \
  pio_cksum_asynch \
  pio_cksum_cdf \
  pio_cksum_fpguard \
  pio_cksum_grb2 \
  pio_cksum_mpi_fw_at_all \
  pio_cksum_mpi_fw_at_reblock \
  pio_cksum_mpi_fw_ordered \
  pio_cksum_mpinonb \
  pio_cksum_writer \
  pio_write_deco2d_run \
  pio_write_run \
  test_byteswap_run \
  test_cdf_const \
  test_cdf_transformation \
  test_chunk_cksum \
  test_cksum_extra \
  test_cksum_grib \
  test_cksum_grib2 \
  test_cksum_ieg \
  test_cksum_nc \
  test_cksum_nc2 \
  test_cksum_nc4 \
  test_cksum_service \
  test_f2003 \
  test_month_adjust \
  test_resource_copy_mpi_run \
  test_resource_copy_run \
  test_table_run

check_PROGRAMS = \
  calendar_test1 \
  cksum_read \
  cksum_verify \
  cksum_write \
  cksum_write_chunk \
  pio_write \
  pio_write_deco2d \
  test_byteswap \
  test_grib \
  test_month_adjust \
  test_resource_copy \
  test_table

if ENABLE_NETCDF
check_PROGRAMS += \
  test_cdf_read \
  test_cdf_write
endif

if ENABLE_MPI
check_PROGRAMS += test_resource_copy_mpi
endif ENABLE_MPI

AM_CPPFLAGS = -I$(top_srcdir)/src
if ENABLE_MPI
AM_CPPFLAGS += -DUSE_MPI
endif ENABLE_MPI
AM_CFLAGS = $(PPM_CORE_C_INCLUDE) $(YAXT_C_INCLUDE) $(MPI_C_INCLUDE)

AM_LDFLAGS =
if ENABLE_ALL_STATIC
AM_LDFLAGS += -all-static
endif

LDADD = $(top_builddir)/src/libcdi.la
if ENABLE_MPI
LDADD += $(top_builddir)/src/libcdipio.la
endif

calendar_test1_SOURCES = calendar_test1.c

cksum_read_SOURCES = \
  cksum_read.c \
  ensure_array_size.c \
  ensure_array_size.h \
  stream_cksum.c \
  stream_cksum.h \
  var_cksum.c \
  var_cksum.h

cksum_verify_SOURCES = cksum_verify.c

cksum_write_SOURCES = \
  cksum_write.c \
  simple_model_helper.c \
  simple_model_helper.h

cksum_write_chunk_SOURCES = \
  cksum_write_chunk.c \
  simple_model_helper.c \
  simple_model_helper.h

pio_write_SOURCES = \
  pio_write.c \
  pio_write.h \
  pio_write_setup_grid.c \
  pio_write_setup_grid.h \
  simple_model.c \
  simple_model_helper.c \
  simple_model_helper.h

pio_write_deco2d_SOURCES = \
  deco2d_model.c \
  pio_write.c \
  pio_write.h \
  pio_write_setup_grid.c \
  pio_write_setup_grid.h \
  simple_model_helper.c \
  simple_model_helper.h

test_byteswap_SOURCES = test_byteswap.c

test_grib_SOURCES = test_grib.c

test_month_adjust_SOURCES = test_month_adjust.c

test_resource_copy_SOURCES = test_resource_copy.c
test_resource_copy_LDADD = $(top_builddir)/src/libcdiresunpack.la

test_table_SOURCES = test_table.c

test_cdf_read_SOURCES = test_cdf_read.c

test_cdf_write_SOURCES = \
  simple_model_helper.c \
  simple_model_helper.h \
  test_cdf_write.c

test_resource_copy_mpi_SOURCES = test_resource_copy_mpi.c

clean-local:
	-rm -f *.grb *.grb2 *.nc *.nc2 *.nc4 *.srv *.ext *.cksum *.ieg *.log *.stderr *.stdout
	-rm -rf *.dSYM

AUTOMAKE_OPTIONS = color-tests

EXTRA_DIST = test_cksum_base

# overrides to make regular compilations also use libtool
# static because the libtool libraries in this directory are meant to
# be statically linked and there is no use for the PIC object
COMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CC) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_CFLAGS) $(CFLAGS)
FCCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(FC) -static $(AM_FCFLAGS) $(FCFLAGS)
CXXCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CXX) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_CXXFLAGS) $(CXXFLAGS)
PPFCCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(FC) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_FCFLAGS) $(FCFLAGS)
