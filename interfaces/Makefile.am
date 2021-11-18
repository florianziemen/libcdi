AM_CPPFLAGS = -I$(top_srcdir)/src
AM_LDFLAGS = $(NETCDF_LIBS) $(SZLIB_LIBS) $(LDFLAGS)
locallibs = $(top_builddir)/src/.libs/libcdi.so $(LTLIBOBJS) $(NETCDF_LIBS) $(SZLIB_LIBS)

bin_PROGRAMS =
noinst_PROGRAMS = 
noinst_LTLIBRARIES = 
BINDINGS =

if CREATE_INTERFACES
noinst_PROGRAMS += CdiInfo
noinst_LTLIBRARIES += libcdipp.la
BINDINGS += help
endif

if ENABLE_RUBY
BINDINGS += rubyObj
endif

if ENABLE_PYTHON
BINDINGS += pythonObj
endif

SWIGCDILIB = cdilib.i
SWIGCDIOBJ = cdiobj.i

EXTRA_DIST = \
  $(SWIGCDILIB) \
  $(SWIGCDIOBJ) \
  CdiInfo.cpp \
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
  ruby/testLib.rb \
  ruby/testObj.rb

libcdipp_la_SOURCES = cdi.cpp cdi.hpp
libcdipp_la_LIBADD = $(top_builddir)/src/libcdi.la

CdiInfo_SOURCES = CdiInfo.cpp
CdiInfo_LDADD = libcdipp.la

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

if ENABLE_RUBY
rubyObj: rubyObjWrapper rubyObjMakefile
	@cd ruby; make

rubyLib: rubyLibWrapper rubyLibMakefile
	@cd ruby; make

rubyObjWrapper: cdiobj.i
if ENABLE_SWIG
	$(SWIG) -ruby -c++ -module CdiObj -o ruby/cdiobj_wrap.cpp $(AM_CPPFLAGS) cdiobj.i
endif

rubyLibWrapper: cdilib.i
if ENABLE_SWIG
	$(SWIG) -ruby -module CdiLib -o ruby/cdilib_wrap.c $(AM_CPPFLAGS) cdilib.i
endif

rubyObjTest: rubyObj
	@cd ruby; ruby testObj.rb

rubyLibTest: rubyLib
	@cd ruby; ruby testLib.rb

rubyObjInstall: rubyObj
	@cd ruby; make install

rubyObjMakefile:
	cd ruby; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(LDFLAGS)" INCFLAGS="-I../" ruby extconfObj.rb

rubyLibMakefile:
	cd ruby; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(locallibs) $(LDFLAGS)" INCFLAGS="-I../../src" ruby extconfLib.rb
endif

if ENABLE_PYTHON
pythonObj: python/cdiobj_wrap.cpp
	cd python; CC="$(CC)" CXX="$(CXX)" BUILDLIBDIR="$(top_builddir)/src/.libs" LIBDIR=$(libdir) CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="-L../../src/.libs $(LDFLAGS)" INCFLAGS="-I../" python setupObj.py build_ext --inplace

pythonLib: pythonLibWrapper
	@cd python; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(LDFLAGS)" INCFLAGS="-I../../src" python setupLib.py build_ext --inplace

pythonLibWrapper: cdilib.i
if ENABLE_SWIG
	$(SWIG) -python -module CdiLib -o python/cdilib_wrap.c $(AM_CPPFLAGS) cdilib.i
endif

pythonObjTest: pythonObj
	@cd python; python testObj.py

pythonLibTest: pythonLib
	@cd python; python testLib.py

pythonObjInstall: pythonObj
	cd python; CC="$(CC)" CXX="$(CXX)" BUILDLIBDIR="$(top_builddir)/src/.libs" LIBDIR=$(libdir) CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="-L../../src/.libs $(LDFLAGS)" INCFLAGS="-I../" python setupObj.py install --prefix=$(prefix)

python/cdiobj_wrap.cpp: cdiobj.i
if ENABLE_SWIG
	$(SWIG) -python -c++ -module CdiObj -o python/cdiobj_wrap.cpp $(AM_CPPFLAGS) cdiobj.i
endif
endif

CLEANFILES  = `ls *~`

# overrides to make regular compilations also use libtool
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
