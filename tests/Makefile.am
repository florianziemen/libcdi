TESTS = \
  calendar_test1 \
  cksum_verify \
  pio_write_deco2d_run_serial \
  pio_write_run_serial \
  test_byteswap \
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
  test_resource_copy \
  test_table

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
  pio_write_deco2d_serial \
  pio_write_serial \
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

pio_write_deco2d_serial_SOURCES = \
  deco2d_model.c \
  pio_write.c \
  pio_write.h \
  pio_write_setup_grid.c \
  pio_write_setup_grid.h \
  simple_model_helper.c \
  simple_model_helper.h

pio_write_serial_SOURCES = \
  pio_write.c \
  pio_write.h \
  pio_write_setup_grid.c \
  pio_write_setup_grid.h \
  simple_model.c \
  simple_model_helper.c \
  simple_model_helper.h

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

pio_write_deco2d_parallel_SOURCES = \
  $(pio_write_deco2d_serial_SOURCES)
pio_write_deco2d_parallel_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_MPI
pio_write_deco2d_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

pio_write_parallel_SOURCES = \
  $(pio_write_serial_SOURCES)
pio_write_parallel_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_MPI
pio_write_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

test_resource_copy_mpi_SOURCES = test_resource_copy_mpi.c
test_resource_copy_mpi_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

clean-local:
	-rm -f *.cksum *.ext *.grb *.grb2 *.ieg *.nc *.nc2 *.nc4 *.stderr *.stdout *.svc
	-rm -rf *.dSYM

AUTOMAKE_OPTIONS = color-tests

# Compile programs using Libtool:
COMPILE = $(LTCOMPILE)
FCCOMPILE = $(LTFCCOMPILE)
PPFCCOMPILE = $(LTPPFCCOMPILE)
CXXCOMPILE = $(LTCXXCOMPILE)

#
# Avoid compiling twice by running Libtool with '-static'
#   This is safe because none of the Libtool libraries in this directory is
#   installed:
#

LTCOMPILE_prefix = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CC) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES)
LTCOMPILE = $(LTCOMPILE_prefix) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)

LTFCCOMPILE_prefix = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(FC) -static
LTFCCOMPILE = $(LTFCCOMPILE_prefix) $(AM_FCFLAGS) $(FCFLAGS)

# Also compile *.F90 without CPPFLAGS, which are normally meant for the C
# compiler and might not be compatible with the Fortran compiler:
LTPPFCCOMPILE = $(LTFCCOMPILE)

LTCXXCOMPILE_prefix = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CXX) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES)
LTCXXCOMPILE = $(LTCXXCOMPILE_prefix) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS)

#
# TODO: the following is too hard to maintain, we should try to eliminate the
# need for this:
#

pio_write_deco2d_parallel-deco2d_model.o: deco2d_model.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_deco2d_parallel-deco2d_model.o -MD -MP -MF $(DEPDIR)/pio_write_deco2d_parallel-deco2d_model.Tpo -c -o pio_write_deco2d_parallel-deco2d_model.o `test -f 'deco2d_model.c' || echo '$(srcdir)/'`deco2d_model.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_deco2d_parallel-deco2d_model.Tpo $(DEPDIR)/pio_write_deco2d_parallel-deco2d_model.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='deco2d_model.c' object='pio_write_deco2d_parallel-deco2d_model.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_deco2d_parallel-deco2d_model.o `test -f 'deco2d_model.c' || echo '$(srcdir)/'`deco2d_model.c

