#! /bin/bash
#SBATCH --job-name test_pio.booster_nvhpc_ompi.srun
#SBATCH -t 00:10:00
#SBATCH --nodes 3
#SBATCH --tasks-per-node 48
#SBATCH --partition develbooster
#SBATCH --account highresmonsoon
#SBATCH --output ./LOG.%x.%j.o
#SBATCH --error ./LOG.%x.%j.o
#SBATCH --gres=gpu:1
#SBATCH --parsable


# set -eux
set -ex

source ./test_pio.booster_nvhpc_ompi.compile.sh
qprefix="/p/scratch/highresmonsoon/cdi-pio-test_dir"
mkdir -p "$qprefix"
qprefix="$qprefix/files_pio_write"

export SCT_PROC_CHOICE=SCT_SELECT_ALL
# export SCT_PROC_CHOICE=SCT_SELECT_ALL
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
