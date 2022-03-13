#!/usr/bin/env python
from __future__ import print_function
import distutils.core
from distutils.core import setup, Extension

import sys
import os
from lt_distutils import lt_build_ext, lt_install_lib

from setup import *

cdiobj_module = Extension('_CdiObj',
                  sources=['cdiobj_wrap.cpp'],
                  extra_compile_args = INCFLAGS,
                  library_dirs = map(lambda x: x.lstrip('-L'),
                                     filter(lambda x: x if x.startswith('-L')
                                            else None,
                                            os.environ['LDFLAGS'].split())),
                  extra_objects = ['../cdi.lo'],
                  runtime_library_dirs = [os.environ['BUILDLIBDIR'],os.environ['LIBDIR']],
                  extra_link_args = LIBS,
                  libraries    = ['cdi','stdc++'],
                  language = 'c++',
                  )

print_libdir = False
print_build_libdir = False
if '--print-libdir' in sys.argv:
    sys.argv.remove('--print-libdir')
    distutils.core._setup_stop_after = "commandline"
    print_libdir = True

if '--print-build-libdir' in sys.argv:
    sys.argv.remove('--print-build-libdir')
    distutils.core._setup_stop_after = "commandline"
    print_build_libdir = True

dist = setup (name = 'CdiObj',
              version = '0.1',
              author      = "Ralf Mueller",
              description = """pyhton bindings to CDI class library""",
              package_dir = {"" : os.environ['abs_srcdir']},
              ext_modules = [cdiobj_module],
              cmdclass={'build_ext': lt_build_ext, "install_lib" : lt_install_lib},
              py_modules = ["CdiObj"],
              )

if print_libdir or print_build_libdir:
    installer = dist.get_command_obj('install')
    installer.ensure_finalized()
    if print_libdir:
        print(installer.install_lib.rstrip('/'))
    if print_build_libdir:
        print(installer.build_lib)
