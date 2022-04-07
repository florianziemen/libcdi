AM_CPPFLAGS = -I$(top_srcdir)/src

.PHONY: help rubyObj rubyLib pythonObj pythonLib \
  rubyObjWrapper rubyLibWrapper pythonObjWrapper pythonLibWrapper \
  rubyObjTest rubyLibTest pythonObjTest pythonLibTest \
  rubyObjInstall rubyLibInstall pythonObjInstall pythonLibInstall \
  rubyObjMakefile rubyLibMakefile
.NOTPARALLEL:

EXTRA_LTLIBRARIES = libcdipp.la
EXTRA_PROGRAMS = CdiInfo
EXTRA_DIST = \
  cdilib.i \
  cdiobj.i \
  f2003/bindGen.rb \
  python/CdiLib.py \
  python/CdiObj.py \
  python/cdilib_wrap.c \
  python/cdiobj_wrap.cpp \
  python/setup.py \
  python/setupLib.py \
  python/setupObj.py \
  python/table.py \
  python/testLib.py \
  python/testObj.py \
  ruby/cdilib_wrap.c \
  ruby/cdiobj_wrap.cpp \
  ruby/extconf.rb \
  ruby/extconfLib.rb \
  ruby/extconfObj.rb \
  ruby/testLib.rb \
  ruby/testObj.rb

libcdipp_la_SOURCES = cdi.cpp cdi.hpp
libcdipp_la_LIBADD = $(top_builddir)/src/libcdi.la

CdiInfo_SOURCES = CdiInfo.cpp
CdiInfo_LDADD = libcdipp.la

#
#
select_test_file = \
	`if test $(ENABLE_NETCDF) = yes ; then test -f ../testdata/mulval.nc && echo ../testdata/mulval.nc || echo '$(abs_srcdir)/testdata/mulval.nc' ; elif test $(ENABLE_GRIB) = yes -o $(ENABLE_CGRIBEX) = yes ; then test -f ../testdata/mulval.grb && echo ../testdata/mulval.grb || echo '$(abs_srcdir)/testdata/mulval.grb' ; else exit 1; fi`

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

# RUBY
rubyLibDir = rubyLibOut

rubyObj: $(rubyObjDir)/CdiObj.la
$(rubyObjDir)/CdiObj.la: rubyObjWrapper rubyObjMakefile libcdipp.la
	$(AM_V_GEN)$(MKDIR_P) $(rubyObjDir) && $(am__cd) $(rubyObjDir) && \
	  $(MAKE) $(AM_MAKEFLAGS) \
	  prefix="$(prefix)" libdir="$(libdir)" sitedir="$(libdir)/site_ruby" \
	  CXX="$(abs_top_builddir)/libtool --mode=compile --tag=CXX $(CXX) -shared" \
	  OBJS=cdiobj_wrap.lo \
	  LDSHAREDXX="$(abs_top_builddir)/libtool --mode=link --tag=CXX $(CXX) -shared -module -rpath "'$$(sitearchdir)' \
	  DLLIB='$$(TARGET).la'

rubyLib: rubyLibMakefile $(rubyLibDir)/CdiLib.la
$(rubyLibDir)/CdiLib.la: rubyLibWrapper rubyLibMakefile
	$(AM_V_GEN)$(MKDIR_P) $(rubyLibDir) && $(am__cd) $(rubyLibDir) && \
	  $(MAKE) $(AM_MAKEFLAGS) \
	  prefix="$(prefix)" libdir="$(libdir)" sitedir="$(libdir)/site_ruby" \
	  CC="$(abs_top_builddir)/libtool --mode=compile --tag=CC $(CC) -shared" \
	  OBJS=cdilib_wrap.lo \
	  LDSHARED="$(abs_top_builddir)/libtool --mode=link --tag=CC $(CC) -shared -module -rpath "'$$(sitearchdir)' \
	  DLLIB='$$(TARGET).la'

rubyObjWrapper: ruby/cdiobj_wrap.cpp

rubyLibWrapper: ruby/cdilib_wrap.c

rubyObjTest: rubyObj
	$(AM_V_GEN)$(MKDIR_P) $(rubyObjDir) && $(am__cd) $(rubyObjDir) && $(RUBY) -I ./.libs \
	  `test -f ../ruby/testObj.rb && echo ../ruby || echo '$(abs_srcdir)/ruby'`/testObj.rb \
	  $(select_test_file)

rubyLibTest: rubyLib
	$(AM_V_GEN)$(MKDIR_P) $(rubyLibDir) && $(am__cd) $(rubyLibDir) && \
	  $(RUBY) -I ./.libs \
	  `test -f ../ruby/testLib.rb && echo .. || echo '$(abs_srcdir)'`/ruby/testLib.rb \
	  $(select_test_file)

