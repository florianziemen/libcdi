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
if with_on_demand_check_programs
check_PROGRAMS += cdi
else !with_on_demand_check_programs
noinst_PROGRAMS += cdi
endif !with_on_demand_check_programs
endif !ENABLE_CDI_APP

AM_CPPFLAGS = -I$(top_srcdir)/src

AM_LDFLAGS =
if ENABLE_ALL_STATIC
AM_LDFLAGS += -all-static
endif

LDADD = $(top_builddir)/src/libcdi.la

cdi_SOURCES =        \
  cdi.c              \
  printinfo.h        \
  printinfo_extern.c

createtable_SOURCES = \
  createtable.c

clean-local:
	-rm -rf *.dSYM

# Compile programs using Libtool:
COMPILE = $(LTCOMPILE)
FCCOMPILE = $(LTFCCOMPILE)
PPFCCOMPILE = $(LTPPFCCOMPILE)
CXXCOMPILE = $(LTCXXCOMPILE)

#
# Avoid compiling twice by running Libtool with '-static'
#   This is safe because none of the Libtool libraries in this directory is
#   installed:
#

LTCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CC $(AM_LIBTOOLFLAGS) \
	$(LIBTOOLFLAGS) --mode=compile $(CC) -static $(DEFS) \
	$(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
	$(AM_CFLAGS) $(CFLAGS)

LTFCCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=FC $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(FC) -static $(AM_FCFLAGS) $(FCFLAGS)

# Also compile *.F90 without CPPFLAGS, which are normally meant for the C
# compiler and might not be compatible with the Fortran compiler:
LTPPFCCOMPILE = $(LTFCCOMPILE)

LTCXXCOMPILE = $(LIBTOOL) $(AM_V_lt) --tag=CXX $(AM_LIBTOOLFLAGS) \
        $(LIBTOOLFLAGS) --mode=compile $(CXX) -static $(DEFS) \
        $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
        $(AM_CXXFLAGS) $(CXXFLAGS)
