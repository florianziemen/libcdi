noinst_PROGRAMS = \
  collectDataNStreams_serial \
  collectData_serial

if ENABLE_MPI
noinst_PROGRAMS += \
  collectDataNStreams_parallel \
  collectData_parallel \
  compareResourcesArray
endif ENABLE_MPI

if ENABLE_CF_INTERFACE
noinst_PROGRAMS += collectData2003_serial
if BUILD_PIO_FC_PROGRAMS
noinst_PROGRAMS += collectData2003_parallel
endif BUILD_PIO_FC_PROGRAMS
endif ENABLE_CF_INTERFACE

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

collectDataNStreams_serial_SOURCES = collectDataNStreams.c

collectDataNStreams_parallel_SOURCES = $(collectDataNStreams_serial_SOURCES)
collectDataNStreams_parallel_CPPFLAGS = -DUSE_MPI $(AM_CPPFLAGS)
collectDataNStreams_parallel_LDADD = $(top_builddir)/src/libcdipio.la

collectData_serial_SOURCES = collectData.c

collectData_parallel_SOURCES = $(collectData_serial_SOURCES)
collectData_parallel_CPPFLAGS = -DUSE_MPI $(AM_CPPFLAGS)
collectData_parallel_LDADD = $(top_builddir)/src/libcdipio.la

compareResourcesArray_SOURCES = compareResourcesArray.c
compareResourcesArray_LDADD = $(top_builddir)/src/libcdipio.la

collectData2003_serial_SOURCES = collectData2003.F90

collectData2003_parallel_SOURCES = $(collectData2003_serial_SOURCES)
collectData2003_parallel_FCFLAGS = $(FC_DEFINE)USE_MPI $(AM_FCFLAGS)
collectData2003_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(YAXT_FC_LIB) $(MPI_FC_LIB)

clean-local:
	-rm -f *.grb
	-rm -rf *.dSYM

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
