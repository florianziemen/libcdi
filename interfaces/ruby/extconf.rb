$INCFLAGS = "#{ENV['INCFLAGS']} #{$INCFLAGS}"
$CFLAGS   = "#{ENV['CFLAGS']} #{$CFLAGS}"
$LDFLAGS  = "#{ENV['LDFLAGS']} #{$LDFLAGS} -Wl,-rpath,../../src/.libs -L../../src/.libs -lcdi"
$LIBS     = "#{ENV['LIBS']} #{$LIBS}"
