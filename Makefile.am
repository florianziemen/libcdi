SUBDIRS = src interfaces app examples tests

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST =          \
  config/interface.rb \
  doc/cdi_cman.pdf    \
  doc/cdi_fman.pdf    \
  src/cfortran.doc

.NOTPARALLEL:

doc/cdi_cman.pdf:
	$(AM_V_GEN)$(am__cd) $(top_srcdir)/doc/tex && ./makepdf_c && mv cdi_cman.pdf .. && ./cleanup

doc/cdi_fman.pdf:
	$(AM_V_GEN)$(am__cd) $(top_srcdir)/doc/tex && ./makepdf_f && mv cdi_fman.pdf .. && ./cleanup
