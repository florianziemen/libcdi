## Process this file with automake to produce Makefile.in
#
  noinst_PROGRAMS = cdi_write cdi_read cdi_copy
if CREATE_ISOC
  noinst_PROGRAMS += cdi_read_f2003 cdi_write_f2003
endif
#
cdi_write_SOURCES = cdi_write.c
cdi_write_LDADD = ../src/libcdi.a $(LDFLAGS)
#
cdi_read_SOURCES = cdi_read.c
cdi_read_LDADD = ../src/libcdi.a $(LDFLAGS)
#
cdi_copy_SOURCES = cdi_copy.c
cdi_copy_LDADD = ../src/libcdi.a $(LDFLAGS)
#
AM_CPPFLAGS = -I$(top_srcdir)/src
#
cdi_read_f2003: cdi_read_f2003.f90
	$(FC) $(AM_FCFLAGS) $(FCFLAGS) $(FCFLAGS_f90) $@.f90 ../src/cdilib.o ../src/mo_cdi.o -I../src $(LDFLAGS) $(LIBS) -o $@
#
cdi_write_f2003: cdi_write_f2003.f90
	$(FC) $(AM_FCFLAGS) $(FCFLAGS) $(FCFLAGS_f90) $@.f90 ../src/cdilib.o ../src/mo_cdi.o -I../src $(LDFLAGS) $(LIBS) -o $@
#
CLEANFILES  = `ls *~`