rubyObjInstall: rubyObj
	$(AM_V_GEN)$(MKDIR_P) $(rubyObjDir) && $(am__cd) $(rubyObjDir) && $(MAKE) $(AM_MAKEFLAGS) install

rubyLibInstall: rubyLib
	$(AM_V_GEN)$(MKDIR_P) $(rubyLibDir) && $(am__cd) $(rubyLibDir) && \
	  $(MAKE) $(AM_MAKEFLAGS) \
	  prefix="$(prefix)" libdir="$(libdir)" sitedir="$(libdir)/site_ruby" \
	  CXX="$(abs_top_builddir)/libtool --mode=compile --tag=CXX $(CXX) -shared" \
	  OBJS=cdiobj_wrap.lo \
	  LDSHAREDXX="$(abs_top_builddir)/libtool --mode=link --tag=CXX $(CXX) -shared -module -rpath "'$$(sitearchdir)' \
	  INSTALL_PROG="$(SHELL) $(abs_top_builddir)/libtool --mode=install $(install_sh_PROGRAM)" \
	  DLLIB='$$(TARGET).la' install

rubyObjDir = rubyObjOut

rubyObjMakefile: $(rubyObjDir)/Makefile
$(rubyObjDir)/Makefile: ruby/extconfObj.rb
	$(AM_V_GEN)$(MKDIR_P) $(rubyObjDir) && $(am__cd) $(rubyObjDir) && \
	  CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" \
	  LDFLAGS="$(LDFLAGS)" \
	  INCFLAGS="-I$(abs_srcdir) -I$(abs_top_srcdir)/src" \
	  $(RUBY) -I"$(abs_srcdir)/ruby" \
	  `test -f ../ruby/extconfObj.rb && echo .. || echo '$(abs_srcdir)'`/ruby/extconfObj.rb && \
	  $(SED) -i -e '/^\.cpp\.o:/s/^\.cpp\.o/.cpp.lo/;/^.SUFFIXES:/s/$$/ .lo/' \
	  -e '/^install-so: \$$(RUBYARCHDIR)\/\$$(DLLIB)/{' \
	  -e 's/^install-so:.*$$/install-so: $$(DLLIB)/;p;n;d' \
	  -e '}' \
	  -e 's/\$$(\@D)/\$$(RUBYARCHDIR)/' \
	  -e '/^OBJS =/s/\.o/.lo/' \
	  -e '/^CLEANOBJS =/s/\*\.o/*.o *.lo .libs/' \
	  Makefile

rubyLibMakefile: $(rubyLibDir)/Makefile
$(rubyLibDir)/Makefile: ruby/extconfLib.rb
	$(AM_V_GEN)$(MKDIR_P) $(rubyLibDir) && $(am__cd) $(rubyLibDir) && \
	  CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" \
	  LDFLAGS="$(LDFLAGS)" \
	  INCFLAGS="-I$(abs_top_srcdir)/src" \
	  $(RUBY) -I"$(abs_srcdir)/ruby" \
	  `test -f ../ruby/extconfLib.rb && echo .. || echo '$(abs_srcdir)'`/ruby/extconfLib.rb && \
	  $(SED) -i -e '/^\.c\.o:/s/^\.c\.o/.c.lo/;/^.SUFFIXES:/s/$$/ .lo/' \
	  -e '/^install-so: \$$(RUBYARCHDIR)\/\$$(DLLIB)/{' \
	  -e 's/^install-so:.*$$/install-so: $$(DLLIB)/;p;n;d' \
	  -e '}' \
	  -e 's/\$$(\@D)/\$$(RUBYARCHDIR)/' \
	  -e '/^OBJS =/s/\.o/.lo/' \
	  -e '/^CLEANOBJS =/s/\*\.o/*.o *.lo .libs/' \
	  Makefile

if MAINTAINER_MODE
ruby/cdiobj_wrap.cpp: cdiobj.i cdi.hpp
	$(AM_V_GEN)$(SWIG) -ruby -c++ -module CdiObj \
	  -o "$(abs_srcdir)/$@" $(AM_CPPFLAGS) \
	  `test -f ./$< && echo . || echo '$(abs_srcdir)'`/$<

ruby/cdilib_wrap.c: cdilib.i
	$(AM_V_GEN)$(SWIG) -ruby -module CdiLib \
	  -o "$(abs_srcdir)/$@" $(AM_CPPFLAGS) \
	  `test -f ./$< && echo . || echo '$(abs_srcdir)'`/$<
endif

# PYTHON
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
	  PYTHONDONTWRITEBYTECODE=1 \
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

setup_python_cxx_vars = CFLAGS="$(CXXFLAGS) $(DEFS)" \
	  CC="$${LIBTOOL} --mode=compile --tag=CXX $(CXX) -shared" \
	  LDSHARED="$${LIBTOOL} --mode=link --tag=CXX $(CXX) -module -shared" \
	  CXX="$${LIBTOOL} --mode=compile --tag=CXX $(CXX) -shared"

