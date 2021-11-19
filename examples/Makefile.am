SUBDIRS = pio

check_PROGRAMS = \
  cdi_append \
  cdi_copy \
  cdi_read \
  cdi_write \
  cdi_write_ens \
  cdi_write_hybrid \
  cdi_write_local \
  cdi_write_relativ

if ENABLE_NETCDF
check_PROGRAMS += cdi_write_const
endif

if ENABLE_ISOC_INTERFACE
check_PROGRAMS += cdi_read_f2003 cdi_write_f2003
endif ENABLE_ISOC_INTERFACE

AM_CPPFLAGS = -I$(top_srcdir)/src
AM_FCFLAGS  = $(FC_OPTINC)$(top_srcdir)/src $(FC_MOD_FLAG)../src

AM_LDFLAGS =
if ENABLE_ALL_STATIC
AM_LDFLAGS += -all-static
endif

LDADD = $(top_builddir)/src/libcdi.la

cdi_append_SOURCES = cdi_append.c

cdi_copy_SOURCES = cdi_copy.c

cdi_read_SOURCES = cdi_read.c

cdi_write_SOURCES = cdi_write.c

cdi_write_ens_SOURCES = cdi_write_ens.c

cdi_write_hybrid_SOURCES = cdi_write_hybrid.c

cdi_write_local_SOURCES = cdi_write_local.c

cdi_write_relativ_SOURCES = cdi_write_relativ.c

cdi_write_const_SOURCES = cdi_write_const.c

cdi_read_f2003_SOURCES = cdi_read_f2003.f90
cdi_read_f2003_LDADD = $(top_builddir)/src/libcdi_f2003.la

cdi_write_f2003_SOURCES = cdi_write_f2003.f90
cdi_write_f2003_LDADD = $(top_builddir)/src/libcdi_f2003.la

if FC_MOD_UPPERCASE
mo_cdi_mod = MO_CDI.$(FCMODEXT)
else !FC_MOD_UPPERCASE
mo_cdi_mod = mo_cdi.$(FCMODEXT)
endif !FC_MOD_UPPERCASE

cdi_read_f2003.$(OBJEXT) cdi_write_f2003.$(OBJEXT): $(top_builddir)/src/$(mo_cdi_mod)

clean-local:
	-rm -f *.grb *.grb2 *.nc
	-rm -rf *.dSYM

# overrides to make regular compilations also use libtool
# static because the libtool libraries in this directory are meant to
# be statically linked and there is no use for the PIC object
COMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CC) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_CFLAGS) $(CFLAGS)
FCCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(FC) -static $(AM_FCFLAGS) $(FCFLAGS)
CXXCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CXX) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_CXXFLAGS) $(CXXFLAGS)
