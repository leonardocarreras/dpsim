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

sudo systemd-run --scope --slice=rt-follower.slice   chrt -f 90 taskset -c 4-9   /home/leo/git/github/my/dpsim/build/dpsim-villas/examples/cxx/EMT_Ph3_simple_cosim_follower_sync   -o log=true

sudo systemd-run --scope --slice=rt-leader.slice   chrt -f 90 taskset -c 10-15   /home/leo/git/github/my/dpsim/build/dpsim-villas/examples/cxx/EMT_Ph3_simple_cosim_leader_sync   -o log=true -t 0.00005 -d 10
