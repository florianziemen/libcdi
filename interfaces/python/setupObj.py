#!/usr/bin/env python
from distutils.core import setup, Extension
from setup import *

cdiobj_module = Extension('_CdiObj',
                  sources=['cdiobj_wrap.cpp'],
                  extra_compile_args = INCFLAGS,
                  library_dirs = LDFLAGS,
                  extra_objects = ['../cdi.o'],
                  runtime_library_dirs = [],
                  extra_link_args = LIBS,
                  libraries    = ['cdi','stdc++'],
                  language = 'c++',
                  )

setup (name = 'CdiObj',
       version = '0.1',
       author      = "Ralf Mueller",
       description = """pyhton bindings to CDI class library""",
       ext_modules = [cdiobj_module],
       py_modules = ["CdiObj"],
       )
