check_PROGRAMS = \
  collectData \
  collectDataNStreams

if ENABLE_MPI
check_PROGRAMS += compareResourcesArray
endif ENABLE_MPI

if ENABLE_CF_INTERFACE
check_PROGRAMS += collectData2003
endif ENABLE_CF_INTERFACE

AM_CPPFLAGS = -I$(top_srcdir)/src $(YAXT_C_INCLUDE)
if ENABLE_MPI
AM_CPPFLAGS += -DUSE_MPI
endif ENABLE_MPI
AM_CFLAGS = $(MPI_C_INCLUDE)

AM_FCFLAGS = $(FC_OPTINC)$(top_srcdir)/src $(MPI_FC_INCLUDE)
if ENABLE_MPI
AM_FCFLAGS += $(FC_DEFINE)USE_MPI
endif ENABLE_MPI

AM_LDFLAGS =
if ENABLE_ALL_STATIC
AM_LDFLAGS += -all-static
endif

LDADD = $(top_builddir)/src/libcdi.la
if ENABLE_MPI
LDADD += $(top_builddir)/src/libcdipio.la
endif ENABLE_MPI

collectData_SOURCES = collectData.c

collectDataNStreams_SOURCES = collectDataNStreams.c

compareResourcesArray_SOURCES = compareResourcesArray.c

collectData2003_SOURCES = collectData2003.F90
collectData2003_LDADD = $(LDADD) $(YAXT_FC_LIB) $(MPI_FC_LIB)

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
