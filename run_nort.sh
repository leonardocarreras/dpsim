module load dpsim
ISOLATED_CPUS=2-21

sudo env \
LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
OMP_NUM_THREADS=18 \
OMP_PROC_BIND=spread \
OMP_PLACES=cores \
OMP_DYNAMIC=false \
OMP_DISPLAY_ENV=VERBOSE \
MKL_NUM_THREADS=1 \
OPENBLAS_NUM_THREADS=1 \
GOMP_CPU_AFFINITY="$ISOLATED_CPUS" \
taskset -c "$ISOLATED_CPUS" \
chrt -f 60 \
./build/dpsim-villas/examples/cxx/cosim-9bus-4order -o threads=10 log=false -t 0.00005 -d 200