pio_write_deco2d_parallel-deco2d_model.obj: deco2d_model.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_deco2d_parallel-deco2d_model.obj -MD -MP -MF $(DEPDIR)/pio_write_deco2d_parallel-deco2d_model.Tpo -c -o pio_write_deco2d_parallel-deco2d_model.obj `if test -f 'deco2d_model.c'; then $(CYGPATH_W) 'deco2d_model.c'; else $(CYGPATH_W) '$(srcdir)/deco2d_model.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_deco2d_parallel-deco2d_model.Tpo $(DEPDIR)/pio_write_deco2d_parallel-deco2d_model.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='deco2d_model.c' object='pio_write_deco2d_parallel-deco2d_model.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_deco2d_parallel-deco2d_model.obj `if test -f 'deco2d_model.c'; then $(CYGPATH_W) 'deco2d_model.c'; else $(CYGPATH_W) '$(srcdir)/deco2d_model.c'; fi`

pio_write_deco2d_parallel-pio_write.o: pio_write.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_deco2d_parallel-pio_write.o -MD -MP -MF $(DEPDIR)/pio_write_deco2d_parallel-pio_write.Tpo -c -o pio_write_deco2d_parallel-pio_write.o `test -f 'pio_write.c' || echo '$(srcdir)/'`pio_write.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_deco2d_parallel-pio_write.Tpo $(DEPDIR)/pio_write_deco2d_parallel-pio_write.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='pio_write.c' object='pio_write_deco2d_parallel-pio_write.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_deco2d_parallel-pio_write.o `test -f 'pio_write.c' || echo '$(srcdir)/'`pio_write.c

pio_write_deco2d_parallel-pio_write.obj: pio_write.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_deco2d_parallel-pio_write.obj -MD -MP -MF $(DEPDIR)/pio_write_deco2d_parallel-pio_write.Tpo -c -o pio_write_deco2d_parallel-pio_write.obj `if test -f 'pio_write.c'; then $(CYGPATH_W) 'pio_write.c'; else $(CYGPATH_W) '$(srcdir)/pio_write.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_deco2d_parallel-pio_write.Tpo $(DEPDIR)/pio_write_deco2d_parallel-pio_write.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='pio_write.c' object='pio_write_deco2d_parallel-pio_write.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_deco2d_parallel-pio_write.obj `if test -f 'pio_write.c'; then $(CYGPATH_W) 'pio_write.c'; else $(CYGPATH_W) '$(srcdir)/pio_write.c'; fi`

pio_write_deco2d_parallel-pio_write_setup_grid.o: pio_write_setup_grid.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_deco2d_parallel-pio_write_setup_grid.o -MD -MP -MF $(DEPDIR)/pio_write_deco2d_parallel-pio_write_setup_grid.Tpo -c -o pio_write_deco2d_parallel-pio_write_setup_grid.o `test -f 'pio_write_setup_grid.c' || echo '$(srcdir)/'`pio_write_setup_grid.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_deco2d_parallel-pio_write_setup_grid.Tpo $(DEPDIR)/pio_write_deco2d_parallel-pio_write_setup_grid.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='pio_write_setup_grid.c' object='pio_write_deco2d_parallel-pio_write_setup_grid.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_deco2d_parallel-pio_write_setup_grid.o `test -f 'pio_write_setup_grid.c' || echo '$(srcdir)/'`pio_write_setup_grid.c

