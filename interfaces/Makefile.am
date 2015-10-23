## Process this file with automake to produce Makefile.in
#
SWIGCDILIB = cdilib.i
SWIGCDIOBJ = cdiobj.i
BINDINGS   =
BINDINGS_INSTALL =
BINDINGS_TESTS =

EXTRA_DIST = $(SWIGCDIOBJ) $(SWIGCDILIB) \
	\
	ruby/cdilib_wrap.c ruby/cdiobj_wrap.cpp ruby/testLib.rb \
	ruby/testObj.rb \
	\
	python/cdilib_wrap.c python/cdiobj_wrap.cpp python/testLib.py \
	python/testObj.py python/setupObj.py python/setupLib.py \
	python/setup.py python/table.py python/CdiLib.py \
	python/CdiObj.py \
	\
	CdiInfo.cpp \
	\
	f2003/bindGen.rb

#
bin_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =
if CREATE_INTERFACES
noinst_PROGRAMS += CdiInfo
lib_LTLIBRARIES += libcdipp.la
endif
#
locallibs = $(top_builddir)/src/.libs/libcdi.so @LTLIBOBJS@ @NETCDF_LIBS@ @HDF5_LIBS@ @SZLIB_LIBS@ @ZLIB_LIBS@

libcdipp_la_SOURCES = cdi.cpp cdi.hpp
libcdipp_la_LIBADD  = $(top_builddir)/src/libcdi.la

CdiInfo_SOURCES     = CdiInfo.cpp
CdiInfo_LDADD       = libcdipp.la
#
#
select_test_file = \
	`if test $(ENABLE_NETCDF) = yes ; then test -f ../testdata/mulval.nc && echo ../testdata/mulval.nc || echo '$(abs_srcdir)/testdata/mulval.nc' ; elif test $(ENABLE_GRIB) = yes -o $(ENABLE_CGRIBEX) = yes ; then test -f ../testdata/mulval.grb && echo ../testdata/mulval.grb || echo '$(abs_srcdir)/testdata/mulval.grb' ; else exit 1; fi`

if ENABLE_RUBY
BINDINGS += rubyObj rubyLib
BINDINGS_INSTALL += rubyObjInstall
BINDINGS_TESTS += rubyObjTest rubyLibTest
# Ruby ====================================================
rubyLibDir = rubyLibOut


rubyLibMakefile: $(rubyLibDir)/Makefile
$(rubyLibDir)/Makefile: ruby/extconfLib.rb Makefile
	$(MKDIR_P) $(rubyLibDir)
	@cd $(rubyLibDir) && CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" \
	  LDFLAGS="$(LDFLAGS)" \
	  INCFLAGS="-I$(abs_top_srcdir)/src" \
	  ruby -I"$(abs_srcdir)/ruby" \
	  `test -f ../ruby/extconfLib.rb && echo ../ruby || echo '$(abs_srcdir)/ruby/'`extconfLib.rb
	$(SED) -i -e '/^\.c\.o:/s/^\.c\.o/.c.lo/;/^.SUFFIXES:/s/$$/ .lo/' \
	  -e '/^install-so: \$$(RUBYARCHDIR)\/\$$(DLLIB)/{' \
	  -e 's/^install-so:.*$$/install-so: $$(DLLIB)/;p;n;d' \
	  -e '}' \
	  -e 's/\$$(\@D)/\$$(RUBYARCHDIR)/' \
	  $(rubyLibDir)/Makefile

rubyLibWrapper: $(rubyLibDir)/cdilib_wrap.c
$(rubyLibDir)/cdilib_wrap.c: cdilib.i ../src/cdi.h
if ENABLE_SWIG
	$(MKDIR_P) $(rubyLibDir)
	$(SWIG) -ruby -module CdiLib -o $(rubyLibDir)/cdilib_wrap.c \
	  $(AM_CPPFLAGS) $<
endif

rubyLib: rubyLibMakefile $(rubyLibDir)/CdiLib.la
$(rubyLibDir)/CdiLib.la: $(rubyLibDir)/cdilib_wrap.c
	$(MKDIR_P) $(rubyLibDir)
	@cd $(rubyLibDir) && $(MAKE) \
	  prefix="$(prefix)" libdir="$(libdir)" sitedir="$(libdir)/site_ruby" \
	  CC="$(abs_top_builddir)/libtool --mode=compile --tag=CC $(CC) -shared" \
	  OBJS=cdilib_wrap.lo \
	  LDSHARED="$(abs_top_builddir)/libtool --mode=link --tag=CC $(CC) -shared -module -rpath "'$$(sitearchdir)' \
	  DLLIB='$$(TARGET).la'



