noinst_PROGRAMS=myModel

if USE_MPI
noinst_PROGRAMS+=pio_write myModel2003 compareResourcesArray
endif

AM_CFLAGS=-I$(top_srcdir)/src

pio_write_SOURCES=pio_write.F90

pio_write_LDADD=-L../src -lcdi

myModel_SOURCES=myModel.c

myModel_LDADD=-L../src -lcdi

myModel2003_SOURCES=myModel2003.F90

myModel2003_LDADD=-L../src -lcdi

compareResourcesArray_SOURCES=compareResourcesArray.c

compareResourcesArray_LDADD=-L../src -lcdi