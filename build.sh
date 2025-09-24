module load dpsim

rm -rf build

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel --target cosim-9bus-4order