rubyLibTest: rubyLib
	$(MKDIR_P) $(rubyLibDir)
	@cd $(rubyLibDir) && ruby -I ./.libs \
	  `test -f ../ruby/testLib.rb && echo ../ruby || echo '$(abs_srcdir)/ruby'`/testLib.rb \
	  $(select_test_file)

rubyLibInstall: rubyLib
	$(MKDIR_P) $(rubyLibDir)
	@cd $(rubyLibDir) && $(MAKE) \
	  prefix="$(prefix)" libdir="$(libdir)" sitedir="$(libdir)/site_ruby" \
	  CC="$(abs_top_builddir)/libtool --mode=compile --tag=CC $(CC) -shared" \
	  OBJS=cdilib_wrap.lo \
	  LDSHARED="$(abs_top_builddir)/libtool --mode=link --tag=CC $(CC) -shared -module -rpath "'$$(sitearchdir)' \
	  INSTALL_PROG="$(SHELL) $(abs_top_builddir)/libtool --mode=install $(install_sh_PROGRAM)" \
	  DLLIB='$$(TARGET).la' install

# =========================================================
rubyObjDir = rubyObjOut

rubyObjMakefile: $(rubyObjDir)/Makefile
$(rubyObjDir)/Makefile: ruby/extconfObj.rb
	$(MKDIR_P) $(rubyObjDir)
	@cd $(rubyObjDir) && CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" \
	  LDFLAGS="$(LDFLAGS)" \
	  INCFLAGS="-I$(abs_srcdir)" \
	  ruby -I"$(abs_srcdir)/ruby" \
	  `test -f ../ruby/extconfObj.rb && echo ../ruby || echo '$(abs_srcdir)/ruby/'`extconfObj.rb
	$(SED) -i -e '/^\.cpp\.o:/s/^\.cpp\.o/.cpp.lo/;/^.SUFFIXES:/s/$$/ .lo/' \
	  -e '/^install-so: \$$(RUBYARCHDIR)\/\$$(DLLIB)/{' \
	  -e 's/^install-so:.*$$/install-so: $$(DLLIB)/;p;n;d' \
	  -e '}' \
	  -e 's/\$$(\@D)/\$$(RUBYARCHDIR)/' \
	  $(rubyObjDir)/Makefile

rubyObjWrapper: $(rubyObjDir)/cdiobj_wrap.cpp
$(rubyObjDir)/cdiobj_wrap.cpp: cdiobj.i cdi.hpp
if ENABLE_SWIG
	$(MKDIR_P) $(rubyObjDir)
	$(SWIG) -ruby -c++ -module CdiObj -o $(rubyObjDir)/cdiobj_wrap.cpp \
	  $(AM_CPPFLAGS) $<
endif

rubyObj: $(rubyObjDir)/CdiObj.la
$(rubyObjDir)/CdiObj.la: $(rubyObjDir)/cdiobj_wrap.cpp $(rubyObjDir)/Makefile
	$(MKDIR_P) $(rubyObjDir)
	@cd $(rubyObjDir) && $(MAKE) \
	  prefix="$(prefix)" libdir="$(libdir)" sitedir="$(libdir)/site_ruby" \
	  CXX="$(abs_top_builddir)/libtool --mode=compile --tag=CXX $(CXX) -shared" \
	  OBJS=cdiobj_wrap.lo \
	  LDSHAREDXX="$(abs_top_builddir)/libtool --mode=link --tag=CXX $(CXX) -shared -module -rpath "'$$(sitearchdir)' \
	  DLLIB='$$(TARGET).la'

rubyObjTest: rubyObj
	$(MKDIR_P) $(rubyObjDir)
	@cd $(rubyObjDir) && \
	  ruby -I ./.libs \
	  `test -f ../ruby/testObj.rb && echo ../ruby || echo '$(abs_srcdir)/ruby'`/testObj.rb \
	  $(select_test_file)

