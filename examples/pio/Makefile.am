check_PROGRAMS=collectData collectDataNStreams

AM_CPPFLAGS = -I$(top_srcdir)/src $(YAXT_CFLAGS)
AM_FCFLAGS  = $(FPP_INCOPT)$(top_srcdir)/src $(MPI_FC_INCLUDE)
AM_CFLAGS = $(MPI_C_INCLUDE)

if USE_MPI
check_PROGRAMS+=compareResourcesArray
AM_FCFLAGS += $(FPP_DEFOPT)USE_MPI
endif

if USE_FC
check_PROGRAMS+=collectData2003
endif

collectData_SOURCES=collectData.c

if USE_MPI
LDADD=$(top_builddir)/src/libcdipio.la $(MPI_C_LIB)
collectData2003_LDADD=$(top_builddir)/src/libcdipio.la $(YAXT_FC_LIB) $(MPI_FC_LIB)
else
LDADD=$(top_builddir)/src/libcdi.la
collectData2003_LDADD=$(top_builddir)/src/libcdi.la
endif

collectData2003_SOURCES=collectData2003.F90

collectDataNStreams_SOURCES = collectDataNStreams.c

compareResourcesArray_SOURCES = compareResourcesArray.c
#
clean-local: clean-local-dirs
.PHONY: clean-local-dirs
clean-local-dirs:
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
PPFCCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(FC) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_FCFLAGS) $(FCFLAGS)
