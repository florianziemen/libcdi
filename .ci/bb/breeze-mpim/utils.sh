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
  . /etc/profile.d/mpim.sh
  set -${saved_set}
  switch_for_module automake/1.16.1

  # Tell the custom installation of Automake 1.16 where to look for the libtool
  # macros:
  export ACLOCAL_PATH="/usr/share/aclocal:${ACLOCAL_PATH-}"

  SWIG_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/swig-4.0.2-busrrtn'
  RUBY_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/ruby-3.0.2-stqm2vj'
  PYTHON_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/python-2.7.18-zqrnwom'

  export PATH="${RUBY_ROOT}/bin:${PYTHON_ROOT}/bin:${SWIG_ROOT}/bin:${PATH-}"
}

#
# Sets variables for tests with GCC 6.3.0.
#
init_gcc630 ()
{
  init_env
  switch_for_module gcc/6.3.0

  CC=gcc
  CXX=g++
  FC=gfortran

  ECCODES_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/eccodes-2.21.0-3sdngaq'
  NETCDF_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/netcdf-c-4.8.0-fzupaca'
  UUID_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/libuuid-1.0.3-rm4kv2o'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  HDF5_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/hdf5-1.10.7-4l3frcp'
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${HDF5_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Sets variables for tests with NVHPC 21.3.0.
#
init_nv2130 ()
{
  init_env
  switch_for_module gcc/6.3.0

  export NVHPC='/data/mpi/sclab/sip/m300488/nvhpc'
  export NVLOCALRC="${NVHPC}/Linux_x86_64/21.3/compilers/bin/localrc.60300"
  export PATH="${NVHPC}/Linux_x86_64/21.3/compilers/bin:${PATH-}"

  CC=nvc
  CXX=nvc++
  FC=nvfortran

  export PATH="/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/mpich-3.4.2-sfxulsj/bin:${PATH-}"
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"

  ECCODES_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/eccodes-2.21.0-3sdngaq'
  NETCDF_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/netcdf-c-4.8.0-vjppuov'
  PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/scales-ppm-1.0.7-uy4z72r'
  YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/yaxt-0.9.2.1-lbqqemx'
  UUID_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/libuuid-1.0.3-rm4kv2o'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  HDF5_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nvhpc-21.3-sandybridge/hdf5-1.10.7-h7pvthl'
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${HDF5_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Sets variables for tests with Clang 12.0.1.
#
init_clang1201 ()
{
  init_env
  switch_for_module gcc/6.3.0

  export PATH="/data/mpi/sclab/sip/m300488/clang/12.0.1/bin:${PATH-}"
  # Clang does not inject RPATHs to the standard library in use:
  export LD_LIBRARY_PATH="/sw/stretch-x64/gcc/gcc-6.3.0/lib64:${LD_LIBRARY_PATH-}"

  CC=clang
  CXX=clang++
  FC=gfortran

  export PATH="/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/openmpi-4.1.1-6ydagyy/bin:${PATH-}"
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun) --oversubscribe"

  ECCODES_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/eccodes-2.21.0-3sdngaq'
  NETCDF_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/netcdf-c-4.8.0-z6r5guk'
  PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/scales-ppm-1.0.7-5mhatgx'
  YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/yaxt-0.9.2.1-cj2nf5j'
  UUID_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/libuuid-1.0.3-rm4kv2o'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  HDF5_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/clang-12.0.1-sandybridge/hdf5-1.10.7-4y54xnh'
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${HDF5_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Sets variables for tests with NAG 6.2.6223.
#
init_nag626223 ()
{
  init_env
  switch_for_module gcc/6.3.0 nag/6.2

  CC=gcc
  CXX=g++
  FC=nagfor

  export PATH="/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/mpich-3.4.2-fzsvtw2/bin:${PATH-}"
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"

  ECCODES_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/eccodes-2.21.0-3sdngaq'
  NETCDF_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/netcdf-c-4.8.0-ogawg3w'
  PPM_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/scales-ppm-1.0.7-5kdlwzv'
  YAXT_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/yaxt-0.9.2.1-rhq2ysn'
  UUID_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/gcc-6.3.0-sandybridge/libuuid-1.0.3-rm4kv2o'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  HDF5_ROOT='/data/mpi/sclab/sip/m300488/libcdi-ci-sw/nag-6.2-sandybridge/hdf5-1.10.7-pltg3k4'
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib:${HDF5_ROOT}/lib:${LD_LIBRARY_PATH-}"
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
