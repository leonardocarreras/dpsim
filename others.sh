sudo taskset -c 2-7 chrt -f 90  /home/leo/git/github/my/dpsim/build/dpsim-villas/examples/cxx/EMT_Ph3_simple_cosim_follower   -o log=true -t 0.00005 -d 10
sudo taskset -c 2-7 chrt -f 90   gdbserver :12345   /home/leo/git/github/my/dpsim/build/dpsim-villas/examples/cxx/EMT_Ph3_simple_cosim_follower   -o log=true -t 0.00005 -d 10

sudo systemctl set-property --runtime -- user.slice   AllowedCPUs=0-3
sudo systemctl set-property --runtime -- system.slice AllowedCPUs=0-3
sudo systemctl set-property --runtime -- init.scope   AllowedCPUs=0-3

sudo tee /etc/systemd/system/rt-leader.slice <<'EOF'
[Slice]
AllowedCPUs=10-15
EOF
sudo tee /etc/systemd/system/rt-follower.slice <<'EOF'
[Slice]
AllowedCPUs=4-9
EOF
sudo systemctl daemon-reload

# STARTUP ORDER: leader first (creates shmem, waits for follower), then follower.
# Starting follower first causes a deadlock: follower enters PreStep blocking-read
# before the leader completes its startup sync handshake.
# CPU split: keep 0-3 for housekeeping and use only one SMT sibling per core
# for RT work. This avoids placing follower workers on hyperthread siblings,
# which was measurably slower than using fewer physical cores.

# Step 1 — start leader in terminal A (it blocks at sync-wait)
sudo systemd-run --scope --slice=rt-leader.slice   chrt -f 90 taskset -c 14-15   /home/leo/git/github/my/paper/interfacepaper/dpsim/build/dpsim-villas/examples/cxx/EMT_Ph3_9bus_load6_cosim_leader   -o shm=/dpsim_sync_case -o log=true -t 0.00005 -d 10

# Step 2 — start follower in terminal B (connects to shmem, both sides proceed)
# Run 1: generate measurement profile (no sched_in yet)
sudo systemd-run --scope --slice=rt-follower.slice   chrt -f 90 taskset -c 4-13   /home/leo/git/github/my/paper/interfacepaper/dpsim/build/dpsim-villas/examples/cxx/EMT_Ph3_9bus_4order_cosim   -t 0.00005 -d 10 -o transport=shmem -o shm=/dpsim_sync_case -o log=true   -o threads=5 -o sched_cpus=4,6,8,10,12   -o sched_out=/home/leo/git/github/my/paper/interfacepaper/dpsim/logs/cosim-9bus-4order_EMT/measurements.csv

sudo systemd-run --scope --slice=rt-follower.slice   chrt -f 90 taskset -c 4-13   /home/leo/git/github/my/paper/interfacepaper/dpsim/build/dpsim-villas/examples/cxx/EMT_Ph3_9bus_4order_cosim   -t 0.00005 -d 10 -o transport=shmem -o shm=/dpsim_sync_case -o log=true -o sched_out=/home/leo/git/github/my/paper/interfacepaper/dpsim/logs/cosim-9bus-4order_EMT/measurements.csv

# Run 2+: use profile for fit report and optimized assignment
sudo systemd-run --scope --slice=rt-follower.slice   chrt -f 90 taskset -c 4,6,8,10,12   /home/leo/git/github/my/paper/interfacepaper/dpsim/build/dpsim-villas/examples/cxx/EMT_Ph3_9bus_4order_cosim   -t 0.00005 -d 10 -o transport=shmem -o shm=/dpsim_sync_case -o log=true   -o threads=5 -o sched_cpus=4,6,8,10,12   -o sched_in=/home/leo/git/github/my/paper/interfacepaper/dpsim/logs/cosim-9bus-4order_EMT/measurements.csv   -o sched_out=/home/leo/git/github/my/paper/interfacepaper/dpsim/logs/cosim-9bus-4order_EMT/measurements2.csv   -o sched_report=true