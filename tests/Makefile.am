## Process this file with automake to produce Makefile.in
#
TESTS = test_grib.sh test_cksum_grib test_cksum_nc test_cksum_extra \
	test_cksum_service test_cksum_nc2 test_cksum_nc4 test_cksum_ieg \
	pio_write_run pio_cksum_mpinonb pio_cksum_fpguard \
	pio_cksum_asynch pio_cksum_writer
check_PROGRAMS = test_grib cksum_write cksum_read pio_write
#
test_grib_SOURCES = test_grib.c
cksum_write_SOURCES = cksum_write.c cksum.c cksum.h
cksum_read_SOURCES = cksum_read.c \
	cksum.c cksum.h \
	ensure_array_size.h ensure_array_size.c
pio_write_SOURCES = pio_write.c cksum.h cksum.c
#
LDADD = -L../src -lcdi -lm
AM_LDFLAGS = -L$(top_srcdir)/src
INCLUDES   = -I$(top_srcdir)/src
#
EXTRA_DIST = $(TESTS)
#
CLEANFILES  = `ls *~ *.grb *.nc *.srv *.ext`
