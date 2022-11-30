# Initialize utility functions:
. "$(cd "$(dirname "${BASH_SOURCE}")"; pwd)/../common_utils.sh"

#
# Initializes the environment.
#
init_env ()
{
  case $(lsb_release -sc) in
    stretch)
      module unload automake/1.16.1
      switch_for_module automake/1.16.1

      # Use non-Debian libtool by default:
      LIBTOOL_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/libtool-2.4.6-qdyaqqw'
      export PATH="${LIBTOOL_ROOT}/bin:${PATH-}"
      export ACLOCAL_PATH="${LIBTOOL_ROOT}/share/aclocal:${ACLOCAL_PATH-}"

      SWIG_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/swig-4.0.2-busrrtn'
      RUBY_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/ruby-3.0.2-stqm2vj'
      PYTHON_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/python-2.7.18-zqrnwom'

      export PATH="${RUBY_ROOT}/bin:${PYTHON_ROOT}/bin:${SWIG_ROOT}/bin:${PATH}"
    ;;
    bullseye)
      switch_for_module automake autoconf libtool swig ruby python
    ;;
  esac
}

#
# Initializes the Debian libtool
init_debian_libtool ()
{
  export PATH="/usr/bin:${PATH-}"
  export ACLOCAL_PATH="/usr/share/aclocal:${ACLOCAL_PATH-}"

  case $(lsb_release -sc) in
    stretch)
      automake_module='automake/1.16.1'
    ;;
    bullseye)
      automake_module='automake'
    ;;
  esac

  # Prepend ACLOCAL_PATH with the path to Automake:
  module unload "${automake_module}"
  switch_for_module "${automake_module}"
}

#
# Sets variables for tests with GCC.
#
init_gcc ()
{
  init_env

  case $(lsb_release -sc) in
    stretch)
      switch_for_module gcc/6.3.0

      ECCODES_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/eccodes-2.21.0-3sdngaq'
      NETCDF_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/netcdf-c-4.8.0-fzupaca'

      # The installation of ecCodes does not provide '*.la' files, which would
      # trigger the RPATH injection:
      export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${LD_LIBRARY_PATH-}"
      # The installation of NetCDF library provides '*.la' files, which trigger
      # the linking to the HDF5 library but its installation does not provide
      # the '*.la' files, which would trigger the RPATH injection:
      HDF5_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/hdf5-1.10.7-4l3frcp'
      export LD_LIBRARY_PATH="${HDF5_ROOT}/lib:${LD_LIBRARY_PATH}"
    ;;
    bullseye)
      switch_for_module gcc/12.1.0 mpich/3.4.3-gcc-12.1.0

      ECCODES_ROOT='/sw/bullseye-x64/packages/gcc-12.1.0/eccodes-2.26.0'
      NETCDF_ROOT='/sw/bullseye-x64/packages/gcc-12.1.0/netcdf-c-4.9.0'
      PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw-bullseye/ppm-1.0.8.1-gcc-12.1.0'
      YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw-bullseye/yaxt-0.9.3.1-gcc-12.1.0'

      # The installations of NetCDF and ecCodes libraries do not provide '*.la'
      # files, which would trigger the RPATH injection:
      export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${NETCDF_ROOT}/lib:${LD_LIBRARY_PATH-}"

      MPICC=mpicc
      MPIFC=mpif90
      MPI_LAUNCH="$(which mpirun)"
    ;;
  esac

  CC=gcc
  CXX=g++
  FC=gfortran
}

#
# Sets variables for tests with NVHPC.
#
init_nvhpc ()
{
  init_env

  case $(lsb_release -sc) in
    stretch)
      switch_for_module gcc/6.3.0

      export NVHPC='/data/mpi/sclab/sip/m300488/nvhpc'
      export NVLOCALRC="${NVHPC}/Linux_x86_64/21.3/compilers/bin/localrc.60300"
      export PATH="${NVHPC}/Linux_x86_64/21.3/compilers/bin:${PATH-}"
      export PATH="/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/mpich-3.4.2-sfxulsj/bin:${PATH}"

      ECCODES_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/eccodes-2.21.0-3sdngaq'
      NETCDF_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/netcdf-c-4.8.0-vjppuov'
      PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/scales-ppm-1.0.7-uy4z72r'
      YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/yaxt-0.9.2.1-lbqqemx'

      # The installation of ecCodes does not provide '*.la' files, which would
      # trigger the RPATH injection:
      export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${LD_LIBRARY_PATH-}"
      # The installation of NetCDF library provides '*.la' files, which trigger
      # the linking to the HDF5 library but its installation does not provide
      # the '*.la' files, which would trigger the RPATH injection:
      HDF5_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/hdf5-1.10.7-h7pvthl'
      export LD_LIBRARY_PATH="${HDF5_ROOT}/lib:${LD_LIBRARY_PATH}"
    ;;
    bullseye)
      switch_for_module nvhpc/22.3 mpich/3.4.3-nvhpc-22.3

      ECCODES_ROOT='/sw/bullseye-x64/packages/nvhpc-22.3/eccodes-2.26.0'
      NETCDF_ROOT='/sw/bullseye-x64/packages/nvhpc-22.3/netcdf-c-4.9.0'
      PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw-bullseye/ppm-1.0.8.1-nvhpc-22.3'
      YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw-bullseye/yaxt-0.9.3.1-nvhpc-22.3'

      # The installations of NetCDF and ecCodes libraries do not provide '*.la'
      # files, which would trigger the RPATH injection:
      export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${NETCDF_ROOT}/lib:${LD_LIBRARY_PATH-}"
    ;;
  esac

  CC=nvc
  CXX=nvc++
  FC=nvfortran

  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"
}

