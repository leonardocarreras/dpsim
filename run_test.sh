module load dpsim

ISOLATED_CPUS=$(lscpu -p=CPU,CORE | grep -v '^#' | awk -F, '$2 != 0 { if (!seen[$2]++) print $1 }' | paste -sd "," -)
echo $ISOLATED_CPUS

sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ISOLATED_CPUS=$ISOLATED_CPUS \
taskset -c "$ISOLATED_CPUS" \
chrt -f 60 \
./build/dpsim-villas/examples/cxx/cosim-test \
  -o log=true -t 0.00005 -d 10
