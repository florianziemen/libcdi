TESTS =                       \
  calendar_test1              \
  cksum_verify                \
  pio_write_deco2d_run_serial \
  pio_write_run_serial        \
  test_byteswap               \
  test_cdf_const              \
  test_cdf_transformation     \
  test_cksum_extra            \
  test_cksum_grib             \
  test_cksum_grib2            \
  test_cksum_ieg              \
  test_cksum_nc               \
  test_cksum_nc2              \
  test_cksum_nc4              \
  test_cksum_nc_chunk         \
  test_cksum_service          \
  test_f2003                  \
  test_month_adjust           \
  test_resource_copy          \
  test_table

# MPI test must have prefix *.test:
TESTS +=                             \
  pio_cksum_asynch.test              \
  pio_cksum_fpguard.test             \
  pio_cksum_grb2.test                \
  pio_cksum_mpi_fw_at_all.test       \
  pio_cksum_mpi_fw_at_reblock.test   \
  pio_cksum_mpi_fw_ordered.test      \
  pio_cksum_mpinonb.test             \
  pio_cksum_nc.test                  \
  pio_cksum_nc2.test                 \
  pio_cksum_nc4.test                 \
  pio_cksum_writer.test              \
  pio_write_deco2d_run_parallel.test \
  pio_write_run_parallel.test        \
  test_resource_copy_mpi_run.test

if !with_concurrent_mpi_checks
pio_cksum_fpguard.log: pio_cksum_asynch.log
pio_cksum_grb2.log: pio_cksum_fpguard.log
pio_cksum_mpi_fw_at_all.log: pio_cksum_grb2.log
pio_cksum_mpi_fw_at_reblock.log: pio_cksum_mpi_fw_at_all.log
pio_cksum_mpi_fw_ordered.log: pio_cksum_mpi_fw_at_reblock.log
pio_cksum_mpinonb.log: pio_cksum_mpi_fw_ordered.log
pio_cksum_nc.log: pio_cksum_mpinonb.log
pio_cksum_nc2.log: pio_cksum_nc.log
pio_cksum_nc4.log: pio_cksum_nc2.log
pio_cksum_writer.log: pio_cksum_nc4.log
pio_write_deco2d_run_parallel.log: pio_cksum_writer.log
pio_write_run_parallel.log: pio_write_deco2d_run_parallel.log
test_resource_copy_mpi_run.log: pio_write_run_parallel.log
endif !with_concurrent_mpi_checks

test_PROGRAMS_ =          \
  calendar_test1          \
  cksum_read              \
  cksum_verify            \
  cksum_write             \
  cksum_write_chunk       \
  pio_write_deco2d_serial \
  pio_write_serial        \
  test_byteswap           \
  test_month_adjust       \
  test_resource_copy      \
  test_table

if ENABLE_NETCDF
test_PROGRAMS_ += \
  test_cdf_read   \
  test_cdf_write
endif

if ENABLE_MPI
test_PROGRAMS_ +=           \
  pio_write_deco2d_parallel \
  pio_write_parallel        \
  test_resource_copy_mpi
endif ENABLE_MPI

if with_on_demand_check_programs
check_PROGRAMS = $(test_PROGRAMS_)
else !with_on_demand_check_programs
noinst_PROGRAMS = $(test_PROGRAMS_)
endif !with_on_demand_check_programs

AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS = $(PPM_CORE_C_INCLUDE) $(YAXT_C_INCLUDE) $(MPI_C_INCLUDE)

AM_LDFLAGS =
if ENABLE_ALL_STATIC
AM_LDFLAGS += -all-static
endif

LDADD = $(top_builddir)/src/libcdi.la

calendar_test1_SOURCES = \
  calendar_test1.c

cksum_read_SOURCES =  \
  cksum_read.c        \
  ensure_array_size.c \
  ensure_array_size.h \
  stream_cksum.c      \
  stream_cksum.h      \
  var_cksum.c         \
  var_cksum.h

cksum_verify_SOURCES = \
  cksum_verify.c

cksum_write_SOURCES =   \
  cksum_write.c         \
  simple_model_helper.c \
  simple_model_helper.h

cksum_write_chunk_SOURCES = \
  cksum_write_chunk.c       \
  simple_model_helper.c     \
  simple_model_helper.h

pio_write_deco2d_serial_SOURCES = \
  deco2d_model.c                  \
  pio_write.c                     \
  pio_write.h                     \
  pio_write_setup_grid.c          \
  pio_write_setup_grid.h          \
  simple_model_helper.c           \
  simple_model_helper.h

pio_write_serial_SOURCES = \
  pio_write.c              \
  pio_write.h              \
  pio_write_setup_grid.c   \
  pio_write_setup_grid.h   \
  simple_model.c           \
  simple_model_helper.c    \
  simple_model_helper.h

test_byteswap_SOURCES = \
  test_byteswap.c

test_month_adjust_SOURCES = \
  test_month_adjust.c

test_resource_copy_SOURCES = \
  test_resource_copy.c
test_resource_copy_LDADD = $(top_builddir)/src/libcdiresunpack.la

test_table_SOURCES = \
  test_table.c

test_cdf_read_SOURCES = \
  test_cdf_read.c

test_cdf_write_SOURCES = \
  simple_model_helper.c  \
  simple_model_helper.h  \
  test_cdf_write.c

pio_write_deco2d_parallel_SOURCES =  \
  $(pio_write_deco2d_serial_SOURCES)
pio_write_deco2d_parallel_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_MPI
pio_write_deco2d_parallel_LDADD = $(top_builddir)/src/libcdipio.la

pio_write_parallel_SOURCES =  \
  $(pio_write_serial_SOURCES)
pio_write_parallel_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_MPI
pio_write_parallel_LDADD = $(top_builddir)/src/libcdipio.la

test_resource_copy_mpi_SOURCES = \
  test_resource_copy_mpi.c
test_resource_copy_mpi_LDADD = $(top_builddir)/src/libcdipio.la

clean-local:
	-rm -f *.cksum *.ext *.grb *.grb2 *.ieg *.nc *.nc2 *.nc4 *.stderr *.stdout *.svc
	-rm -rf *.dSYM

AUTOMAKE_OPTIONS = color-tests

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
PPFCCOMPILE = $(FCCOMPILE)
