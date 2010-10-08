#! /bin/bash
#-----------------------------------------------------------------------------
. /sw/share/Modules/init/bash

module load GCC/4.3.3
module load NAG/5.2.721
module load OpenMPI

export AIO_LIB="-laio -lrt"
export IB_LIB="-lrdmacm -libverbs -lnuma -ldl -Wl,--export-dynamic -lnsl -lutil -lm -ldl"

export MPIROOT=/sw/sles10-x64/ofed/openmpi-1.4.2-nag52
export MPI_INCLUDE=-I${MPIROOT}/include
export MPI_LIB="-L${MPIROOT}/lib -WL,-pthread -lmpi_f90 -lmpi_f77 -lmpi -lopen-rte -lopen-pal"

export CC=gcc
export FC=nagfor
export F77=nagfor
export CPPFLAGS="-DNAGf90Fortran -DHAVE_LIBPTHREAD" 
export CFLAGS="${MPI_INCLUDE} -g -pedantic -Wall -Wextra" 
#export LIBS="${AIO_LIB}"
export LIBS="${AIO_LIB} -lpthread"

export LDFLAGS=-g
export VALGRINDROOT=/sw/sles10-x64/valgrind-3.5.0

# don't use --enable-mpi

cwd=$(pwd -P)

make distclean
#./configure
./configure --prefix=${MPIROOT} --enable-debug \
    --enable-memchecker --with-valgrind=${VALGRINDROOT}

cd src
gcc -o make_fint make_fint.c
./make_fint cdi.h
cd $cwd

makefiles=$(find . -name Makefile)

for file in $makefiles 
do
    sed -i "s:LIBS =:LIBS = ${MPI_LIB} ${IB_LIB}:" $file
done

make -j 4

if [ ! -e lib ]
then
    mkdir lib
fi
cp src/libcdi.a lib/libcdi_parallel.a

make distclean

export CPPFLAGS="-DNAGf90Fortran -DHAVE_LIBPTHREAD -DNOMPI" 
export CFLAGS=

./configure
cd src
gcc -o make_fint make_fint.c
./make_fint cdi.h
cd $cwd

make -j 4

cp src/libcdi.a lib/libcdi_serial.a

