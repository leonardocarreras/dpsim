#!/usr/bin/env bash
set -euo pipefail

# === USER KNOBS (override via env when calling) =============================
CPUS="${CPUS:-2-21}"                          # CPUs for workload (range or list)
THREADS="${THREADS:-10}"                      # DPsim threads & OMP threads
RT_PRIO="${RT_PRIO:-60}"                      # 0 = no RT, else FIFO prio
EXE="${EXE:-./build/dpsim-villas/examples/cxx/cosim-9bus-4order}"
# If your parser needs separate -o flags, switch to: ARGS="-o threads=$THREADS -o log=false -t 0.00005 -d 20"
ARGS="${ARGS:--o threads=${THREADS},log=false -t 0.00005 -d 20}"

# --- sanity
if [[ $EUID -eq 0 ]]; then
  echo "Run this as your normal user (it will sudo for the run)."; exit 1
fi

# === capture your current (module) environment safely =======================
ENVFILE="$(mktemp)"
/usr/bin/env -0 > "$ENVFILE"
trap 'rm -f "$ENVFILE"' EXIT

# helper: expand 2-5,8 -> "2,3,4,5,8" to count threads if needed
expand_cpulist() {
  awk -v list="$1" 'BEGIN{
    n=split(list,a,","); for(i=1;i<=n;i++){
      if(a[i] ~ /-/){ split(a[i],r,"-"); for(j=r[1]; j<=r[2]; j++) printf("%s%d",(c++?",":""),j); }
      else { printf("%s%d",(c++?",":""),a[i]); }
    }
  }'
}

# === jump to root; create cpuset; re-import env; run ========================
sudo CPUS="$CPUS" THREADS="$THREADS" RT_PRIO="$RT_PRIO" EXE="$EXE" ARGS="$ARGS" ENVFILE="$ENVFILE" bash -s <<'ROOTSIDE'
set -euo pipefail

# pull in env from caller (NULL-separated), no brittle quoting
while IFS= read -r -d '' kv; do
  k="${kv%%=*}"
  v="${kv#*=}"

  # skip noisy vars & ALL exported functions (BASH_FUNC_*…)
  case "$k" in
    SHLVL|_|PWD|OLDPWD) continue ;;
    BASH_FUNC_*) continue ;;
  esac

  # only accept valid variable names (no % or funky chars)
  if [[ "$k" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
    export "$k=$v"
  fi
done < "$ENVFILE"


# OpenMP config (match DPsim threads)
export OMP_NUM_THREADS="${THREADS}"
export OMP_PROC_BIND=spread
export OMP_PLACES=cores
export OMP_DYNAMIC=false
export OMP_DISPLAY_ENV=VERBOSE
export GOMP_CPU_AFFINITY="${CPUS}"

# Prepare cpuset cgroup (cgroup v2). If cpuset controller not enabled, try to enable.
CGPARENT=/sys/fs/cgroup
echo +cpuset > "$CGPARENT/cgroup.subtree_control" 2>/dev/null || true

CG="$CGPARENT/dpsimrt-$$"
mkdir -p "$CG"

MEMS="$(cat "$CGPARENT/cpuset.mems.effective" 2>/dev/null || echo 0)"
echo "${CPUS}" > "$CG/cpuset.cpus"
echo "${MEMS}" > "$CG/cpuset.mems"

# Move this shell into the cpuset
echo $$ > "$CG/cgroup.procs"

# Build the command: taskset + optional chrt + exe + args
CMD=(taskset -c "${CPUS}")
if [[ "${RT_PRIO}" != "0" ]]; then
  CMD+=(chrt -f "${RT_PRIO}")
fi
CMD+=("${EXE}")

# Append ARGS with normal word-splitting
echo "Running: ${CMD[*]} ${ARGS}"
exec "${CMD[@]}" ${ARGS}
ROOTSIDE
