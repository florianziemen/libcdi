noinst_PROGRAMS =
CLEANFILES =

EXTRA_PROGRAMS = \
  collectData \
  collectDataNStreams

if ENABLE_MPI
EXTRA_PROGRAMS += \
  collectData.parallel \
  collectDataNStreams.parallel \
  compareResourcesArray
endif ENABLE_MPI

if ENABLE_CF_INTERFACE
if BUILD_FC_PROGRAMS
EXTRA_PROGRAMS += collectData2003
endif BUILD_FC_PROGRAMS
if BUILD_PIO_FC_PROGRAMS
EXTRA_PROGRAMS += collectData2003.parallel
endif BUILD_PIO_FC_PROGRAMS
endif ENABLE_CF_INTERFACE

if with_example_programs
noinst_PROGRAMS += $(EXTRA_PROGRAMS)
else !with_example_programs
CLEANFILES += $(EXTRA_PROGRAMS)
endif !with_example_programs
examples-local: $(EXTRA_PROGRAMS)

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
nodist_collectData_parallel_SOURCES = $(collectData_SOURCES:.c=.parallel.c)
collectData_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(YAXT_C_LIB) $(LDADD)

collectDataNStreams_SOURCES = collectDataNStreams.c
nodist_collectDataNStreams_parallel_SOURCES = $(collectDataNStreams_SOURCES:.c=.parallel.c)
collectDataNStreams_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(YAXT_C_LIB) $(LDADD)

compareResourcesArray_SOURCES = compareResourcesArray.c
compareResourcesArray_LDADD = $(top_builddir)/src/libcdipio.la $(YAXT_C_LIB) $(LDADD)

collectData2003_SOURCES = collectData2003.F90
nodist_collectData2003_parallel_SOURCES = $(collectData2003_SOURCES:.F90=.parallel.F90)
collectData2003_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(YAXT_FC_LIB) $(MPI_FC_LIB) $(LDADD)

clean-local:
	-rm -f *.parallel.c *.parallel.F90
	-rm -f *.grb
	-rm -rf *.dSYM

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
SUFFIXES = .parallel.c .parallel.F90

.c.parallel.c:
	$(AM_V_GEN):;{ \
	  echo '#define USE_MPI 1'; \
	  echo '#include "$<"'; \
	  } >$@

.F90.parallel.F90:
	$(AM_V_GEN):;{ \
	  echo '#define USE_MPI 1'; \
	  echo '#include "$<"'; \
	  } >$@
