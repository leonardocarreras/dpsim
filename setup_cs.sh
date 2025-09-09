#!/usr/bin/env bash
set -euo pipefail

# --- Config (override via env) ---
SYSTEM_CPUS="${SYSTEM_CPUS:-0-1}"   # housekeeping CPUs (ranges/CSV)
RT_PRIO="${RT_PRIO:-90}"
TSTEP="${TSTEP:-0.00005}"
DUR="${DUR:-1}"
LOGFLAG="${LOGFLAG:-true}"
EXDIR="${EXDIR:-/home/leo/git/github/my/dpsim/build/dpsim-villas/examples/cxx}"
FOLLOWER="${FOLLOWER:-$EXDIR/EMT_Ph3_simple_cosim_follower}"
LEADER="${LEADER:-$EXDIR/EMT_Ph3_simple_cosim_leader}"

# --- Preconditions ---
[[ ${BASH_VERSINFO[0]} -ge 4 ]] || { echo "Need bash >= 4"; exit 1; }
[[ $EUID -eq 0 ]] || { echo "Run as root: sudo $0"; exit 1; }
[[ -x "$FOLLOWER" ]] || { echo "Missing follower: $FOLLOWER"; exit 1; }
[[ -x "$LEADER"   ]] || { echo "Missing leader:   $LEADER"; exit 1; }

# --- Helpers ---
expand_list() {  # "0-1,4,6-7" -> "0 1 4 6 7"
  echo "$1" | tr ',' ' ' | awk -v RS=' ' '
    function add(a,b){for(i=a;i<=b;i++)printf i" "}
    /^[0-9]+-[0-9]+$/ {split($0,r,"-"); add(r[1],r[2]); next}
    /^[0-9]+$/ {printf $0" "}
  '
}

# --- Discover online CPUs & cores ---
ALL=$(ls -d /sys/devices/system/cpu/cpu[0-9]* 2>/dev/null | sed -E 's/.*cpu([0-9]+)/\1/' | sort -n)
ONLINE=$(
  for c in $ALL; do
    f="/sys/devices/system/cpu/cpu${c}/online"
    if [[ -f "$f" ]]; then [[ "$(cat "$f")" -eq 1 ]] && echo "$c"; else echo "$c"; fi
  done | sort -n
)

SYS_SET=$(expand_list "$SYSTEM_CPUS" | tr ' ' '\n' | sed '/^$/d' | sort -n | uniq)

# Build (CPU -> CORE) map
declare -A CPU2CORE
while IFS=, read -r cpu core on; do
  [[ "$cpu" =~ ^# ]] && continue
  [[ "${on:-Y}" != "Y" ]] && continue
  CPU2CORE["$cpu"]="$core"
done < <(lscpu -p=CPU,CORE,ONLINE)

# Remove system CPUs, then group remaining CPUs by core (keep SMT siblings together)
declare -A CORE2CPUS
for cpu in $ONLINE; do
  if echo "$SYS_SET" | tr ' ' '\n' | grep -qx "$cpu"; then continue; fi
  core="${CPU2CORE[$cpu]:-}"
  [[ -z "$core" ]] && continue
  CORE2CPUS[$core]="${CORE2CPUS[$core]-} $cpu"
done

# Split cores into two disjoint sets: A (follower) and B (leader)
cores_sorted=$(printf "%s\n" "${!CORE2CPUS[@]}" | sort -n)
setA=()
setB=()
i=0
for core in $cores_sorted; do
  cpus="$(echo "${CORE2CPUS[$core]}" | xargs)"
  if (( i % 2 == 0 )); then setA+=($cpus); else setB+=($cpus); fi
  ((i++))
done

# Flatten & validate
CPUS_A=$(printf "%s " "${setA[@]}" | tr -s ' ' | sed 's/^ *//;s/ *$//' | tr ' ' ',' )
CPUS_B=$(printf "%s " "${setB[@]}" | tr -s ' ' | sed 's/^ *//;s/ *$//' | tr ' ' ',' )

if [[ -z "$CPUS_A" || -z "$CPUS_B" ]]; then
  echo "Not enough isolated CPUs to split. SYSTEM_CPUS='$SYSTEM_CPUS' ONLINE='$(echo $ONLINE)'"
  exit 1
fi

echo "🏠 System CPUs:            $(echo $SYS_SET | tr ' ' ',')"
echo "🛡️  Follower CPU set (A):  $CPUS_A"
echo "🛡️  Leader   CPU set (B):  $CPUS_B"

# --- Run follower (background) ---
echo "🚀 Follower → CPUs {$CPUS_A}, FIFO:$RT_PRIO"
taskset -c "$CPUS_A" chrt -f "$RT_PRIO" \
  "$FOLLOWER" -o "log=$LOGFLAG" -t "$TSTEP" -d "$DUR" \
  > follower.log 2>&1 &
fpid=$!
sleep 0.5
kill -0 "$fpid" 2>/dev/null || { echo "Follower exited early. See follower.log"; exit 1; }

# --- Run leader (foreground) ---
echo "🚀 Leader   → CPUs {$CPUS_B}, FIFO:$RT_PRIO"
taskset -c "$CPUS_B" chrt -f "$RT_PRIO" \
  "$LEADER" -o "log=$LOGFLAG" -t "$TSTEP" -d "$DUR"

echo "📜 Logs: follower.log (follower PID $fpid)"
