#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
# SPDX-License-Identifier: MPL-2.0
#
# Launch one basic ITM co-simulation pair (see dpsim-villas/examples/cxx/cosim/basics).
#
# Same-domain pairs (EMT to EMT, DP to DP) connect their two VILLAS shmem nodes
# directly, so this launches just the two DPsim processes with no bridge. Mixed
# EMT to DP pairs need the VILLAS bridge; use configs/run-emt-dp-cosim.sh for the
# 9-bus mixed example, or pass DPSIM_COSIM_CONF and a running villas-node.
#
# Usage:
#   run-basics-cosim.sh <leader-binary-name> <follower-binary-name>
# Example:
#   run-basics-cosim.sh EMTEMT_simple_cosim_leader_Isrc EMTEMT_simple_cosim_follower_Vsrc
#
# For a mixed EMT to DP pair, also set DPSIM_COSIM_CONF to the bridge config
# (configs/villas-emt-dp-basics.conf) and DPSIM_VILLAS_NODE to the villas-node
# binary; the script then starts the bridge as a third process.
#
# Environment overrides:
#   DPSIM_BUILD_DIR    build tree (default: <repo>/build)
#   DPSIM_VILLAS_LIB   dir prepended to LD_LIBRARY_PATH (for a custom villas build)
#   DPSIM_COSIM_CONF   bridge config; if set, a villas-node bridge is started
#   DPSIM_VILLAS_NODE  villas-node binary (default: villas-node on PATH)
#   DUR                simulated duration in seconds (default: 5)
#   RUNDIR             working dir for logs (default: <repo>/cosim-run-basics)

set -u

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <leader-binary-name> <follower-binary-name>" >&2
    exit 2
fi
LEADER_NAME="$1"
FOLLOWER_NAME="$2"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${DPSIM_BUILD_DIR:-$REPO_ROOT/build}"
BIN_DIR="$BUILD_DIR/dpsim-villas/examples/cxx"
DUR="${DUR:-5}"
RUNDIR="${RUNDIR:-$REPO_ROOT/cosim-run-basics}"

LEADER="$BIN_DIR/$LEADER_NAME"
FOLLOWER="$BIN_DIR/$FOLLOWER_NAME"

if [ -n "${DPSIM_VILLAS_LIB:-}" ]; then
    export LD_LIBRARY_PATH="$DPSIM_VILLAS_LIB:${LD_LIBRARY_PATH:-}"
fi

for f in "$LEADER" "$FOLLOWER"; do
    if [ ! -x "$f" ]; then
        echo "ERROR: not found or not executable: $f" >&2
        exit 1
    fi
done

rm -f /dev/shm/dpsim.* /dev/shm/sem.dpsim* 2>/dev/null || true
rm -rf "$RUNDIR"; mkdir -p "$RUNDIR"; cd "$RUNDIR"

cleanup() {
    for p in "${VP:-}" "${FP:-}" "${LP:-}"; do
        [ -n "$p" ] && kill "$p" 2>/dev/null || true
    done
    wait 2>/dev/null || true
    rm -f /dev/shm/dpsim.* /dev/shm/sem.dpsim* 2>/dev/null || true
}
trap cleanup INT TERM EXIT

VP=""
if [ -n "${DPSIM_COSIM_CONF:-}" ]; then
    VILLAS_NODE="${DPSIM_VILLAS_NODE:-villas-node}"
    echo "== starting VILLAS bridge =="
    "$VILLAS_NODE" "$DPSIM_COSIM_CONF" > bridge.log 2>&1 &
    VP=$!
    sleep 1
fi

echo "== starting follower $FOLLOWER_NAME =="
"$FOLLOWER" -d "$DUR" -o log=true > follower.log 2>&1 &
FP=$!
sleep 1
echo "== starting leader $LEADER_NAME =="
"$LEADER" -d "$DUR" -o log=true > leader.log 2>&1 &
LP=$!
echo "PIDs: follower=$FP leader=$LP   (duration ${DUR}s, logs in $RUNDIR)"

END=$(( DUR + 30 ))
for _ in $(seq 1 "$END"); do
    if ! kill -0 "$FP" 2>/dev/null && ! kill -0 "$LP" 2>/dev/null; then break; fi
    sleep 1
done

echo "===== result ====="
if grep -qiE 'terminate|Segmentation|Aborted' leader.log follower.log; then
    echo "a process crashed; see leader.log / follower.log"
else
    grep -h "Simulation finished" leader.log follower.log >/dev/null 2>&1 \
        && echo "both processes finished" || echo "check leader.log / follower.log"
fi
echo "== done =="
