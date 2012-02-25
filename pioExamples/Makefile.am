noinst_PROGRAMS=collectData collectData2003 collectDataNStreams

if USE_MPI
noinst_PROGRAMS+=compareResourcesArray 
endif

AM_CFLAGS=-I$(top_srcdir)/src

collectData_SOURCES=collectData.c

collectData_LDADD=-L../src -lcdi

collectData2003_SOURCES=collectData2003.F90

collectData2003_LDADD=-L../src -lcdi

collectDataNStreams_SOURCES=collectDataNStreams.c

collectDataNStreams_LDADD=-L../src -lcdi

compareResourcesArray_SOURCES=compareResourcesArray.c

compareResourcesArray_LDADD=-L../src -lcdi