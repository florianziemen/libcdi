#! /bin/bash
#SBATCH --job-name test_pio.booster_nvhpc_ompi.srun
#SBATCH -t 00:10:00
#SBATCH --nodes 2
#SBATCH --tasks-per-node 48
#SBATCH --partition develbooster
#SBATCH --account highresmonsoon
#SBATCH --output ./LOG.%x.%j.o
#SBATCH --error ./LOG.%x.%j.o
#SBATCH --gres=gpu:1
#SBATCH --parsable


# set -eux
set -ex
tests_dir=/p/scratch/highresmonsoon/xwang/cdi-pio-benchmark/cdi-pio-build-tj20220307-1.8.x-merge/cdi-5eb8cbb1d381fc78bced3fd769033946a99c9f7a/tests
base_dir="$(cd "$tests_dir/.."; pwd)"
qprefix="/p/scratch/highresmonsoon/cdi-pio-test_dir"
mkdir -p "$qprefix"
qprefix="$qprefix/files_pio_write"
set +x

nvhpc_version=21.5
openmpi_version=4.1.1
module --force purge
ml use "$OTHERSTAGES"
ml Stages/2020
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

set -x
cd "$base_dir" || exit 1
make -j 22
cd "$tests_dir" || exit 1

export SCT_PROC_CHOICE=SCT_SELECT_ALL
export SCT_CALLSTATS=1

# reduce output data to decrease test duration
# time ../libtool --mode=execute srun \
#   ./pio_write_deco2d.parallel "-qprefix=$qprefix" -qpio-role-scheme=last \
#   -c -m 384 -n 192 -z 95 -t 20 -y 60 -s 7 \
#   -f grb2 -p PIO_MPI_FW_AT_ALL -w 16

time ../libtool --mode=execute srun \
  ./pio_write_deco2d.parallel "-qprefix=$qprefix" -qpio-role-scheme=last \
  -c -m 768 -n 384 -z 95 -t 10 -y 120 -s 7 \
  -f grb2 -p PIO_MPI_FW_AT_ALL -w 16
set +x
