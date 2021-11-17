noinst_PROGRAMS =

EXTRA_PROGRAMS = \
  collectData \
  collectDataNStreams

if ENABLE_MPI
EXTRA_PROGRAMS += \
  collectDataNStreams_parallel \
  collectData_parallel \
  compareResourcesArray
endif ENABLE_MPI

if ENABLE_CF_INTERFACE
if BUILD_FC_PROGRAMS
EXTRA_PROGRAMS += collectData2003
endif BUILD_FC_PROGRAMS
if BUILD_PIO_FC_PROGRAMS
EXTRA_PROGRAMS += collectData2003_parallel
endif BUILD_PIO_FC_PROGRAMS
endif ENABLE_CF_INTERFACE

if with_example_programs
noinst_PROGRAMS += $(EXTRA_PROGRAMS)
endif with_example_programs
examples: $(EXTRA_PROGRAMS)

AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS = $(YAXT_C_INCLUDE) $(MPI_C_INCLUDE)

AM_FCFLAGS =
if ENABLE_CF_INTERFACE
AM_FCFLAGS += $(FC_OPTINC)$(top_srcdir)/src
endif ENABLE_CF_INTERFACE
AM_FCFLAGS += $(YAXT_FC_MOD) $(MPI_FC_MOD)

AM_LDFLAGS =
if ENABLE_ALL_STATIC
AM_LDFLAGS += -all-static
endif

LDADD = $(top_builddir)/src/libcdi.la

collectData_SOURCES = collectData.c
nodist_collectData_parallel_SOURCES = collectData.parallel.c
collectData_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

collectDataNStreams_SOURCES = collectDataNStreams.c
nodist_collectDataNStreams_parallel_SOURCES = collectDataNStreams.parallel.c
collectDataNStreams_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

compareResourcesArray_SOURCES = compareResourcesArray.c
compareResourcesArray_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

collectData2003_SOURCES = collectData2003.F90
nodist_collectData2003_parallel_SOURCES = collectData2003.parallel.F90
collectData2003_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_FC_LIB) $(MPI_FC_LIB)

clean-local:
	-rm -f *.parallel.c *.parallel.F90
	-rm -f *.grb
	-rm -rf *.dSYM

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

# Generate source files for parallel versions of the programs. We do this to
# avoid target-specific flags, which result into rules that do not use the
# compilation commands above.
SUFFIXES = .parallel.c .parallel.F90

.c.parallel.c:
	$(AM_V_GEN):;{ echo '#define USE_MPI 1' && cat $<; }>$@

.F90.parallel.F90:
	$(AM_V_GEN):;{ echo '#define USE_MPI 1' && cat $<; }>$@
