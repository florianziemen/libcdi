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
  :
}

#
# Sets variables for tests with Cray 11.0.0.
#
init_cray1100 ()
{
  init_env
  switch_for_module craype PrgEnv-cray cce/11.0.0 cray-mpich cray-netcdf

  CC=cc
  CXX=CC
  FC=ftn
  MPI_LAUNCH="$(which srun) -p cscsci -C gpu -A d56 -t 05:00"

  ECCODES_ROOT='/project/d56/libcdi-ci-sw/cce-11.0.0-haswell/eccodes-2.21.0-x3o7v65'
  PPM_ROOT='/project/d56/libcdi-ci-sw/cce-11.0.0-haswell/scales-ppm-1.0.6-43iofhw'
  YAXT_ROOT='/project/d56/libcdi-ci-sw/cce-11.0.0-haswell/yaxt-0.9.0-h6fg2ur'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib64:${PPM_ROOT}/lib:${YAXT_ROOT}/lib:${LD_LIBRARY_PATH-}"
}

#
# Sets variables for tests with PGI 20.1.1.
#
init_pgi2011 ()
{
  init_env
  switch_for_module craype PrgEnv-pgi pgi/20.1.1 cray-mpich cray-netcdf

  CC=cc
  CXX=CC
  FC=ftn
  MPI_LAUNCH="$(which srun) -p cscsci -C gpu -A d56 -t 05:00"

  ECCODES_ROOT='/project/d56/libcdi-ci-sw/pgi-20.1.1-haswell/eccodes-2.21.0-hxzyycf'
  PPM_ROOT='/project/d56/libcdi-ci-sw/pgi-20.1.1-haswell/scales-ppm-1.0.6-tjpvz46'
  # YAXT 0.9.0 cannot be built with PGI 20.1.1:
  YAXT_ROOT='/project/d56/libcdi-ci-sw/pgi-20.1.1-haswell/yaxt-0.7.0-p1-lirxrt4'

  # Here we fix a never-ending story with Libtool overlinkning, absence of
  # '*.la' files when they could help, and '-Wl,--disable/enable-new-dtags':
  export LD_LIBRARY_PATH="${ECCODES_ROOT}/lib64:${PPM_ROOT}/lib:${YAXT_ROOT}/lib:${LD_LIBRARY_PATH-}"
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
