noinst_PROGRAMS =

if ENABLE_MPI
noinst_PROGRAMS += \
  no_link_to_libcdi \
  must_fail
endif

AM_CPPFLAGS = -I$(top_srcdir)/src $(YAXT_C_INCLUDE)
LDADD = $(top_builddir)/src/libcdipio.la
