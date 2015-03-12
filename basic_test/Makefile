CFLAGS  = -O0 -g -fstack-protector-all --std=c99 

all:
	gcc ${CFLAGS} -I../build/include -c main.c -o main.o
	gcc -L../build/lib64  main.o -lcdi -o main
fortran:
	gcc ${CFLAGS} -DHAVE_CF_INTERFACE -DHAVE_LIBNETCDF -DHAVE_LIBGRIB -DHAVE_LIBGRIB_API -I/opt/grib_api/include -c ../src/cdilib.c -o cdilib.o
	gfortran -I../build/include cdilib.o example.f90 -L/opt/grib_api/lib64 -lgrib_api -ljasper -lnetcdff -lnetcdf -o example 
run:
	LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../build/lib64  ./main
runfortran:
	LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../build/lib64:/opt/grib_api/lib64  ./example
clean:
	rm -f main main.o example cdilib.o
