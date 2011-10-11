noinst_PROGRAMS=collectData

if USE_MPI
noinst_PROGRAMS+=compareResourcesArray 
endif

AM_CFLAGS=-I$(top_srcdir)/src

collectData_SOURCES=collectData.c

collectData_LDADD=-L../src -lcdi

compareResourcesArray_SOURCES=compareResourcesArray.c

compareResourcesArray_LDADD=-L../src -lcdi