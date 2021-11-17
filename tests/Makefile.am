TESTS = \
  calendar_test1 \
  cksum_verify \
  test_byteswap_run \
  pio_write_deco2d_run \
  pio_write_run \
  test_cdf_const \
  test_cdf_transformation \
  test_cksum_extra \
  test_cksum_grib \
  test_cksum_grib2 \
  test_cksum_ieg \
  test_cksum_nc \
  test_cksum_nc2 \
  test_cksum_nc4 \
  test_cksum_nc_chunk \
  test_cksum_service \
  test_f2003 \
  test_month_adjust \
  test_resource_copy_run \
  test_table_run

# MPI test must have prefix *.test:
TESTS += \
  pio_cksum_asynch.test \
  pio_cksum_fpguard.test \
  pio_cksum_grb2.test \
  pio_cksum_mpi_fw_at_all.test \
  pio_cksum_mpi_fw_at_reblock.test \
  pio_cksum_mpi_fw_ordered.test \
  pio_cksum_mpinonb.test \
  pio_cksum_nc.test \
  pio_cksum_nc2.test \
  pio_cksum_nc4.test \
  pio_cksum_writer.test \
  pio_write_deco2d_run_parallel.test \
  pio_write_dist_array.test \
  pio_write_run_parallel.test \
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
pio_write_dist_array.log: pio_write_deco2d_run_parallel.log
pio_write_run_parallel.log: pio_write_dist_array.log
test_resource_copy_mpi_run.log: pio_write_run_parallel.log
endif !with_concurrent_mpi_checks

test_PROGRAMS_ = \
  calendar_test1 \
  cksum_read \
  cksum_verify \
  cksum_write \
  cksum_write_chunk \
  pio_write \
  pio_write_deco2d \
  test_byteswap \
  test_month_adjust \
  test_resource_copy \
  test_table

if ENABLE_NETCDF
test_PROGRAMS_ += \
  test_cdf_read \
  test_cdf_write
endif

if ENABLE_MPI
test_PROGRAMS_ += \
  pio_write_deco2d_parallel \
  pio_write_parallel \
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
nodist_pio_write_parallel_SOURCES = \
  pio_write.parallel.c \
  pio_write_setup_grid.parallel.c \
  simple_model.parallel.c \
  simple_model_helper.parallel.c
pio_write_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

pio_write_deco2d_SOURCES = \
  deco2d_model.c \
  pio_write.c \
  pio_write.h \
  pio_write_setup_grid.c \
  pio_write_setup_grid.h \
  simple_model_helper.c \
  simple_model_helper.h
nodist_pio_write_deco2d_parallel_SOURCES = \
  deco2d_model.parallel.c \
  pio_write.parallel.c \
  pio_write_setup_grid.parallel.c \
  simple_model_helper.parallel.c
pio_write_deco2d_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

test_byteswap_SOURCES = test_byteswap.c

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
test_resource_copy_mpi_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

clean-local:
	-rm -f *.parallel.c
	-rm -f *.grb *.grb2 *.nc *.nc2 *.nc4 *.srv *.ext *.cksum *.ieg *.stderr *.stdout
	-rm -rf *.dSYM

AUTOMAKE_OPTIONS = color-tests

EXTRA_DIST = test_cksum_base

# Compile programs using Libtool:
COMPILE = $(LTCOMPILE)
FCCOMPILE = $(LTFCCOMPILE)
PPFCCOMPILE = $(LTPPFCCOMPILE)
CXXCOMPILE = $(LTCXXCOMPILE)

# Compile *.F90 without CPPFLAGS, which are normally meant for the C compiler
# and might not be compatible with the Fortran compiler:
LTPPFCCOMPILE = $(LTFCCOMPILE)

# Avoid compiling twice by running Libtool with '-static'
#   This is safe because all Libtool libraries in this directory are
#   static-only and contain PIC objects:
LTCOMPILE_prefix = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CC) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES)
LTCOMPILE = $(LTCOMPILE_prefix) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=link $(CCLD) -static $(AM_CFLAGS) $(CFLAGS) \
        $(AM_LDFLAGS) $(LDFLAGS) -o $@
LTFCCOMPILE_prefix = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(FC) -static
LTFCCOMPILE = $(LTFCCOMPILE_prefix) $(AM_FCFLAGS) $(FCFLAGS)
FCLINK = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=link $(FCLD) -static $(AM_FCFLAGS) $(FCFLAGS) \
        $(AM_LDFLAGS) $(LDFLAGS) -o $@
LTCXXCOMPILE_prefix = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CXX) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES)
LTCXXCOMPILE = $(LTCXXCOMPILE_prefix) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS)
CXXLINK = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=link $(CXXLD) -static $(AM_CXXFLAGS) \
        $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

# Generate source files for parallel versions of the programs. We do this to
# avoid target-specific flags, which result into rules that do not use the
# compilation commands above.
SUFFIXES = .parallel.c

.c.parallel.c:
	$(AM_V_GEN):;{ echo '#define USE_MPI 1' && cat $<; }>$@