rubyObjInstall: rubyObj
	$(MKDIR_P) $(rubyObjDir)
	@cd $(rubyObjDir) && $(MAKE) \
	  prefix="$(prefix)" libdir="$(libdir)" sitedir="$(libdir)/site_ruby" \
	  CXX="$(abs_top_builddir)/libtool --mode=compile --tag=CXX $(CXX) -shared" \
	  OBJS=cdiobj_wrap.lo \
	  LDSHAREDXX="$(abs_top_builddir)/libtool --mode=link --tag=CXX $(CXX) -shared -module -rpath "'$$(sitearchdir)' \
	  INSTALL_PROG="$(SHELL) $(abs_top_builddir)/libtool --mode=install $(install_sh_PROGRAM)" \
	  DLLIB='$$(TARGET).la' install
endif
# =========================================================
if ENABLE_PYTHON
BINDINGS += pythonObj pythonLib
BINDINGS_INSTALL += pythonObjInstall pythonLibInstall
BINDINGS_TESTS += pythonObjTest pythonLibTest
# Python ==================================================

setup_python_vars=LIBTOOL="$(SHELL) $(abs_top_builddir)/libtool" \
	  AM_LIBTOOLFLAGS="$(AM_LIBTOOLFLAGS)" \
	  LIBTOOLFLAGS="$(LIBTOOLFLAGS) --mode=install" \
	  INSTALL="$(INSTALL) $(INSTALL_STRIP_FLAG)" \
	  CC="$${LIBTOOL} --mode=compile --tag=CC $(CC) -shared" \
	  CFLAGS="$(CFLAGS) $(DEFS)" \
	  LDSHARED="$${LIBTOOL} --mode=link --tag=CC $(CC) -module -shared" \
	  BUILDLIBDIR="$(top_builddir)/src" \
	  LIBDIR="$(libdir)" \
	  LIBS="$(LIBS)" \
	  LDFLAGS="-L../../src $(LDFLAGS)" \
	  INCFLAGS="-I.. -I$(abs_srcdir) -I$(abs_top_srcdir)/src" \
	  OPT= \
	  BASECFLAGS= \
	  abs_srcdir='$(abs_srcdir)/python' ; \
	  abs_builddir='$(abs_builddir)' \
	  abs_builddir_python='$(abs_builddir)/python' \
	  abs_builddir_python_build='$(abs_builddir)/python/'"$${py_build_libdir}/.libs" ; \
	  export LIBTOOL AM_LIBTOOLFLAGS LIBTOOLFLAGS INSTALL \
	  CC BUILDLIBDIR LIBDIR CFLAGS OPT LIBS CDIOBJ_WRAP LDFLAGS \
	  BASECFLAGS INCFLAGS LDSHARED CXX abs_srcdir abs_builddir \
	  abs_builddir_python abs_builddir_python_build ;


setup_cdilib_py_vars = \
  setupLib_py=`test -f setupLib.py || echo '$(abs_srcdir)/python/'`setupLib.py ; \
  py_libdir=`$(PYTHON) $${setupLib_py} install --print-libdir --prefix="$(prefix)"` ; \
  py_build_libdir=`$(PYTHON) $${setupLib_py} install --print-build-libdir --prefix="$(prefix)"` ; \
  LDFLAGS="$${LDFLAGS} -rpath $${py_libdir}" ; \
  abs_builddir_python_build="$(abs_builddir)/python/$${py_build_libdir}/.libs"

pythonLibWrapper: python/cdilib_wrap.c
python/cdilib_wrap.c: cdilib.i ../src/cdi.h
if ENABLE_SWIG
	$(MKDIR_P) python
	$(SWIG) -python -module CdiLib -o python/cdilib_wrap.c \
	  $(AM_CPPFLAGS) $<
endif

pythonLib: python/CdiLib.stamp
python/CdiLib.stamp: python/cdilib_wrap.c
	$(MKDIR_P) python
	@set -e ; cd python ; \
	  $(setup_python_vars) \
	  $(setup_cdilib_py_vars) \
	  $(PYTHON) "$${setupLib_py}" build_ext -v
	touch $@

pythonLibTest: pythonLib
	$(MKDIR_P) python
	@set -e ; cd python ; \
	  $(setup_python_vars) \
	  $(setup_cdilib_py_vars) \
	  $(PYTHON) \
	  `test -f testLib.py || echo '$(abs_srcdir)/python/'`testLib.py \
	  $(select_test_file)

pythonLibInstall:
	@set -e ; cd python; $(setup_python_vars) $(setup_cdilib_py_vars) \
	  $(PYTHON) "$${setupLib_py}" \
	  install --prefix="$(DESTDIR)$(prefix)"

