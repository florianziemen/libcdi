#! /bin/bash
nvhpc_version=21.5
openmpi_version=4.1.1
scriptdir=$(dirname "$0")
if [[ x$scriptdir != x ]]; then
  scriptdir+=/
fi
basedir=${scriptdir}../

ml use "$OTHERSTAGES"
ml Stages/2020
module purge
ml NVHPC/${nvhpc_version}-GCC-10.3.0
ml OpenMPI/${openmpi_version}
ml Ruby/2.7.2
ml UCX/1.10.1
ml netCDF-Fortran
ml CMake/3.18.0
# ecCodes is not directly available, load dependencies instead...
ml OpenGL/2020 libaec
ml ecCodes/2.21.0-nompi
module swap netCDF/4.7.4-serial netCDF/4.7.4

export LD_PRELOAD="/p/software/juwelsbooster/stages/2020/software/UCX/1.11.1/lib/libuct.so.0:/p/software/juwelsbooster/stages/2020/software/UCX/1.11.1/lib/libucp.so.0:/p/software/juwelsbooster/stages/2020/software/UCX/1.11.1/lib/libucs.so.0:/p/software/juwelsbooster/stages/2020/software/UCX/1.11.1/lib/libucm.so.0"

pushd ${basedir} || exit 1
make -j 16
set -x
make check -j 16
popd || exit 1

export SCT_PROC_CHOICE=SCT_REDUCE_ALL
export SCT_CALLSTATS=1

time ../libtool --mode=execute srun -N 5 -n 240 -t 00:10:00 -A highresmonsoon -p booster --gres=gpu:1\
  ./pio_write_deco2d -qpio-role-scheme=last \
  -c -m 768 -n 384 -z 95 -t 10 -y 120 -s 7 \
  -f nc4 -p PIO_MPI_FW_AT_ALL -w 16

export SCT_PROC_CHOICE=SCT_SELECT_ALL
export SCT_CALLSTATS=0

time ../libtool --mode=execute srun -N 5 -n 240 -t 00:10:00 -A highresmonsoon -p booster --gres=gpu:1\
  ./pio_write_deco2d -qpio-role-scheme=last \
  -c -m 768 -n 384 -z 95 -t 10 -y 120 -s 7 \
  -f nc4 -p PIO_MPI_FW_AT_ALL -w 16
