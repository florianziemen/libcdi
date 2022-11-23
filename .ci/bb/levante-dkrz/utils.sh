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
  switch_for_module ruby/3.0.2-gcc-11.2.0
}

#
# Sets variables for tests with GCC 11.2.0.
#
init_gcc1120 ()
{
  init_env
  switch_for_module gcc/11.2.0-gcc-11.2.0 openmpi/4.1.2-gcc-11.2.0

  CC=gcc
  CXX=g++
  FC=gfortran
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"

  ECCODES_ROOT='/sw/spack-levante/eccodes-2.21.0-4ywkk4'
  NETCDF_ROOT='/sw/spack-levante/netcdf-c-4.8.1-6qheqr'
  PPM_ROOT='/work/mh0287/m300488/libcdi-ci-sw/install/ppm-1.0.8-gcc-11.2.0'
  YAXT_ROOT='/work/mh0287/m300488/libcdi-ci-sw/install/yaxt-0.9.3-gcc-11.2.0'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib64:${NETCDF_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Sets variables for tests with Intel Classic 2021.5.0.
#
init_intelclassic202150 ()
{
  init_env
  # Try to make sure that the compiler works with the system gcc:
  module unload gcc
  # For whatever reason, Intel Classic 2021.5.0 is enabled with
  # intel-oneapi-compilers/2022.0.1-gcc-11.2.0:
  switch_for_module intel-oneapi-compilers/2022.0.1-gcc-11.2.0 openmpi/4.1.2-intel-2021.5.0

  AR=xiar
  CC=icc
  CXX=icpc
  FC=ifort
  MPICC=mpicc
  MPIFC=mpif90
  MPI_LAUNCH="$(which mpirun)"

  ECCODES_ROOT='/sw/spack-levante/eccodes-2.21.0-3ehkbb'
  NETCDF_ROOT='/sw/spack-levante/netcdf-c-4.8.1-2k3cmu'
  PPM_ROOT='/work/mh0287/m300488/libcdi-ci-sw/install/ppm-1.0.8-intel-classic-2021.5.0'
  YAXT_ROOT='/work/mh0287/m300488/libcdi-ci-sw/install/yaxt-0.9.3-intel-classic-2021.5.0'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib64:${NETCDF_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Accepts a path to a file containing the testsuite summary (either the
# 'test-suite.log' or the standard output of the 'make check' command) and
# checks whether all tests were run and passed.
#
check_all_tests_passed ()
{
  awk '/SKIP: /{
         print "ERROR: the total number of tests is not equal to the number of passed tests";
         exit 1;
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
