SUBDIRS = src interfaces app examples examples/pio tests

doc/cdi_cman.pdf:
	(cd $(top_srcdir)/doc/tex ; ./makepdf_c ; mv cdi_cman.pdf .. ; ./cleanup)

doc/cdi_fman.pdf:
	(cd $(top_srcdir)/doc/tex ; ./makepdf_f ; mv cdi_fman.pdf .. ; ./cleanup)

CLEANFILES  = `ls *~`

EXTRA_DIST = \
  config/default \
  config/interface.rb \
  doc/cdi_cman.pdf \
  doc/cdi_fman.pdf \
  src/cfortran.doc \
  util/sunf95preproc-wrapper \
  util/sxpreproc-wrapper \
  util/xlfpreproc-wrapper

ACLOCAL_AMFLAGS = -I m4