#
# Sets variables for tests with Clang.
#
init_clang ()
{
  # Make the system gfortran the first gfortran in the PATH:
  export PATH="/usr/bin:${PATH-}"

  init_env

  case $(lsb_release -sc) in
    stretch)
      switch_for_module gcc/6.3.0

      export PATH="/data/mpi/sclab/sip/m300488/clang/12.0.1/bin:${PATH}"
      # Clang does not inject RPATHs to the standard library in use:
      export LD_LIBRARY_PATH="/sw/stretch-x64/gcc/gcc-6.3.0/lib64:${LD_LIBRARY_PATH-}"

      export PATH="/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/openmpi-4.1.1-6ydagyy/bin:${PATH}"

      ECCODES_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/eccodes-2.21.0-3sdngaq'
      NETCDF_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/netcdf-c-4.8.0-z6r5guk'
      PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/scales-ppm-1.0.7-5mhatgx'
      YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/yaxt-0.9.2.1-cj2nf5j'

      # The installation of ecCodes does not provide '*.la' files, which would
      # trigger the RPATH injection:
      export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${LD_LIBRARY_PATH-}"
      # The installation of NetCDF library provides '*.la' files, which trigger
      # the linking to the HDF5 library but its installation does not provide
      # the '*.la' files, which would trigger the RPATH injection:
      HDF5_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/hdf5-1.10.7-4y54xnh'
      export LD_LIBRARY_PATH="${HDF5_ROOT}/lib:${LD_LIBRARY_PATH}"
    ;;
    bullseye)
      switch_for_module clang/14.0.6 openmpi/4.1.3-clang-14.0.6

      ECCODES_ROOT='/sw/bullseye-x64/packages/clang-14.0.6/eccodes-2.26.0'
      NETCDF_ROOT='/sw/bullseye-x64/packages/clang-14.0.6/netcdf-c-4.9.0-openmpi-4.1.3'
      PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw-bullseye/ppm-1.0.8.1-clang-14.0.6'
      YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw-bullseye/yaxt-0.9.3.1-clang-14.0.6'

      # The installations of NetCDF and ecCodes libraries do not provide '*.la'
      # files, which would trigger the RPATH injection:
      export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${NETCDF_ROOT}/lib:${LD_LIBRARY_PATH-}"
    ;;
  esac

  CC=clang
  CXX=clang++
  FC=gfortran

  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun) --oversubscribe"
}

#
# Sets variables for tests with NAG.
#
init_nag ()
{
  # Make the system gcc the first gcc in the PATH:
  export PATH="/usr/bin:${PATH-}"

  init_env

  case $(lsb_release -sc) in
    stretch)
      switch_for_module gcc/6.3.0 nag/6.2


      export PATH="/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/mpich-3.4.2-fzsvtw2/bin:${PATH}"

      ECCODES_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/eccodes-2.21.0-3sdngaq'
      NETCDF_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/netcdf-c-4.8.0-ogawg3w'
      PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/scales-ppm-1.0.7-5kdlwzv'
      YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/yaxt-0.9.2.1-rhq2ysn'

      # The installation of ecCodes does not provide '*.la' files, which would
      # trigger the RPATH injection:
      export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${LD_LIBRARY_PATH-}"
      # The installation of NetCDF library provides '*.la' files, which trigger
      # the linking to the HDF5 library but its installation does not provide
      # the '*.la' files, which would trigger the RPATH injection:
      HDF5_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/hdf5-1.10.7-pltg3k4'
      export LD_LIBRARY_PATH="${HDF5_ROOT}/lib:${LD_LIBRARY_PATH}"
    ;;
    bullseye)
      switch_for_module nag/7.1 mpich/3.4.3-nag-7.1

      ECCODES_ROOT='/sw/bullseye-x64/packages/nag-7.1/eccodes-2.26.0'
      NETCDF_ROOT='/sw/bullseye-x64/packages/nag-7.1/netcdf-c-4.9.0-mpich-3.4.3'
      PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw-bullseye/ppm-1.0.8.1-nag-7.1'
      YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw-bullseye/yaxt-0.9.3.1-nag-7.1'

      # The installations of NetCDF and ecCodes libraries do not provide '*.la'
      # files, which would trigger the RPATH injection:
      export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${NETCDF_ROOT}/lib:${LD_LIBRARY_PATH-}"
    ;;
  esac

  CC=gcc
  CXX=g++
  FC=nagfor

  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"
}
