## Process this file with automake to produce Makefile.in
#
bin_PROGRAMS = cdi
#
noinst_PROGRAMS = createtable
#
cdi_SOURCES = cdi.c printinfo.h
#
cdi_LDADD = ../src/libcdi.a $(LDFLAGS)
#
createtable_SOURCES = createtable.c
#
createtable_LDADD = ../src/libcdi.a $(LDFLAGS)
#
AM_CPPFLAGS = -I$(top_srcdir)/src
#
CLEANFILES  = `ls *~`
