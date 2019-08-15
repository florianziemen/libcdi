## Process this file with automake to produce Makefile.in
#
SWIG_FILE = $(abs_top_srcdir)/interfaces/cdi.i
BINDINGS  =
CLEANFILES =
BINDING_TESTS =

EXTRA_DIST = mulval.nc.gz

INTERFACES_FILES = \
	$(abs_top_srcdir)/interfaces/ruby/test.rb \
	$(abs_top_srcdir)/interfaces/ruby/setup.rb \
	$(abs_top_srcdir)/interfaces/python/test.py \
	$(abs_top_srcdir)/interfaces/python/setup.py

MODULE_NAME         = Cdi
RUBY_WRAPPER_FILE   = $(abs_top_builddir)/interfaces/ruby/cdi_wrapper.cpp
RUBY_SHARED_LIB     = $(abs_top_builddir)/interfaces/ruby/$(MODULE_NAME).so
RUBY_MAKEFILE       = $(abs_top_builddir)/interfaces/ruby/Makefile
PYTHON_WRAPPER_FILE = $(abs_top_builddir)/interfaces/python/cdi_wrapper.cpp
PYTHON_SHARED_LIB   = $(abs_top_builddir)/interfaces/python/$(MODULE_NAME).so
PYTHON_MAKEFILE     = $(abs_top_builddir)/interfaces/python/Makefile

CLEANFILES += $(RUBY_WRAPPER_FILE) $(RUBY_MAKEFILE) $(RUBY_SHARED_LIB) $(PYTHON_MAKEFILE) $(PYTHON_SHARED_LIB) $(PYTHON_WRAPPER_FILE)

bin_PROGRAMS =
noinst_PROGRAMS = 
noinst_LTLIBRARIES = 
if CREATE_INTERFACES
noinst_PROGRAMS += CdiInfo
noinst_LTLIBRARIES += libcdipp.la
endif
#
locallibs = $(abs_top_builddir)/src/.libs/libcdi.so @LTLIBOBJS@ @NETCDF_LIBS@ @SZLIB_LIBS@

libcdipp_la_SOURCES = cdi.cpp cdi.hpp
libcdipp_la_LIBADD  = $(top_builddir)/src/libcdi.la
libcdipp_la_LDFLAGS = @NETCDF_LIBS@ @SZLIB_LIBS@ $(LDFLAGS)

CdiInfo_SOURCES     = CdiInfo.cpp mulval.nc
CdiInfo_LDADD       = libcdipp.la
CdiInfo_LDFLAGS     = @NETCDF_LIBS@ @SZLIB_LIBS@ $(LDFLAGS)

mulval.nc: $(abs_top_srcdir)/interfaces/mulval.nc.gz
	gunzip -c $< > $@
#
#
if ENABLE_RUBY
BINDINGS += ruby-binding
BINDING_TESTS += ruby-test
# Ruby ====================================================
$(RUBY_WRAPPER_FILE): $(SWIG_FILE)
	mkdir -p $(dir $@)
	$(SWIG) -ruby -c++ -globalmodule -debug-classes -o $@ $(AM_CPPFLAGS) $<

$(RUBY_MAKEFILE): $(abs_top_srcdir)/interfaces/ruby/setup.rb
	mkdir -p $(dir $@)
	cd ruby && CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(LDFLAGS)" INCFLAGS="-I$(abs_top_srcdir)/src -I$(abs_top_srcdir)/interfaces" ruby $<

$(RUBY_SHARED_LIB): $(RUBY_WRAPPER_FILE) $(RUBY_MAKEFILE)
	cd ruby && make

ruby-binding: $(RUBY_SHARED_LIB)

ruby-test: $(RUBY_SHARED_LIB) mulval.nc
	cd ruby && ruby $(abs_top_srcdir)/interfaces/ruby/test.rb

install-ruby: $(RUBY_SHARED_LIB)
	cd ruby && make install
endif
# =========================================================
if ENABLE_PYTHON
BINDINGS += python-binding
BINDING_TESTS += python-test
# Python ==================================================
$(PYTHON_WRAPPER_FILE): $(SWIG_FILE)
	mkdir -p $(dir $@)
	$(SWIG) -python -c++ -debug-classes -o $(PYTHON_WRAPPER_FILE) $(AM_CPPFLAGS) $<

$(PYTHON_SHARED_LIB): $(PYTHON_WRAPPER_FILE) $(abs_top_srcdir)/interfaces/python/setup.py
	mkdir -p $(dir $@)
	cd python; CC="$(CC)" CXX="$(CXX)" BUILDLIBDIR="$(top_builddir)/src/.libs" LIBDIR=$(libdir) CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="-L../../src/.libs $(LDFLAGS)" INCFLAGS="-I$(abs_top_srcdir)/src -I$(abs_top_srcdir)/interfaces" python $(abs_top_srcdir)/interfaces/python/setup.py build_ext --inplace -R "$(abs_top_builddir)/src/.libs"

python-binding: $(PYTHON_SHARED_LIB)

python-test: $(abs_top_srcdir)/interfaces/python/test.py mulval.nc
	@cd python; python $<

install-python: $(PYTHON_SHARED_LIB) mulval.nc
	cd python; CC="$(CC)" CXX="$(CXX)" BUILDLIBDIR="$(top_builddir)/src/.libs" LIBDIR=$(libdir) CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="-L../../src/.libs $(LDFLAGS)" INCFLAGS="-I$(abs_top_srcdir)/src -I$(abs_top_srcdir)/interfaces" python $(abs_top_srcdir)/interfaces/python/setup.py install --prefix=$(prefix) -R $(libdir)
endif
#
AM_CPPFLAGS = -I$(top_srcdir)/src
#
all-local: $(BINDINGS)
#
test: $(BINDING_TESTS)
#
CLEANFILES  += `ls *~ *.o *.la`