# =========================================================
pythonObjWrapper: python/cdiobj_wrap.cpp
python/cdiobj_wrap.cpp: cdiobj.i cdi.hpp
if ENABLE_SWIG
	$(MKDIR_P) python
	$(SWIG) -python -c++ -module CdiObj -o python/cdiobj_wrap.cpp \
	  $(AM_CPPFLAGS) $<
endif

setup_python_cxx_vars = CFLAGS="$(CXXFLAGS) $(DEFS)" \
	  CC="$${LIBTOOL} --mode=compile --tag=CXX $(CXX) -shared" \
	  LDSHARED="$${LIBTOOL} --mode=link --tag=CXX $(CXX) -module -shared" \
	  CXX="$${LIBTOOL} --mode=link --tag=CXX $(CXX) -module -shared"

setup_cdiobj_py_vars = \
  setupObj_py=`test -f setupObj.py || echo '$(abs_srcdir)/python/'`setupObj.py ; \
  py_libdir=`$(PYTHON) $${setupObj_py} install --print-libdir --prefix="$(prefix)"` ; \
  py_build_libdir=`$(PYTHON) $${setupObj_py} install --print-build-libdir --prefix="$(prefix)"` ; \
  LDFLAGS="$${LDFLAGS} -shared -rpath $${py_libdir}" \
  abs_builddir_python_build='$(abs_builddir)/python/'"$${py_build_libdir}/.libs"


pythonObj: python/CdiObj.stamp
python/CdiObj.stamp: python/cdiobj_wrap.cpp cdi.lo
	$(MKDIR_P) python
	@set -e ; cd python; $(setup_python_vars) \
	  $(setup_python_cxx_vars) \
	  $(setup_cdiobj_py_vars) \
	  $(PYTHON) "$${setupObj_py}" \
	  build_ext -v
	touch $@

pythonObjTest: pythonObj
	$(MKDIR_P) python
	@set -e ; cd python ; \
	  $(setup_python_vars) \
	  $(setup_python_cxx_vars) \
	  $(setup_cdiobj_py_vars) \
	  $(PYTHON) `test -f testObj.py || echo '$(abs_srcdir)/python/'`testObj.py \
	  $(select_test_file)


pythonObjInstall:
	@set -e ; cd python; $(setup_python_vars) $(setup_cdiobj_py_vars) \
	  $(PYTHON) "$${setupObj_py}" \
	  install --prefix="$(DESTDIR)$(prefix)"
endif
# =========================================================
help:
	@echo "#=====================================================#"
	@echo "# EXPERIMENTAL FEATURE ===============================#"
	@echo "#=====================================================#"
	@echo "Bindings for: python ruby"
	@echo "(please exchange <lang> with the corresponding language)"
	@echo "#=====================================================#"
	@echo "  help             - this message"
	@echo "  <lang>Obj        - create object interface for Ruby"
	@echo "  <lang>Lib        - create function interface for Ruby"
	@echo "  <lang>ObjWrapper - create the wrapper files using swig (object interface)"
	@echo "  <lang>LibWrapper - create the wrapper files using swig (function interface)"
	@echo "  <lang>ObjTest    - run object interface test (./<lang>/testObj.rb). Accepts input file argument."
	@echo "  <lang>LibTest    - run function library test (./<lang>/testLib.rb). Accepts input file argument."
	@echo "  <lang>ObjInstall - install shared libraries and optional language files for <lang>"
	@echo "  <lang>LibInstall - see <lang>ObjInstall"
#
AM_CPPFLAGS = -I$(top_srcdir)/src
#
all-local: $(BINDINGS)
install-exec-local: $(BINDINGS_INSTALL)
check-local: $(BINDINGS_TESTS)
#
clean-local:
	rm -f *~ python/CdiLib.py python/CdiLib.pyc python/CdiLib.stamp \
		python/CdiObj.py python/CdiObj.pyc python/CdiObj.stamp \
		python/cdilib_wrap.c python/cdiobj_wrap.cpp
	rm -rf python/build
	-cd $(rubyObjDir) && \rm -f CdiObj.la Makefile cdiobj_wrap.cpp \
		cdiobj_wrap.lo cdiobj_wrap.o
	-cd $(rubyLibDir) && \rm -f CdiLib.la Makefile cdilib_wrap.c \
		cdilib_wrap.lo cdilib_wrap.o
	rm -rf python/.libs $(rubyLibDir)/.libs $(rubyObjDir)/.libs
	-rmdir python $(rubyLibDir) $(rubyObjDir)
