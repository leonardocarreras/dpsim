#!/bin/bash
# Launch all three co-simulation processes simultaneously.
# Each opens its shmem side; VILLAS unblocks once all peers are connected.
# Must be run as root (or via sudo) for real-time scheduling and hugepages.

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="$SCRIPT_DIR/build/dpsim-villas/examples/cxx"

# Clean stale shmem segments from previous runs.
rm -f /dev/shm/dpsim.* 2>/dev/null || true

cleanup() {
    echo "Stopping all processes..."
    kill "$VILLAS_PID" "$DP_PID" "$EMT_PID" 2>/dev/null || true
    wait
    rm -f /dev/shm/dpsim.* 2>/dev/null || true
}
trap cleanup INT TERM

echo "Starting VILLAS bridge..."
chrt --fifo 98 villas node "$SCRIPT_DIR/configs/villas-emt-dp-cosim.conf" &
VILLAS_PID=$!

echo "Starting DP follower..."
chrt --fifo 99 "$BUILD/DP_Ph1_9bus_4order_cosim_follower" &
DP_PID=$!

echo "Starting EMT leader..."
chrt --fifo 99 "$BUILD/EMT_Ph3_9bus_load6_cosim_leader" &
EMT_PID=$!

echo "All processes started. PIDs: VILLAS=$VILLAS_PID  DP=$DP_PID  EMT=$EMT_PID"
echo "Press Ctrl+C to stop."
wait
