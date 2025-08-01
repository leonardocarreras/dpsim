sudo bash ~/prepare_rt.sh

lspci | grep Xil

module load dpsim

sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH taskset -c 9,11,13,15 chrt -f 99 ./build/dpsim-villas/examples/cxx/FpgaCosim3PhInfiniteBus -o log=false -t 0.00005 -d 10
