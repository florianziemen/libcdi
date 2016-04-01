## Process this file with automake to produce Makefile.in
#
bin_PROGRAMS    =
noinst_PROGRAMS =
check_PROGRAMS  =
if ENABLE_CDI_APP
if ENABLE_CDI_LIB
bin_PROGRAMS += cdi
else !ENABLE_CDI_LIB
noinst_PROGRAMS += cdi
endif !ENABLE_CDI_LIB
noinst_PROGRAMS += createtable
else !ENABLE_CDI_APP
check_PROGRAMS += cdi
endif !ENABLE_CDI_APP
#
#
cdi_SOURCES         = cdi.c printinfo.h printinfo_extern.c
cdi_LDADD           = $(top_builddir)/src/libcdi.la @LIBS@
if ENABLE_ALL_STATIC
cdi_LDFLAGS         = -all-static
endif
#
createtable_SOURCES = createtable.c
createtable_LDADD   = $(top_builddir)/src/libcdi.la
if ENABLE_ALL_STATIC
createtable_LDFLAGS = -all-static
endif
#
AM_CPPFLAGS         = -I$(top_srcdir)/src
#
CLEANFILES          = `ls *~`
#
clean-local: clean-local-dirs
.PHONY: clean-local-dirs
clean-local-dirs:
	-rm -rf *.dSYM
