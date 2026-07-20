#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0
#
# Launch the WSCC 9-bus DP<->EMT co-simulation: three processes connected through
# VILLAS shared memory (EMT leader + DP follower + VILLAS bridge).
#
#   VILLAS bridge   configs/villas-emt-dp-cosim.conf
#   DP follower     DP_Ph1_9bus_4order_cosim_follower   (current-source side)
#   EMT leader      EMT_Ph3_9bus_load6_cosim_leader     (voltage-source side)
#
# Start order does not matter: the follower waits on the leader's TCP sync
# rendezvous (InterfaceCosimSync) and both run from the same published instant.
#
# This is the deterministic, non-real-time validation launch (no RT scheduling,
# no root). For the real-time launch, run each DPsim process under
#   chrt --fifo 99 <binary>
# and the bridge under `chrt --fifo 98 villas-node ...`, as root, with hugepages
# configured. The process wiring is otherwise identical.
#
# Paths are derived from the repository and overridable via environment:
#   DPSIM_BUILD_DIR    build tree (default: <repo>/build)
#   DPSIM_COSIM_CONF   bridge config (default: this script's dir/villas-emt-dp-cosim.conf)
#   DPSIM_VILLAS_NODE  villas-node binary (default: villas-node on PATH)
#   DPSIM_VILLAS_LIB   dir prepended to LD_LIBRARY_PATH (for a custom villas build)
#   DUR                simulated duration in seconds (default: 15)
#   RUNDIR             working dir for logs (default: <repo>/cosim-run)

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${DPSIM_BUILD_DIR:-$REPO_ROOT/build}"
BIN_DIR="$BUILD_DIR/dpsim-villas/examples/cxx"
CONF="${DPSIM_COSIM_CONF:-$SCRIPT_DIR/villas-emt-dp-cosim.conf}"
VILLAS_NODE="${DPSIM_VILLAS_NODE:-villas-node}"
DUR="${DUR:-15}"
RUNDIR="${RUNDIR:-$REPO_ROOT/cosim-run}"

LEADER="$BIN_DIR/EMT_Ph3_9bus_load6_cosim_leader"
FOLLOWER="$BIN_DIR/DP_Ph1_9bus_4order_cosim_follower"

if [ -n "${DPSIM_VILLAS_LIB:-}" ]; then
    export LD_LIBRARY_PATH="$DPSIM_VILLAS_LIB:${LD_LIBRARY_PATH:-}"
fi

for f in "$LEADER" "$FOLLOWER" "$CONF"; do
    if [ ! -e "$f" ]; then
        echo "ERROR: not found: $f" >&2
        echo "Build the examples first (targets EMT_Ph3_9bus_load6_cosim_leader," >&2
        echo "DP_Ph1_9bus_4order_cosim_follower) or set DPSIM_BUILD_DIR." >&2
        exit 1
    fi
done
if ! command -v "$VILLAS_NODE" >/dev/null 2>&1 && [ ! -x "$VILLAS_NODE" ]; then
    echo "ERROR: villas-node not found ($VILLAS_NODE). Set DPSIM_VILLAS_NODE." >&2
    exit 1
fi

# Clean stale shared-memory segments from previous runs.
rm -f /dev/shm/dpsim.* /dev/shm/sem.dpsim* 2>/dev/null || true
rm -rf "$RUNDIR"; mkdir -p "$RUNDIR"; cd "$RUNDIR"

cleanup() {
    for p in "${VP:-}" "${DP:-}" "${EMT:-}"; do
        [ -n "$p" ] && kill "$p" 2>/dev/null || true
    done
    wait 2>/dev/null || true
    rm -f /dev/shm/dpsim.* /dev/shm/sem.dpsim* 2>/dev/null || true
}
trap cleanup INT TERM EXIT

echo "== starting VILLAS bridge =="
"$VILLAS_NODE" "$CONF" > bridge.log 2>&1 &
VP=$!
sleep 1

echo "== starting DP follower =="
"$FOLLOWER" -d "$DUR" -o log=true > follower.log 2>&1 &
DP=$!

echo "== starting EMT leader =="
"$LEADER" -d "$DUR" -o log=true > leader.log 2>&1 &
EMT=$!

echo "PIDs: bridge=$VP follower=$DP leader=$EMT   (duration ${DUR}s, logs in $RUNDIR)"

# Wait until both DPsim processes have exited (bounded by duration + slack).
END=$(( DUR + 30 ))
for _ in $(seq 1 "$END"); do
    if ! kill -0 "$DP" 2>/dev/null && ! kill -0 "$EMT" 2>/dev/null; then break; fi
    sleep 1
done

kill -0 "$DP"  2>/dev/null && echo "follower still running (stopping)" || echo "follower exited"
kill -0 "$EMT" 2>/dev/null && echo "leader still running (stopping)"   || echo "leader exited"

LCSV="logs/EMT_3Ph_9bus_load6_cosim_leader/EMT_3Ph_9bus_load6_cosim_leader.csv"
echo "===== result ====="
if grep -qiE 'terminate|Segmentation|what\(\)|Aborted' leader.log follower.log; then
    echo "a process crashed; see leader.log / follower.log"
elif [ -f "$LCSV" ]; then
    echo "leader CSV rows: $(wc -l < "$LCSV")"
else
    echo "no leader CSV produced; see leader.log / bridge.log"
fi
echo "== done =="
