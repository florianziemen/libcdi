set -ex
tests_dir="$(pwd)"
base_dir="$(cd "$tests_dir/.."; pwd)"
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

set -x
export LD_PRELOAD="/p/software/juwelsbooster/stages/2020/software/UCX/1.11.1/lib/libuct.so.0:/p/software/juwelsbooster/stages/2020/software/UCX/1.11.1/lib/libucp.so.0:/p/software/juwelsbooster/stages/2020/software/UCX/1.11.1/lib/libucs.so.0:/p/software/juwelsbooster/stages/2020/software/UCX/1.11.1/lib/libucm.so.0"

cd "$base_dir" || exit 1
make -j 22
make install
cd "$tests_dir" || exit 1
