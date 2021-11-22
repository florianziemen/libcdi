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
rubyObj: rubyObjWrapper rubyObjMakefile libcdipp.la
	@$(am__cd) ruby && $(MAKE) $(AM_MAKEFLAGS)

rubyLib: rubyLibWrapper rubyLibMakefile
	@$(am__cd) ruby && $(MAKE) $(AM_MAKEFLAGS)

rubyObjWrapper: ruby/cdiobj_wrap.cpp

rubyLibWrapper: ruby/cdilib_wrap.c

rubyObjTest: rubyObj
	@$(am__cd) ruby && $(RUBY) testObj.rb

rubyLibTest: rubyLib
	@$(am__cd) ruby && $(RUBY) testLib.rb

rubyObjInstall: rubyObjMakefile
	@$(am__cd) ruby && $(MAKE) $(AM_MAKEFLAGS) install

rubyLibInstall: rubyLibMakefile
	@$(am__cd) ruby && $(MAKE) $(AM_MAKEFLAGS) install

rubyObjMakefile:
	$(AM_V_GEN)$(am__cd) ruby && \
	INCFLAGS='-I$(abs_srcdir)' CFLAGS='$(CFLAGS)' LDFLAGS='$(LDFLAGS)' LIBS='$(LIBS)' $(RUBY) extconfObj.rb

rubyLibMakefile:
	$(AM_V_GEN)$(am__cd) ruby && \
	INCFLAGS='-I$(abs_top_srcdir)/src' CFLAGS='$(CFLAGS)' LDFLAGS='$(LDFLAGS) -L$(abs_top_builddir)/src/.libs' LIBS='-lcdi $(LIBS)' $(RUBY) extconfLib.rb

ruby/cdiobj_wrap.cpp: @MAINTAINER_MODE_TRUE@ cdiobj.i
	$(AM_V_GEN)$(SWIG) -ruby -c++ -module CdiObj -o $@ $(AM_CPPFLAGS) $<

ruby/cdilib_wrap.c: @MAINTAINER_MODE_TRUE@ cdilib.i
	$(AM_V_GEN)$(SWIG) -ruby -module CdiLib -o $@ $(AM_CPPFLAGS) $<

# PYTHON
pythonObj: pythonObjWrapper cdi.lo
	@$(am__cd) python && CC='$(CC)' CXX='$(CXX)' INCFLAGS='-I$(abs_srcdir)' CFLAGS='$(CFLAGS)' CXXFLAGS='$(CXXFLAGS)' LDFLAGS='$(LDFLAGS) -L$(abs_top_builddir)/src/.libs' LIBS='$(LIBS)' BUILDLIBDIR='$(abs_top_builddir)/src/.libs' LIBDIR='$(libdir)' $(PYTHON) setupObj.py build_ext --inplace

pythonLib: pythonLibWrapper $(top_builddir)/src/cdilib.lo
	@$(am__cd) python && CC='$(CC)' CXX='$(CXX)' INCFLAGS='-I$(abs_top_srcdir)/src' CFLAGS='$(CFLAGS)' CXXFLAGS='$(CXXFLAGS)' LDFLAGS='$(LDFLAGS)' LIBS='$(LIBS)' $(PYTHON) setupLib.py build_ext --inplace

pythonObjWrapper: python/cdiobj_wrap.cpp

pythonLibWrapper: python/cdilib_wrap.c

pythonObjTest: pythonObj
	@$(am__cd) python && $(PYTHON) testObj.py

pythonLibTest: pythonLib
	@$(am__cd) python && $(PYTHON) testLib.py

pythonObjInstall: pythonObj
	@$(am__cd) python && CC='$(CC)' CXX='$(CXX)' INCFLAGS='-I$(abs_srcdir)' CFLAGS='$(CFLAGS)' CXXFLAGS='$(CXXFLAGS)' LDFLAGS='$(LDFLAGS) -L$(abs_top_builddir)/src/.libs' LIBS='$(LIBS)' BUILDLIBDIR='$(abs_top_builddir)/src/.libs' LIBDIR='$(libdir)' $(PYTHON) setupObj.py install --prefix='$(prefix)'

pythonLibInstall: pythonLib
	@$(am__cd) python && CC='$(CC)' CXX='$(CXX)' INCFLAGS='-I$(abs_top_srcdir)/src' CFLAGS='$(CFLAGS)' CXXFLAGS='$(CXXFLAGS)' LDFLAGS='$(LDFLAGS)' LIBS='$(LIBS)' $(PYTHON) setupLib.py install --prefix='$(prefix)'

python/cdiobj_wrap.cpp: @MAINTAINER_MODE_TRUE@ cdiobj.i
	$(AM_V_GEN)$(SWIG) -python -c++ -module CdiObj -o $@ $(AM_CPPFLAGS) $<

python/cdilib_wrap.c: @MAINTAINER_MODE_TRUE@ cdilib.i
	$(AM_V_GEN)$(SWIG) -python -module CdiLib -o $@ $(AM_CPPFLAGS) $<

$(top_builddir)/src/cdilib.lo:
	@$(am__cd) $(top_builddir)/src && $(MAKE) $(AM_MAKEFLAGS) cdilib.lo

clean-local:
	-rm -f CdiInfo libcdipp.la
	-$(am__cd) ruby && rm -rf *.o *.so Makefile .sitearchdir.time
	-$(am__cd) python && rm -rf *.pyc *.pyo *.so __pycache__ build

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
