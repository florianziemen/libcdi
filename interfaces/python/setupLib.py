#!/usr/bin/env python
from distutils.core import setup, Extension
from setup import *
from lt_distutils import lt_build_ext, lt_install_lib
import sys, os, distutils

CdiLib_module = Extension('_CdiLib',
                  sources=['cdilib_wrap.c'],
                  extra_compile_args = INCFLAGS,
                  library_dirs = list(map(
                      lambda x: x.lstrip('-L'),
                      filter(lambda x: x if x.startswith('-L') else None,
                             os.environ['LDFLAGS'].split()))),
                  libraries = ['cdi'],
                  extra_link_args = LIBS,
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

dist = setup (name = 'CdiLib',
              version = '0.1',
              author      = "Ralf Mueller",
              description = """pyhton bindings to CDI function library""",
              package_dir = {"" : os.environ['abs_srcdir']},
              ext_modules = [CdiLib_module],
              cmdclass={'build_ext': lt_build_ext, "install_lib" : lt_install_lib},
              py_modules = ["CdiLib"],
              )

if print_libdir or print_build_libdir:
    installer = dist.get_command_obj('install')
    installer.ensure_finalized()
    if print_libdir:
        print(installer.install_lib.rstrip('/'))
    if print_build_libdir:
        print(installer.build_lib)