pio_write_deco2d_parallel-pio_write_setup_grid.obj: pio_write_setup_grid.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_deco2d_parallel-pio_write_setup_grid.obj -MD -MP -MF $(DEPDIR)/pio_write_deco2d_parallel-pio_write_setup_grid.Tpo -c -o pio_write_deco2d_parallel-pio_write_setup_grid.obj `if test -f 'pio_write_setup_grid.c'; then $(CYGPATH_W) 'pio_write_setup_grid.c'; else $(CYGPATH_W) '$(srcdir)/pio_write_setup_grid.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_deco2d_parallel-pio_write_setup_grid.Tpo $(DEPDIR)/pio_write_deco2d_parallel-pio_write_setup_grid.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='pio_write_setup_grid.c' object='pio_write_deco2d_parallel-pio_write_setup_grid.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_deco2d_parallel-pio_write_setup_grid.obj `if test -f 'pio_write_setup_grid.c'; then $(CYGPATH_W) 'pio_write_setup_grid.c'; else $(CYGPATH_W) '$(srcdir)/pio_write_setup_grid.c'; fi`

pio_write_deco2d_parallel-simple_model_helper.o: simple_model_helper.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_deco2d_parallel-simple_model_helper.o -MD -MP -MF $(DEPDIR)/pio_write_deco2d_parallel-simple_model_helper.Tpo -c -o pio_write_deco2d_parallel-simple_model_helper.o `test -f 'simple_model_helper.c' || echo '$(srcdir)/'`simple_model_helper.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_deco2d_parallel-simple_model_helper.Tpo $(DEPDIR)/pio_write_deco2d_parallel-simple_model_helper.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='simple_model_helper.c' object='pio_write_deco2d_parallel-simple_model_helper.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_deco2d_parallel-simple_model_helper.o `test -f 'simple_model_helper.c' || echo '$(srcdir)/'`simple_model_helper.c

pio_write_deco2d_parallel-simple_model_helper.obj: simple_model_helper.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_deco2d_parallel-simple_model_helper.obj -MD -MP -MF $(DEPDIR)/pio_write_deco2d_parallel-simple_model_helper.Tpo -c -o pio_write_deco2d_parallel-simple_model_helper.obj `if test -f 'simple_model_helper.c'; then $(CYGPATH_W) 'simple_model_helper.c'; else $(CYGPATH_W) '$(srcdir)/simple_model_helper.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_deco2d_parallel-simple_model_helper.Tpo $(DEPDIR)/pio_write_deco2d_parallel-simple_model_helper.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='simple_model_helper.c' object='pio_write_deco2d_parallel-simple_model_helper.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_deco2d_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_deco2d_parallel-simple_model_helper.obj `if test -f 'simple_model_helper.c'; then $(CYGPATH_W) 'simple_model_helper.c'; else $(CYGPATH_W) '$(srcdir)/simple_model_helper.c'; fi`

pio_write_parallel-pio_write.o: pio_write.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_parallel-pio_write.o -MD -MP -MF $(DEPDIR)/pio_write_parallel-pio_write.Tpo -c -o pio_write_parallel-pio_write.o `test -f 'pio_write.c' || echo '$(srcdir)/'`pio_write.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_parallel-pio_write.Tpo $(DEPDIR)/pio_write_parallel-pio_write.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='pio_write.c' object='pio_write_parallel-pio_write.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_parallel-pio_write.o `test -f 'pio_write.c' || echo '$(srcdir)/'`pio_write.c

pio_write_parallel-pio_write.obj: pio_write.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_parallel-pio_write.obj -MD -MP -MF $(DEPDIR)/pio_write_parallel-pio_write.Tpo -c -o pio_write_parallel-pio_write.obj `if test -f 'pio_write.c'; then $(CYGPATH_W) 'pio_write.c'; else $(CYGPATH_W) '$(srcdir)/pio_write.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_parallel-pio_write.Tpo $(DEPDIR)/pio_write_parallel-pio_write.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='pio_write.c' object='pio_write_parallel-pio_write.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_parallel-pio_write.obj `if test -f 'pio_write.c'; then $(CYGPATH_W) 'pio_write.c'; else $(CYGPATH_W) '$(srcdir)/pio_write.c'; fi`

pio_write_parallel-pio_write_setup_grid.o: pio_write_setup_grid.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_parallel-pio_write_setup_grid.o -MD -MP -MF $(DEPDIR)/pio_write_parallel-pio_write_setup_grid.Tpo -c -o pio_write_parallel-pio_write_setup_grid.o `test -f 'pio_write_setup_grid.c' || echo '$(srcdir)/'`pio_write_setup_grid.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_parallel-pio_write_setup_grid.Tpo $(DEPDIR)/pio_write_parallel-pio_write_setup_grid.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='pio_write_setup_grid.c' object='pio_write_parallel-pio_write_setup_grid.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_parallel-pio_write_setup_grid.o `test -f 'pio_write_setup_grid.c' || echo '$(srcdir)/'`pio_write_setup_grid.c

pio_write_parallel-pio_write_setup_grid.obj: pio_write_setup_grid.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_parallel-pio_write_setup_grid.obj -MD -MP -MF $(DEPDIR)/pio_write_parallel-pio_write_setup_grid.Tpo -c -o pio_write_parallel-pio_write_setup_grid.obj `if test -f 'pio_write_setup_grid.c'; then $(CYGPATH_W) 'pio_write_setup_grid.c'; else $(CYGPATH_W) '$(srcdir)/pio_write_setup_grid.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_parallel-pio_write_setup_grid.Tpo $(DEPDIR)/pio_write_parallel-pio_write_setup_grid.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='pio_write_setup_grid.c' object='pio_write_parallel-pio_write_setup_grid.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_parallel-pio_write_setup_grid.obj `if test -f 'pio_write_setup_grid.c'; then $(CYGPATH_W) 'pio_write_setup_grid.c'; else $(CYGPATH_W) '$(srcdir)/pio_write_setup_grid.c'; fi`

