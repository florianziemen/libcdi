require 'mkmf'
load "extconf.rb"
$libs = append_library($libs, "stdc++")
$LDFLAGS += " ../../src/libcdi.la"
$srcs = %w[cdilib_wrap.c]
$objs = %w[cdilib_wrap.o]
create_makefile('CdiLib')
