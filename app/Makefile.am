## Process this file with automake to produce Makefile.in
#
bin_PROGRAMS    =
noinst_PROGRAMS =
if ENABLE_CDI_LIB
  bin_PROGRAMS    += cdi
else
  noinst_PROGRAMS += cdi
endif
#
if CREATE_ALL_STATIC
bin_PROGRAMS       += cdi_static
endif
#
noinst_PROGRAMS    += createtable
#
cdi_SOURCES         = cdi.c printinfo.h
cdi_LDADD           = $(top_builddir)/src/libcdi.la
#
cdi_static_SOURCES  = cdi.c printinfo.h
cdi_static_LDADD    = $(top_builddir)/src/libcdi.la
cdi_static_LDFLAGS  = -all-static

createtable_SOURCES = createtable.c
createtable_LDADD   = $(top_builddir)/src/libcdi.la
#
AM_CPPFLAGS         = -I$(top_srcdir)/src
#
CLEANFILES          = `ls *~`
