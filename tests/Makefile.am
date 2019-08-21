## Process this file with automake to produce Makefile.in
#
#TESTS = test_grib.sh
TESTS = cksum_verify \
	test_cksum_grib \
	test_cksum_grib2 \
	test_cksum_nc test_cksum_extra \
	test_cksum_service test_cksum_nc2 test_cksum_nc4 test_cksum_ieg \
	test_chunk_cksum \
	calendar_test1 \
	test_month_adjust \
	pio_write_run pio_cksum_mpinonb pio_cksum_fpguard \
	pio_cksum_asynch pio_cksum_writer \
	pio_cksum_cdf \
	pio_cksum_grb2 \
	pio_cksum_mpi_fw_ordered \
	pio_cksum_mpi_fw_at_all \
	pio_cksum_mpi_fw_at_reblock \
	test_resource_copy pio_write_deco2d_run \
	test_f2003 test_cdf_transformation test_cdf_const \
	test_table test_byteswap

check_PROGRAMS = cksum_verify test_grib cksum_write cksum_read \
	calendar_test1 \
	test_month_adjust \
	pio_write \
	test_resource_copy cksum_write_chunk pio_write_deco2d test_table \
	test_byteswap

if ENABLE_NETCDF
check_PROGRAMS += test_cdf_write test_cdf_read
endif

#
test_grib_SOURCES = test_grib.c
cksum_verify_SOURCES = cksum_verify.c
cksum_write_SOURCES = cksum_write.c simple_model_helper.h simple_model_helper.c
cksum_write_chunk_SOURCES = cksum_write_chunk.c simple_model_helper.h simple_model_helper.c
cksum_read_SOURCES = cksum_read.c \
	var_cksum.c var_cksum.h \
	stream_cksum.c stream_cksum.h \
	ensure_array_size.h ensure_array_size.c
calendar_test1_SOURCES = calendar_test1.c
test_month_adjust_SOURCES = test_month_adjust.c
pio_write_SOURCES = pio_write.c pio_write.h simple_model.c \
	simple_model_helper.h simple_model_helper.c \
	pio_write_setup_grid.h pio_write_setup_grid.c
pio_write_deco2d_SOURCES = pio_write.c pio_write.h deco2d_model.c \
	simple_model_helper.h simple_model_helper.c \
	pio_write_setup_grid.h pio_write_setup_grid.c
test_resource_copy_SOURCES = test_resource_copy.c
test_resource_copy_LDADD = $(UUID_C_LIB) ../src/libcdiresunpack.la $(LDADD)
test_resource_copy_mpi_SOURCES = test_resource_copy_mpi.c
test_cdf_write_SOURCES = test_cdf_write.c \
	simple_model_helper.h simple_model_helper.c
test_cdf_read_SOURCES = test_cdf_read.c
test_table_SOURCES = test_table.c
test_byteswap_SOURCES = test_byteswap.c
#
AM_CFLAGS = $(PPM_CORE_CFLAGS) $(YAXT_CFLAGS) $(MPI_C_INCLUDE)
if USE_MPI
pio_write_LDADD = ../src/libcdipio.la $(UUID_C_LIB) $(MPI_C_LIB)
pio_write_deco2d_LDADD = ../src/libcdipio.la $(UUID_C_LIB) $(MPI_C_LIB)
TESTS +=  test_resource_copy_mpi_run
check_PROGRAMS += test_resource_copy_mpi
test_resource_copy_mpi_LDADD = ../src/libcdipio.la $(UUID_C_LIB)
else
pio_write_LDADD = $(LDADD) $(UUID_C_LIB)
pio_write_deco2d_LDADD = $(LDADD) $(UUID_C_LIB)
test_resource_copy_mpi_LDADD = $(LDADD) $(UUID_C_LIB)
endif

LDADD = ../src/libcdi.la
AM_CPPFLAGS = -I$(top_srcdir)/src
#
EXTRA_DIST = test_cdf_write.c test_cdf_read.c
#
AUTOMAKE_OPTIONS = color-tests
#
clean-local: clean-local-dirs
	-rm -f *~ *.grb *.nc *.nc2 *.nc4 *.srv *.svc *.ext *.cksum *.ieg *.log

.PHONY: clean-local-dirs
clean-local-dirs:
	-rm -rf test_cdf_const.d *.dSYM

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
