noinst_PROGRAMS =

EXTRA_PROGRAMS =             \
  collectDataNStreams_serial \
  collectData_serial

if ENABLE_MPI
EXTRA_PROGRAMS +=              \
  collectDataNStreams_parallel \
  collectData_parallel         \
  compareResourcesArray
endif ENABLE_MPI

if ENABLE_CF_INTERFACE
EXTRA_PROGRAMS += collectData2003_serial
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

collectDataNStreams_serial_SOURCES = \
  collectDataNStreams.c

collectDataNStreams_parallel_SOURCES = \
  $(collectDataNStreams_serial_SOURCES)
collectDataNStreams_parallel_CPPFLAGS = -DUSE_MPI $(AM_CPPFLAGS)
collectDataNStreams_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

collectData_serial_SOURCES = collectData.c

collectData_parallel_SOURCES = $(collectData_serial_SOURCES)
collectData_parallel_CPPFLAGS = -DUSE_MPI $(AM_CPPFLAGS)
collectData_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

compareResourcesArray_SOURCES = compareResourcesArray.c
compareResourcesArray_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_C_LIB)

collectData2003_serial_SOURCES = collectData2003.F90

collectData2003_parallel_SOURCES = \
  $(collectData2003_serial_SOURCES)
collectData2003_parallel_FCFLAGS = $(FC_DEFINE)USE_MPI $(AM_FCFLAGS)
collectData2003_parallel_LDADD = $(top_builddir)/src/libcdipio.la $(PPM_CORE_C_LIB) $(YAXT_FC_LIB) $(MPI_FC_LIB)

clean-local:
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

#
# TODO: the following is too hard to maintain, we should try to eliminate the
# need for this:
#

collectDataNStreams_parallel-collectDataNStreams.o: collectDataNStreams.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(collectDataNStreams_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT collectDataNStreams_parallel-collectDataNStreams.o -MD -MP -MF $(DEPDIR)/collectDataNStreams_parallel-collectDataNStreams.Tpo -c -o collectDataNStreams_parallel-collectDataNStreams.o `test -f 'collectDataNStreams.c' || echo '$(srcdir)/'`collectDataNStreams.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/collectDataNStreams_parallel-collectDataNStreams.Tpo $(DEPDIR)/collectDataNStreams_parallel-collectDataNStreams.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='collectDataNStreams.c' object='collectDataNStreams_parallel-collectDataNStreams.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(collectDataNStreams_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o collectDataNStreams_parallel-collectDataNStreams.o `test -f 'collectDataNStreams.c' || echo '$(srcdir)/'`collectDataNStreams.c

collectDataNStreams_parallel-collectDataNStreams.obj: collectDataNStreams.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(collectDataNStreams_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT collectDataNStreams_parallel-collectDataNStreams.obj -MD -MP -MF $(DEPDIR)/collectDataNStreams_parallel-collectDataNStreams.Tpo -c -o collectDataNStreams_parallel-collectDataNStreams.obj `if test -f 'collectDataNStreams.c'; then $(CYGPATH_W) 'collectDataNStreams.c'; else $(CYGPATH_W) '$(srcdir)/collectDataNStreams.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/collectDataNStreams_parallel-collectDataNStreams.Tpo $(DEPDIR)/collectDataNStreams_parallel-collectDataNStreams.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='collectDataNStreams.c' object='collectDataNStreams_parallel-collectDataNStreams.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(collectDataNStreams_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o collectDataNStreams_parallel-collectDataNStreams.obj `if test -f 'collectDataNStreams.c'; then $(CYGPATH_W) 'collectDataNStreams.c'; else $(CYGPATH_W) '$(srcdir)/collectDataNStreams.c'; fi`

collectData_parallel-collectData.o: collectData.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(collectData_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT collectData_parallel-collectData.o -MD -MP -MF $(DEPDIR)/collectData_parallel-collectData.Tpo -c -o collectData_parallel-collectData.o `test -f 'collectData.c' || echo '$(srcdir)/'`collectData.c
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/collectData_parallel-collectData.Tpo $(DEPDIR)/collectData_parallel-collectData.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='collectData.c' object='collectData_parallel-collectData.o' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(collectData_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o collectData_parallel-collectData.o `test -f 'collectData.c' || echo '$(srcdir)/'`collectData.c

collectData_parallel-collectData.obj: collectData.c
@am__fastdepCC_TRUE@	$(AM_V_CC)$(LTCOMPILE_prefix) $(collectData_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -MT collectData_parallel-collectData.obj -MD -MP -MF $(DEPDIR)/collectData_parallel-collectData.Tpo -c -o collectData_parallel-collectData.obj `if test -f 'collectData.c'; then $(CYGPATH_W) 'collectData.c'; else $(CYGPATH_W) '$(srcdir)/collectData.c'; fi`
@am__fastdepCC_TRUE@	$(AM_V_at)$(am__mv) $(DEPDIR)/collectData_parallel-collectData.Tpo $(DEPDIR)/collectData_parallel-collectData.Po
@AMDEP_TRUE@@am__fastdepCC_FALSE@	$(AM_V_CC)source='collectData.c' object='collectData_parallel-collectData.obj' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@@am__fastdepCC_FALSE@	DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) @AMDEPBACKSLASH@
@am__fastdepCC_FALSE@	$(AM_V_CC@am__nodep@)$(LTCOMPILE_prefix) $(collectData_parallel_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o collectData_parallel-collectData.obj `if test -f 'collectData.c'; then $(CYGPATH_W) 'collectData.c'; else $(CYGPATH_W) '$(srcdir)/collectData.c'; fi`

collectData2003_parallel-collectData2003.o: collectData2003.F90
	$(AM_V_PPFC)$(LTFCCOMPILE_prefix) $(collectData2003_parallel_FCFLAGS) $(FCFLAGS) -c -o collectData2003_parallel-collectData2003.o `test -f 'collectData2003.F90' || echo '$(srcdir)/'`collectData2003.F90

collectData2003_parallel-collectData2003.obj: collectData2003.F90
	$(AM_V_PPFC)$(LTFCCOMPILE_prefix) $(collectData2003_parallel_FCFLAGS) $(FCFLAGS) -c -o collectData2003_parallel-collectData2003.obj `if test -f 'collectData2003.F90'; then $(CYGPATH_W) 'collectData2003.F90'; else $(CYGPATH_W) '$(srcdir)/collectData2003.F90'; fi`
