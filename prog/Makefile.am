## Process this file with automake to produce Makefile.in
#
bin_PROGRAMS = cdi createtable
#
cdi_SOURCES = cdi.c
#
cdi_LDADD = $(top_srcdir)/src/libcdi.a $(LDFLAGS) -lm
#
createtable_SOURCES = createtable.c
#
createtable_LDADD = $(top_srcdir)/src/libcdi.a $(LDFLAGS) -lm
#
CPPFLAGS = -I$(top_srcdir)/src
#
CLEANFILES  = `ls *~`
