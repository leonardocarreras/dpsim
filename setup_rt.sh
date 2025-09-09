#!/usr/bin/env bash
set -euo pipefail

# ---------------- CONFIG ----------------
# Option A: leave empty to auto-pick housekeeping CPUs (CPU 0 + its HT sibling)
SYSTEM_CPUS="${SYSTEM_CPUS:-}"
OPEN_GUIS="${OPEN_GUIS:-1}"  # set to 0 to skip GUI launch

# -------------- ROOT CHECK --------------
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "❌ Run me as root: sudo $0"
  exit 1
fi

# --------- Detect desktop user (for GUI) ---------
DESKTOP_USER="${SUDO_USER:-$(logname 2>/dev/null || true)}"

run_as_user() {
  local cmd="$*"
  if [[ -n "${DESKTOP_USER:-}" ]]; then
    sudo -u "${DESKTOP_USER}" env \
      XDG_RUNTIME_DIR="/run/user/$(id -u "$DESKTOP_USER")" \
      DISPLAY="${DISPLAY:-:0}" \
      bash -lc "$cmd" || true
  fi
}

# --------- Install helpful tools ----------
echo "📦 Ensuring tools are present..."
export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -y gnome-system-monitor tuned util-linux linux-tools-common >/dev/null

# --------- Disable irqbalance ----------
systemctl stop irqbalance 2>/dev/null || true
systemctl disable irqbalance 2>/dev/null || true

# --------- CPU discovery ----------
ALL_CPUS=$(ls -d /sys/devices/system/cpu/cpu[0-9]* 2>/dev/null | sed -E 's/.*cpu([0-9]+)/\1/' | LC_ALL=C sort -n)
ONLINE_CPUS=$(
  for c in $ALL_CPUS; do
    f="/sys/devices/system/cpu/cpu${c}/online"
    if [[ -f "$f" ]]; then [[ "$(cat "$f")" -eq 1 ]] && echo "$c"; else echo "$c"; fi
  done | LC_ALL=C sort -n | uniq
)

# Auto-pick SYSTEM_CPUS if not provided: CPU 0 + its HT sibling (same core as CPU 0)
if [[ -z "${SYSTEM_CPUS}" ]]; then
  mapfile -t CSV < <(lscpu -p=CPU,CORE,ONLINE | grep -v '^#')
  cpu0_core=""
  for line in "${CSV[@]}"; do
    IFS=',' read -r cpu core on <<<"$line"
    if [[ "$cpu" == "0" ]]; then cpu0_core="$core"; break; fi
  done
  SYS_LIST="0"
  if [[ -n "$cpu0_core" ]]; then
    for line in "${CSV[@]}"; do
      IFS=',' read -r cpu core on <<<"$line"
      if [[ "$core" == "$cpu0_core" && "$cpu" != "0" && "$on" == "Y" ]]; then
        SYS_LIST+=" $cpu"; break
      fi
    done
  fi
else
  # Expand user-provided ranges "0-1,4,6-7"
  expand_cpu_list() {
    echo "$1" | tr ',' ' ' | awk -v RS=' ' '
      function add(a,b){for(i=a;i<=b;i++)printf i" "}
      /^[0-9]+-[0-9]+$/ {split($0,r,"-"); add(r[1],r[2]); next}
      /^[0-9]+$/ {printf $0" "}
    '
  }
  SYS_LIST=$(expand_cpu_list "$SYSTEM_CPUS" | tr ' ' '\n' | sed '/^$/d' | LC_ALL=C sort -n | uniq | tr '\n' ' ')
fi

ONLINE_SORTED=$(echo "$ONLINE_CPUS" | tr ' ' '\n' | sed '/^$/d' | LC_ALL=C sort -n | uniq)

# Compute isolated = ONLINE - SYSTEM using awk (no 'comm')
ISOLATE_LIST=$(
  awk 'NR==FNR {sys[$1]=1; next} !($1 in sys)' \
    <(printf "%s\n" $SYS_LIST) <(printf "%s\n" $ONLINE_SORTED) \
  | tr '\n' ' ' | sed 's/ $//'
)

if [[ -z "$ISOLATE_LIST" ]]; then
  echo "❌ No CPUs left to isolate. SYSTEM_CPUS='${SYS_LIST:-<auto-detect failed>}' ONLINE='$(echo "$ONLINE_SORTED" | tr '\n' ' ')'"
  exit 1
fi

ISOLATE_CSV=$(echo "$ISOLATE_LIST" | tr ' ' ',')

echo "🏠 System CPUs: $(echo "$SYS_LIST")"
echo "🛡️  Isolated CPUs: $(echo "$ISOLATE_LIST")"

# --------- Apply Tuned profile ----------
echo "🎛️  Applying Tuned profile: latency-performance"
systemctl enable tuned 2>/dev/null || true
systemctl start tuned 2>/dev/null || true
tuned-adm profile latency-performance || true

# --------- Move IRQs to System CPUs ----------
echo "🔃 Pinning IRQs to System CPUs..."
for f in /proc/irq/*/smp_affinity_list; do
  [[ -f "$f" ]] && echo "$(echo "$SYS_LIST" | tr ' ' ',')" > "$f" 2>/dev/null || true
done

# --------- cgroup mode detection ----------
CG2=0
[[ -f /sys/fs/cgroup/cgroup.controllers ]] && CG2=1

if [[ "$CG2" -eq 1 ]]; then
  echo "🧩 Detected cgroup v2 (systemd unified). Using systemd scopes (no cset)."

  cat <<EOF

Run your RT workload with CPU isolation via a transient systemd scope, e.g.:

  sudo systemd-run --scope \\
      -p AllowedCPUs=${ISOLATE_CSV} \\
      -p CPUSchedulingPolicy=fifo \\
      -p CPUSchedulingPriority=90 \\
      -- bash -lc '<your-command>'

Examples:
  sudo systemd-run --scope -p AllowedCPUs=${ISOLATE_CSV} -- chrt -f 90 ./your_app
  sudo systemd-run --scope -p AllowedCPUs=${ISOLATE_CSV} -- taskset -c ${ISOLATE_CSV} ./your_app

EOF
else
  echo "🧩 Detected legacy cgroup v1. You can use cset (cpuset fs)."
  echo "   Creating cpuset shield..."
  cset shield --reset 2>/dev/null || true
  cset shield --cpu="${ISOLATE_CSV}" --kthread=on
  echo "✅ cpuset shield ready. Use: sudo cset shield --exec -- chrt -f 90 ./your_app"
fi

# --------- Launch GUI (optional) ----------
if [[ "${OPEN_GUIS}" -eq 1 && -n "${DESKTOP_USER:-}" ]]; then
  echo "🟢 Launching GNOME System Monitor..."
  run_as_user "gnome-system-monitor &>/dev/null &"
fi

echo "✅ Setup complete."
