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
libcdipp_la_LDFLAGS = @NETCDF_LIBS@ @HDF5_LIBS@ @SZLIB_LIBS@ @ZLIB_LIBS@ $(LDFLAGS)

CdiInfo_SOURCES     = CdiInfo.cpp
CdiInfo_LDADD       = libcdipp.la
CdiInfo_LDFLAGS     = @NETCDF_LIBS@ @HDF5_LIBS@ @SZLIB_LIBS@ @ZLIB_LIBS@ $(LDFLAGS)
#
#
if ENABLE_RUBY
BINDINGS += rubyObj
BINDINGS_INSTALL += rubyObjInstall
BINDINGS_TESTS += rubyObjTest
# Ruby ====================================================
rubyLibMakefile:
	cd ruby; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(locallibs) $(LDFLAGS)" INCFLAGS="-I../../src" ruby extconfLib.rb

rubyLibWrapper: cdilib.i ../src/cdi.h
if ENABLE_SWIG
	$(SWIG) -ruby -module CdiLib -o ruby/cdilib_wrap.c $(AM_CPPFLAGS) cdilib.i
endif

rubyLib: rubyLibWrapper rubyLibMakefile
	@cd ruby; $(MAKE)

rubyLibTest: rubyLib
	@cd ruby; ruby testLib.rb
# =========================================================
rubyObjMakefile: ruby/Makefile
ruby/Makefile: ruby/extconfObj.rb
	cd ruby; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(LDFLAGS)" \
	  INCFLAGS="-I$(abs_srcdir)" \
	  ruby -I"$(abs_srcdir)/ruby" \
	  `test -f extconfObj.rb || echo '$(abs_srcdir)/ruby/'`extconfObj.rb
	$(SED) -i '/^\.cpp.o:/s/^\.cpp.o/.cpp.lo/;/^.SUFFIXES:/s/$$/ .lo/' ruby/Makefile

rubyObjWrapper: ruby/cdiobj_wrap.cpp
ruby/cdiobj_wrap.cpp: cdiobj.i cdi.hpp
if ENABLE_SWIG
	$(MKDIR_P) ruby
	$(SWIG) -ruby -c++ -module CdiObj -o ruby/cdiobj_wrap.cpp \
	  $(AM_CPPFLAGS) $<
endif

rubyObj: rubyObjWrapper rubyObjMakefile
	@cd ruby; $(MAKE) \
	  CXX="$(abs_top_builddir)/libtool --mode=compile --tag=CXX $(CXX)" \
	  OBJS=cdiobj_wrap.lo \
	  LDSHAREDXX="$(abs_top_builddir)/libtool --mode=link --tag=CXX $(CXX) -module" \
	  DLLIB='$$(TARGET).la'

rubyObjTest: rubyObj
	@cd ruby; ruby testObj.rb

rubyObjInstall: rubyObj
	@cd ruby; $(MAKE) install
endif
# =========================================================
if ENABLE_PYTHON
BINDINGS += pythonObj pythonLib
BINDINGS_INSTALL += pythonObjInstall pythonLibInstall
BINDINGS_TESTS += pythonObjTest pythonLibTest
# Python ==================================================
pythonLibWrapper: python/cdilib_wrap.c
python/cdilib_wrap.c: cdilib.i
if ENABLE_SWIG
	$(SWIG) -python -module CdiLib -o python/cdilib_wrap.c \
	  $(AM_CPPFLAGS) cdilib.i
endif

pythonLib: pythonLibWrapper
	@cd python; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(LDFLAGS)" INCFLAGS="-I../../src" $(PYTHON) setupLib.py build_ext --inplace

pythonLibTest: pythonLib
	@cd python; $(PYTHON) testLib.py
# =========================================================
pythonObjWrapper: python/cdiobj_wrap.cpp
python/cdiobj_wrap.cpp: cdiobj.i cdi.hpp
if ENABLE_SWIG
	$(MKDIR_P) python
	$(SWIG) -python -c++ -module CdiObj -o python/cdiobj_wrap.cpp \
	  $(AM_CPPFLAGS) $<
endif

setup_python_vars=LIBTOOL="$(SHELL) $(abs_top_builddir)/libtool" \
	  AM_LIBTOOLFLAGS="$(AM_LIBTOOLFLAGS)" \
	  LIBTOOLFLAGS="$(LIBTOOLFLAGS) --mode=install" \
	  INSTALL="$(INSTALL) $(INSTALL_STRIP_FLAG)" \
	  CC="$${LIBTOOL} --mode=compile --tag=CXX $(CXX)" \
	  BUILDLIBDIR="$(top_builddir)/src" \
	  LIBDIR="$(libdir)" \
	  CFLAGS="$(CXXFLAGS)" \
	  LIBS="$(LIBS)" \
	  CDIOBJ_WRAP=`test -f cdiobj_wrap.cpp || echo '../$(srcdir)/python/'`cdiobj_wrap.cpp \
	  LDFLAGS="-L../../src $(LDFLAGS)" \
	  INCFLAGS="-I.. -I$(abs_srcdir)" \
	  OPT= \
	  BASECFLAGS= \
	  LDSHARED="$${LIBTOOL} --mode=link --tag=CXX $(CXX) -module" \
	  CXX="$${LIBTOOL} --mode=link --tag=CXX $(CXX) -module" \
	  abs_srcdir='$(abs_srcdir)/python' ; \
	  export LIBTOOL AM_LIBTOOLFLAGS LIBTOOLFLAGS INSTALL \
	  CC BUILDLIBDIR LIBDIR CFLAGS OPT LIBS CDIOBJ_WRAP LDFLAGS \
	  BASECFLAGS INCFLAGS LDSHARED CXX abs_srcdir ; \
	  setupObj_py=`test -f setupObj.py || echo '$(abs_srcdir)/python/'`setupObj.py ; \
	  py_libdir=`$(PYTHON) $${setupObj_py} install --print-libdir --prefix="$(prefix)"` ; \
	  py_build_libdir=`$(PYTHON) $${setupObj_py} install --print-build-libdir --prefix="$(prefix)"` ; \
	  LDFLAGS="$${LDFLAGS} -rpath $${py_libdir}" ;

pythonObj: python/CdiObj.stamp
python/CdiObj.stamp: python/cdiobj_wrap.cpp
	$(MKDIR_P) python
	@set -e ; cd python; $(setup_python_vars) \
	  $(PYTHON) "$${setupObj_py}" \
	  build_ext -v
	touch python/CdiObj.stamp

pythonObjTest: pythonObj
	$(MKDIR_P) python
	set -e ; cd python ; \
	  $(setup_python_vars) \
	  abs_builddir="$(abs_builddir)/python" \
	  abs_builddir_python_build="$(abs_builddir)/python/$${py_build_libdir}/.libs" \
	  $(PYTHON) `test -f testObj.py || echo '$(abs_srcdir)/python/'`testObj.py

pythonObjInstall:
	@set -e ; cd python; $(setup_python_vars) \
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
CLEANFILES  = `ls *~`
