## Process this file with automake to produce Makefile.in
#
SWIGCDILIB = cdilib.i
SWIGCDIOBJ = cdiobj.i
BINDINGS   =
EXTRA_DIST = $(SWIGCDIOBJ) $(SWIGCDILIB)
EXTRA_DIST += ruby/cdilib_wrap.c ruby/cdiobj_wrap.cpp ruby/testLib.rb ruby/testObj.rb
EXTRA_DIST += python/cdilib_wrap.c python/cdiobj_wrap.cpp python/testLib.py python/testObj.py
if CREATE_INTERFACES
BINDINGS += help
endif
#
bin_PROGRAMS =
#
noinst_PROGRAMS = 
if CREATE_INTERFACES
noinst_PROGRAMS += CdiInfo
endif
#
CdiInfo_SOURCES = CdiInfo.cpp cdi.cpp cdi.hpp
#
locallibs     = $(top_builddir)/src/libcdi.la @LTLIBOBJS@ @NETCDF_LDFLAGS@ @HDF5_LDFLAGS@ @SZLIB_LDFLAGS@ @ZLIB_LDFLAGS@
CdiInfo_LDADD = $(top_builddir)/src/libcdi.la @LTLIBOBJS@ @NETCDF_LDFLAGS@ @HDF5_LDFLAGS@ @SZLIB_LDFLAGS@ @ZLIB_LDFLAGS@ $(LDFLAGS) $(LIBS) 
#
if WITH_RUBY
BINDINGS += rubyObj
# Ruby ====================================================
rubyLibMakefile:
	cd ruby; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(locallibs) $(LDFLAGS)" INCFLAGS="-I../../src" ruby extconfLib.rb

rubyLibWrapper: cdilib.i
if WITH_SWIG
	$(SWIG) -ruby -module CdiLib -o ruby/cdilib_wrap.c $(AM_CPPFLAGS) cdilib.i
endif

rubyLib: rubyLibWrapper rubyLibMakefile
	@cd ruby; make

rubyLibTest: rubyLib
	@cd ruby; ruby testLib.rb
# =========================================================
rubyObjMakefile:
	cd ruby; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(LDFLAGS)" INCFLAGS="-I../" ruby extconfObj.rb

rubyObjWrapper: cdiobj.i
if WITH_SWIG
	$(SWIG) -ruby -c++ -module CdiObj -o ruby/cdiobj_wrap.cpp $(AM_CPPFLAGS) cdiobj.i
endif

rubyObj: rubyObjWrapper rubyObjMakefile
	@cd ruby; make

rubyObjTest: rubyObj
	@cd ruby; ruby testObj.rb

rubyObjInstall: rubyObj
	@cd ruby; make install
endif
# =========================================================
if WITH_PYTHON
BINDINGS += pythonObj
# Python ==================================================
pythonLibWrapper: cdilib.i
if WITH_SWIG
	$(SWIG) -python -module CdiLib -o python/cdilib_wrap.c $(AM_CPPFLAGS) cdilib.i
endif

pythonLib: pythonLibWrapper
	@cd python; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(LDFLAGS)" INCFLAGS="-I../../src" python setupLib.py build_ext --inplace

pythonLibTest: pythonLib
	@cd python; python testLib.py
# =========================================================
pythonObjWrapper: cdiobj.i
if WITH_SWIG
	$(SWIG) -python -c++ -module CdiObj -o python/cdiobj_wrap.cpp $(AM_CPPFLAGS) cdiobj.i
endif

pythonObj: pythonObjWrapper
	cd python; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(locallibs) $(LDFLAGS)" INCFLAGS="-I../" python setupObj.py build_ext --inplace

pythonObjTest: pythonObj
	@cd python; python testObj.py

pythonObjInstall: pythonObj
	@cd python; CFLAGS="$(CFLAGS)" LIBS="$(LIBS)" LDFLAGS="$(locallibs) $(LDFLAGS)" INCFLAGS="-I../" python setupObj.py install
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
#
CLEANFILES  = `ls *~`