pio_write_parallel-simple_model.o: simple_model.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_parallel-simple_model.o -MD -MP -MF $(DEPDIR)/pio_write_parallel-simple_model.Tpo -c -o pio_write_parallel-simple_model.o `test -f 'simple_model.c' || echo '$(srcdir)/'`simple_model.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_parallel-simple_model.Tpo $(DEPDIR)/pio_write_parallel-simple_model.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='simple_model.c' object='pio_write_parallel-simple_model.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_parallel-simple_model.o `test -f 'simple_model.c' || echo '$(srcdir)/'`simple_model.c

pio_write_parallel-simple_model.obj: simple_model.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_parallel-simple_model.obj -MD -MP -MF $(DEPDIR)/pio_write_parallel-simple_model.Tpo -c -o pio_write_parallel-simple_model.obj `if test -f 'simple_model.c'; then $(CYGPATH_W) 'simple_model.c'; else $(CYGPATH_W) '$(srcdir)/simple_model.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_parallel-simple_model.Tpo $(DEPDIR)/pio_write_parallel-simple_model.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='simple_model.c' object='pio_write_parallel-simple_model.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_parallel-simple_model.obj `if test -f 'simple_model.c'; then $(CYGPATH_W) 'simple_model.c'; else $(CYGPATH_W) '$(srcdir)/simple_model.c'; fi`

pio_write_parallel-simple_model_helper.o: simple_model_helper.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_parallel-simple_model_helper.o -MD -MP -MF $(DEPDIR)/pio_write_parallel-simple_model_helper.Tpo -c -o pio_write_parallel-simple_model_helper.o `test -f 'simple_model_helper.c' || echo '$(srcdir)/'`simple_model_helper.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_parallel-simple_model_helper.Tpo $(DEPDIR)/pio_write_parallel-simple_model_helper.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='simple_model_helper.c' object='pio_write_parallel-simple_model_helper.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_parallel-simple_model_helper.o `test -f 'simple_model_helper.c' || echo '$(srcdir)/'`simple_model_helper.c

pio_write_parallel-simple_model_helper.obj: simple_model_helper.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT pio_write_parallel-simple_model_helper.obj -MD -MP -MF $(DEPDIR)/pio_write_parallel-simple_model_helper.Tpo -c -o pio_write_parallel-simple_model_helper.obj `if test -f 'simple_model_helper.c'; then $(CYGPATH_W) 'simple_model_helper.c'; else $(CYGPATH_W) '$(srcdir)/simple_model_helper.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/pio_write_parallel-simple_model_helper.Tpo $(DEPDIR)/pio_write_parallel-simple_model_helper.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='simple_model_helper.c' object='pio_write_parallel-simple_model_helper.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(pio_write_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o pio_write_parallel-simple_model_helper.obj `if test -f 'simple_model_helper.c'; then $(CYGPATH_W) 'simple_model_helper.c'; else $(CYGPATH_W) '$(srcdir)/simple_model_helper.c'; fi`
