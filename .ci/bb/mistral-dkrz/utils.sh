#
# Accepts a list of environment modules and loads them witch conflict
# resolution.
#
switch_for_module ()
{
  for sfm_module in "$@"; do
    sfm_module_full=
    sfm_module_short=
    case $sfm_module in
      */*)
        # The module is provided with the version part:
        sfm_module_full=$sfm_module
        sfm_module_short=`echo $sfm_module | sed 's%/[^/]*$%%'` ;;
      *)
        # Only the name of the module is provided, get the default version:
        sfm_module_full=`module show $sfm_module 2>&1 | sed -n "s%^[^ \t]*/\\($sfm_module.*\\):%\\1%p"`
        sfm_module_short=$sfm_module ;;
    esac

    # A space-separated list of modules that are already loaded:
    sfm_loaded_full=`module -t list 2>&1 | tr '\n' ' ' | sed 's/^.*Modulefiles://' | sed 's/(default)//g'`

    # Check whether the requested module if already loaded:
    if test -n "$sfm_module_full"; then
      case " $sfm_loaded_full " in
        *" $sfm_module_full "*)
          echo "module $sfm_module is already loaded"
          continue ;;
      esac
    fi

    # A list of modules in conflict:
    sfm_conflicts=`module show $sfm_module 2>&1 | sed -n 's/^conflict\(.*\)/\1/p' | tr '\n\t' '  '`

    # A list of loaded modules without version parts:
    sfm_loaded_short=`echo "$sfm_loaded_full" | sed 's%\([^ ][^ ]*\)/[^ ]*%\1%g'`

    # Add the short name of the module to the list of conflicts:
    sfm_conflicts="$sfm_conflicts $sfm_module_short"

    # A list of loaded modules that are in conflict with the requested module:
    sfm_loaded_conflicts=
    for sfm_conflict in $sfm_conflicts""; do
      sfm_loaded_list=      
      case $sfm_conflict in
        */*)
          # The conflict is specified with the version part:
          sfm_loaded_list=$sfm_loaded_full ;;
        *)
          # The conflict is specified without the version part:
          sfm_loaded_list=$sfm_loaded_short ;;
      esac

      # Check that the conflicted module is loaded:
      case " $sfm_loaded_list " in
        *" $sfm_conflict "*)
          # The conflict is loaded, check whether it is already added to the
          # list:
          case " $sfm_loaded_conflicts " in
            *" $sfm_conflict "*) ;;
            *)
              # The conflict is not in the list, append:
              sfm_loaded_conflicts="$sfm_loaded_conflicts $sfm_conflict" ;;
          esac
        ;;
      esac
    done

    # Calculate the number of modules that must be unloaded to before loading
    # the requested module:
    sfm_loaded_conflicts_count=`echo $sfm_loaded_conflicts | wc -w`

    case $sfm_loaded_conflicts_count in
      0)
        # None of the conflicting modules is loaded:
        sfm_cmd="module load $sfm_module" ;;
      1)
        # There is only one module that must be unloaded, use switch command:
        sfm_cmd="module switch $sfm_loaded_conflicts $sfm_module" ;;
      *)
        # There is more than one module that must be unloaded, unload all of
        # them and then load the requested one:
        sfm_cmd="module unload $sfm_loaded_conflicts && module load $sfm_module" ;;
    esac

    echo "$sfm_cmd"
    eval "$sfm_cmd"
  done
}

#
# Initializes the environment.
#
init_env ()
{
  saved_set=$-
  set +eu
  . /sw/rhel6-x64/etc/profile.mistral
  module use /sw/spack-rhel6/spack/modules/linux-rhel6-haswell
  set -${saved_set}
  switch_for_module automake libtool autoconf
}

#
# Sets variables for tests with GCC 6.4.0.
#
init_gcc640 ()
{
  init_env
  switch_for_module gcc/6.4.0 openmpi/2.0.2p1_hpcx-gcc64

  CC=gcc
  CXX=g++
  FC=gfortran
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"

  GRIBAPI_ROOT='/sw/rhel6-x64/grib_api/grib_api-1.15.0-gcc48'
  NETCDF_ROOT='/sw/rhel6-x64/netcdf/netcdf_c-4.4.0-parallel-openmpi2-gcc64'
  PPM_ROOT='/work/mh0287/m300488/cdi-pio-sw/ppm-1.0.6-gcc64'
  YAXT_ROOT='/work/mh0287/m300488/cdi-pio-sw/yaxt-0.9.0-gcc64'
}

#
# Sets variables for tests with Intel 17.0.6.
#
init_intel1706 ()
{
  init_env
  switch_for_module gcc/6.4.0 intel/17.0.6 openmpi/2.0.2p1_hpcx-intel14

  CC=icc
  CXX=icpc
  FC=ifort
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"

  GRIBAPI_ROOT='/sw/rhel6-x64/grib_api/grib_api-1.15.0-gcc48'
  NETCDF_ROOT='/sw/rhel6-x64/netcdf/netcdf_c-4.4.0-parallel-openmpi2-intel14'
  PPM_ROOT='/work/mh0287/m300488/cdi-pio-sw/ppm-1.0.6-intel17'
  YAXT_ROOT='/work/mh0287/m300488/cdi-pio-sw/yaxt-0.9.0-intel17'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  PNETCDF_ROOT='/sw/rhel6-x64/netcdf/parallel_netcdf-1.6.1-openmpi2-intel14'
  export LD_LIBRARY_PATH="${PNETCDF_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Sets variables for tests with NAG 6.2.6223.
#
init_nag626223 ()
{
  init_env
  switch_for_module gcc/6.4.0 nag/6.2

  # There is no environment module for MPICH:
  MPICH_ROOT='/mnt/lustre01/work/k20200/sw/spack/opt/spack/linux-rhel6-x86_64/nag-6.2/mpich-3.2.1-zxw7jhgz7qk4kim5jtziybfzigtiauxj'
  export PATH="${MPICH_ROOT}/bin:${PATH-}"

  CC=gcc
  CXX=g++
  FC=nagfor
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"

  ECCODES_ROOT='/mnt/lustre01/work/k20200/sw/spack/opt/spack/linux-rhel6-x86_64/nag-6.2/eccodes-2.5.0-givay23bqhgzreuk5um2wtjsumebwbt4'
  NETCDF_ROOT='/mnt/lustre01/work/k20200/sw/spack/opt/spack/linux-rhel6-x86_64/gcc-8.3.0-works/netcdf-4.7.0-eftv5u36ogb5awbb27hffiiv5rib4kqj'
  PPM_ROOT='/work/mh0287/m300488/cdi-pio-sw/ppm-1.0.6-nag62'
  YAXT_ROOT='/work/mh0287/m300488/cdi-pio-sw/yaxt-0.9.0-nag62'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Sets variables for tests with PGI 19.9.0.
#
init_pgi1990 ()
{
  init_env
  switch_for_module pgi/19.9-spack openmpi/2.0.2p2-pgi-19.9

  CC=pgcc
  CXX=pgc++
  FC=pgfortran
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which orterun)"

  ECCODES_ROOT='/sw/spack-rhel6/eccodes-2.19.1-cqlkd7'
  NETCDF_ROOT='/sw/spack-rhel6/netcdf-c-4.7.4-22ody2'
  PPM_ROOT='/work/mh0287/m300488/cdi-pio-sw/ppm-1.0.6-pgi19'
  YAXT_ROOT='/work/mh0287/m300488/cdi-pio-sw/yaxt-0.9.0-pgi19'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  HDF5_ROOT='/sw/spack-rhel6/hdf5-1.10.7-xynrbi'
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib64:${NETCDF_ROOT}/lib:${HDF5_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Accepts a path to a file containing the testsuite summary (either the
# 'test-suite.log' or the standard output of the 'make check' command) and
# checks whether all tests were run and passed.
#
check_all_tests_passed ()
{
  awk '/^# TOTAL: /{ total=$3 };
       /^# PASS: /{ pass=$3 };
       END {
         if ( total == "" || total != pass ) {
           print "ERROR: the total number of tests is not equal to the number of passed tests";
           exit 1;
         }
       }' $1 >&2
}

#
# Checks whether the current working directory or any of its subdirectories
# contain a file and fails with an error message if that is the case.
#
check_no_files_in_cwd ()
{
  cnfic_files=`find . -type f 2>/dev/null`
  if test -n "$cnfic_files"; then
    {
      echo "ERROR: the current working directory contains undeleted files:"
      echo "$cnfic_files"
    } >&2
    exit 1
  fi
}
