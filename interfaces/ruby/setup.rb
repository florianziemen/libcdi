require 'mkmf'

$INCFLAGS = "#{ENV['INCFLAGS']} #{$INCFLAGS}"
$CFLAGS   = "#{ENV['CFLAGS']} #{$CFLAGS}"
$LDFLAGS  = "#{ENV['LDFLAGS']} #{$LDFLAGS}"
$LIBS     = "#{ENV['LIBS']} #{$LIBS}"

$libs = append_library($libs, "stdc++")
$LDFLAGS  += " ../.libs/libcdipp.a ../../src/.libs/libcdi.a"
$srcs = %w[cdi_wrapper.cpp]
$objs = %w[cdi_wrapper.o]
create_makefile('Cdi')