setup_cdiobj_py_vars = \
  setupObj_py=`test -f setupObj.py || echo '$(abs_srcdir)/python/'`setupObj.py ; \
  py_libdir=`$(PYTHON) $${setupObj_py} install --print-libdir --prefix="$(prefix)"` ; \
  py_build_libdir=`$(PYTHON) $${setupObj_py} install --print-build-libdir --prefix="$(prefix)"` ; \
  LDFLAGS="$${LDFLAGS} -shared -rpath $${py_libdir}" \
  abs_builddir_python_build='$(abs_builddir)/python/'"$${py_build_libdir}/.libs"

pythonObj: python/CdiObj.stamp
python/CdiObj.stamp: python/cdiobj_wrap.cpp cdi.lo pythonObjWrapper
	$(AM_V_GEN)$(MKDIR_P) python && $(am__cd) python && \
	  $(setup_python_vars) \
	  $(setup_python_cxx_vars) \
	  $(setup_cdiobj_py_vars) \
	  $(PYTHON) "$${setupObj_py}" \
	  build_ext -v && touch "../$@"

pythonLib: python/CdiLib.stamp
python/CdiLib.stamp: pythonLibWrapper python/cdilib_wrap.c
	$(AM_V_GEN)$(MKDIR_P) python && $(am__cd) python && \
	  $(setup_python_vars) \
	  $(setup_cdilib_py_vars) \
	  $(PYTHON) "$${setupLib_py}" build_ext -v && \
	  touch "../$@"

pythonObjWrapper: python/cdiobj_wrap.cpp

pythonLibWrapper: python/cdilib_wrap.c

pythonObjTest: pythonObj
	$(AM_V_GEN)$(MKDIR_P) python && $(am__cd) python && \
	  $(setup_python_vars) \
	  $(setup_python_cxx_vars) \
	  $(setup_cdiobj_py_vars) \
	  $(PYTHON) `test -f testObj.py || echo '$(abs_srcdir)/python/'`testObj.py \
	  $(select_test_file)

pythonLibTest: pythonLib
	$(AM_V_GEN)$(MKDIR_P) python && $(am__cd) python && \
	  $(setup_python_vars) \
	  $(setup_cdilib_py_vars) \
	  $(PYTHON) \
	  `test -f testLib.py || echo '$(abs_srcdir)/python/'`testLib.py \
	  $(select_test_file)

pythonObjInstall: pythonObj
	$(AM_V_GEN)$(MKDIR_P) python && $(am__cd) python && \
	  $(setup_python_vars) $(setup_cdiobj_py_vars) \
	  $(PYTHON) "$${setupObj_py}" \
	  install --prefix="$(DESTDIR)$(prefix)"

pythonLibInstall: pythonLib
	$(AM_V_GEN)$(MKDIR_P) python && $(am__cd) python && \
	  $(setup_python_vars) $(setup_cdilib_py_vars) \
	  $(PYTHON) "$${setupLib_py}" \
	  install --prefix="$(DESTDIR)$(prefix)"

if MAINTAINER_MODE
python/cdiobj_wrap.cpp: cdiobj.i
	$(AM_V_GEN)$(MKDIR_P) python && \
	  $(SWIG) -python -c++ -module CdiObj -o $@ $(AM_CPPFLAGS) \
	  `test -f ./$< && echo . || echo '$(abs_srcdir)'`/$<


python/cdilib_wrap.c: cdilib.i
	$(AM_V_GEN)$(MKDIR_P) python && \
	  $(SWIG) -python -module CdiLib -o $@ $(AM_CPPFLAGS) \
	  `test -f ./$< && echo . || echo '$(abs_srcdir)'`/$<

endif

clean-local:
	-rm -f python/CdiLib.stamp python/CdiObj.stamp
	-rm -f CdiInfo libcdipp.la
	-$(am__cd) python && rm -rf *.pyc *.pyo *.so __pycache__ build
	-$(am__cd) $(rubyObjDir) && \rm -rf CdiObj.la Makefile cdiobj_wrap.cpp \
		cdiobj_wrap.lo cdiobj_wrap.o *.o .libs Makefile .sitearchdir.time
	-$(am__cd) $(rubyLibDir) && \rm -rf CdiLib.la Makefile cdilib_wrap.c \
		cdilib_wrap.lo cdilib_wrap.o *.o .libs Makefile .sitearchdir.time
	-rmdir $(rubyLibDir) $(rubyObjDir)

# Compile programs using Libtool:
COMPILE = $(LTCOMPILE)
FCCOMPILE = $(LTFCCOMPILE)
PPFCCOMPILE = $(LTPPFCCOMPILE)
CXXCOMPILE = $(LTCXXCOMPILE)
