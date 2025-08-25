cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel -j20 --target EMT_cosim_9bus_4order