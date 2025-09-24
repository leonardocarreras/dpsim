module load dpsim

# Limit system services and user sessions right now
sudo systemctl set-property --runtime system.slice AllowedCPUs=0-3,28-31
sudo systemctl set-property --runtime user.slice   AllowedCPUs=0-3,28-31
sudo systemctl set-property --runtime init.scope   AllowedCPUs=0-3,28-31

# Create a dedicated slice for the cosim on the isolated CPUs
sudo systemctl set-property --runtime cosim1.slice AllowedCPUs=4-27 CPUWeight=10000 IOWeight=10000

sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
systemd-run --scope --slice=cosim1.slice \
chrt -f 60 taskset -c 4-27 \
./build/dpsim-villas/examples/cxx/cosim-9bus-4order \
  -o log=false -t 0.00005 -d 30
