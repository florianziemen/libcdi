## Process this file with automake to produce Makefile.in
#
bin_PROGRAMS = cdi_write cdi_read cdi_copy
#
cdi_write_SOURCES = cdi_write.c
cdi_write_LDADD = $(top_srcdir)/src/libcdi.a $(LDFLAGS) -lm
#
cdi_read_SOURCES = cdi_read.c
cdi_read_LDADD = $(top_srcdir)/src/libcdi.a $(LDFLAGS) -lm
#
cdi_copy_SOURCES = cdi_copy.c
cdi_copy_LDADD = $(top_srcdir)/src/libcdi.a $(LDFLAGS) -lm
#
CPPFLAGS = -I$(top_srcdir)/src
#
CLEANFILES  = `ls *~`
