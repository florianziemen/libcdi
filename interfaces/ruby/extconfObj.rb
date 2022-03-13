require 'mkmf'
load "extconf.rb"
$libs = append_library($libs, "stdc++")
$LDFLAGS += " ../libcdipp.la ../../src/libcdi.la"
$srcs = %w[cdiobj_wrap.cpp]
$objs = %w[cdiobj_wrap.o]
create_makefile('CdiObj')
