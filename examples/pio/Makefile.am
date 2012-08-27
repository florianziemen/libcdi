noinst_PROGRAMS=collectData collectDataNStreams

if USE_MPI
noinst_PROGRAMS+=compareResourcesArray
endif

if USE_FC
 noinst_PROGRAMS+=collectData2003
endif

AM_CFLAGS   = -I$(top_srcdir)/src
AM_FCFLAGS  = $(FPP_INCOPT)$(top_srcdir)/src

collectData_SOURCES=collectData.c

collectData_LDADD=$(top_builddir)/src/libcdi.la

collectData2003_SOURCES=collectData2003.F90

collectData2003_LDADD=$(top_builddir)/src/libcdi.la

collectDataNStreams_SOURCES=collectDataNStreams.c

collectDataNStreams_LDADD=$(top_builddir)/src/libcdi.la

compareResourcesArray_SOURCES=compareResourcesArray.c

compareResourcesArray_LDADD=$(top_builddir)/src/libcdi.la
