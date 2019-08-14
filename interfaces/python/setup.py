#!/usr/bin/env python
import os, string

INCFLAGS = []
CFLAGS   = []
LDFLAGS  = []
LIBS     = []
if 'INCFLAGS' in os.environ:
   INCFLAGS = os.environ['INCFLAGS'].split()

if 'CFLAGS' in os.environ:
   CFLAGS  = os.environ['CFLAGS'].split()

if 'LDFLAGS' in os.environ:
   LDFLAGS  = os.environ['LDFLAGS'].split('-L')

if 'LIBS' in os.environ:
   LIBS     = os.environ['LIBS'].split()


from distutils.core import setup, Extension
from setup import *

cdiobj_module = Extension('_Cdi',
                  sources=['cdi_wrapper.cpp'],
                  extra_compile_args = INCFLAGS,
                  library_dirs = LDFLAGS,
                  extra_objects = ['../cdi.o'],
                  runtime_library_dirs = [os.environ['BUILDLIBDIR'],os.environ['LIBDIR']],
                  extra_link_args = LIBS,
                  libraries    = ['cdi','stdc++'],
                  language = 'c++',
                  )

setup (name = 'Cdi',
       version = '0.1',
       author      = "Ralf Mueller",
       description = """pyhton bindings to CDI class library""",
       ext_modules = [cdiobj_module],
       py_modules = ["Cdi"],
       )
