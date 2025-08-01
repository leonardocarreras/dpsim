source /global/spack/share/spack/setup-env.sh

spack load gcc@15
spack load cmake@3.30.5%gcc@15

spack load python@3.11%gcc@15
spack load py-pybind11@2.13

spack load fmt
spack load spdlog

module load villas-node/2025.07-dpsim-fpga
module load libcimpp/CGMES_2.4.15_16FEB2016-1b11d5c

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel -j
