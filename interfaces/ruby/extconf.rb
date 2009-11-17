$INCFLAGS = "#{ENV['INCFLAGS']} #{$INCFLAGS}"
$CFLAGS   = "#{ENV['CFLAGS']} #{$CFLAGS}"
$LDFLAGS  = "#{ENV['LDFLAGS']} #{$LDFLAGS} ../../src/cdilib.o"
$LIBS     = "#{ENV['LIBS']} #{$LIBS}"
