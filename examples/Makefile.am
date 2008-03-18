## Process this file with automake to produce Makefile.in
#
noinst_PROGRAMS = cdi_write cdi_read cdi_copy
#
cdi_write_SOURCES = cdi_write.c
cdi_write_LDADD = ../src/libcdi.a $(LDFLAGS) -lm
#
cdi_read_SOURCES = cdi_read.c
cdi_read_LDADD = ../src/libcdi.a $(LDFLAGS) -lm
#
cdi_copy_SOURCES = cdi_copy.c
cdi_copy_LDADD = ../src/libcdi.a $(LDFLAGS) -lm
#
AM_CPPFLAGS = -I$(top_srcdir)/src
#
CLEANFILES  = `ls *~`
