#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Role {
  std::string cmd;
  std::string args;
  std::string slice;
  std::string cpus;
  int rtprio = 0;
};

struct Config {
  Role leader;
  Role follower;
  std::string shm = "/dpsim_sync_case";
  double dt = 0.00005; // seconds
  double dur = 30.0;   // seconds
  std::string workdir; // optional; if set, prefix commands with cd
  std::string
      base_cpus; // optional; if set, constrain user/system/init to these CPUs
  bool help_base_cpus = false; // print help for base CPU affinity
  bool help_slices = false;    // print help for creating slices
};

static void usage() {
  std::cout << "cosimcmd - generate co-simulation commands to copy\n\n";
  std::cout << "Options:\n";
  std::cout << "  --leader-cmd <path>         Leader executable path\n";
  std::cout << "  --leader-args <args>        Extra args for leader\n";
  std::cout << "  --leader-slice <name>       systemd slice for leader (e.g., "
               "rt-leader.slice)\n";
  std::cout
      << "  --leader-cpus <set>         CPU set for leader (e.g., 10-15)\n";
  std::cout << "  --leader-rt <prio>          RT priority for leader (1-99)\n";
  std::cout << "  --follower-cmd <path>       Follower executable path\n";
  std::cout << "  --follower-args <args>      Extra args for follower\n";
  std::cout << "  --follower-slice <name>     systemd slice for follower\n";
  std::cout
      << "  --follower-cpus <set>       CPU set for follower (e.g., 4-9)\n";
  std::cout
      << "  --follower-rt <prio>        RT priority for follower (1-99)\n";
  std::cout
      << "  --shm <name>                SHM name (default /dpsim_sync_case)\n";
  std::cout << "  --dt <seconds>              Timestep in seconds (default "
               "0.00005)\n";
  std::cout
      << "  --dur <seconds>             Duration in seconds (default 30)\n";
  std::cout << "  --workdir <path>            Working directory to cd into "
               "before running\n";
  std::cout << "  --base-cpus <set>          Set AllowedCPUs for user.slice, "
               "system.slice, init.scope (e.g., 0-3)\n";
  std::cout << "  --help-base-cpus            Show example commands for base "
               "cgroups AllowedCPUs\n";
  std::cout << "  --help-slices               Show example systemd slice units "
               "(leader/follower)\n";
  std::cout << "\nExample:\n";
  std::cout << "    cosimcmd \\\n";
  std::cout << "        --leader-cmd "
               "/path/to/EMT_Ph3_simple_cosim_leader_v2_sync \\\n";
  std::cout << "        --leader-slice rt-leader.slice --leader-cpus 10-15 "
               "--leader-rt 95 \\\n";
  std::cout << "        --follower-cmd "
               "/path/to/EMT_Ph3_simple_cosim_follower_v2_sync \\\n";
  std::cout << "        --follower-slice rt-follower.slice --follower-cpus 4-9 "
               "--follower-rt 90 \\\n";
  std::cout << "        --shm /dpsim_sync_case --dt 0.00005 --dur 30\n";
}

static std::string build_run(const Role &r, const std::string &shm, double dt,
                             double dur, const std::string &workdir,
                             bool include_timing) {
  std::ostringstream cmd;
  cmd.setf(std::ios::fixed);
  cmd << std::setprecision(9);
  cmd << "sudo ";
  if (!workdir.empty()) {
    cmd << "bash -lc 'cd " << workdir << " && ";
  }
  cmd << "systemd-run --scope ";
  if (!r.slice.empty())
    cmd << "--slice=" << r.slice << " ";
  if (r.rtprio > 0)
    cmd << "chrt -f " << r.rtprio << " ";
  if (!r.cpus.empty())
    cmd << "taskset -c " << r.cpus << " ";
  cmd << r.cmd;
  if (!r.args.empty())
    cmd << " " << r.args;
  cmd << " -o log=true";
  if (include_timing)
    cmd << " -t " << dt << " -d " << dur;
  cmd << " -o shm=" << shm;
  if (!workdir.empty())
    cmd << "'"; // close bash -lc
  return cmd.str();
}

static void print_help_base_cpus(const std::string &base_set) {
  std::string set = base_set.empty() ? std::string("0-3") : base_set;
  std::cout << "Base CPU affinity (system-wide control groups)\n";
  std::cout << "These runtime changes constrain background load to CPUs in the "
               "set.\n";
  std::cout << "Adjust the CPU list as needed.\n\n";
  std::cout
      << "  sudo systemctl set-property --runtime -- user.slice   AllowedCPUs="
      << set << "\n";
  std::cout
      << "  sudo systemctl set-property --runtime -- system.slice AllowedCPUs="
      << set << "\n";
  std::cout
      << "  sudo systemctl set-property --runtime -- init.scope   AllowedCPUs="
      << set << "\n";
}

static void print_help_slices(const Role &leader, const Role &follower) {
  std::string leader_slice =
      leader.slice.empty() ? std::string("rt-leader.slice") : leader.slice;
  std::string leader_cpus =
      leader.cpus.empty() ? std::string("10-15") : leader.cpus;
  std::string follower_slice = follower.slice.empty()
                                   ? std::string("rt-follower.slice")
                                   : follower.slice;
  std::string follower_cpus =
      follower.cpus.empty() ? std::string("4-9") : follower.cpus;

  std::cout << "Systemd slice units (per-role CPU affinity)\n";
  std::cout
      << "Create slice units and reload daemon. Adjust names/CPU sets.\n\n";
  std::cout << "  sudo tee /etc/systemd/system/" << leader_slice
            << " <<'EOF'\n";
  std::cout << "  [Slice]\n";
  std::cout << "  AllowedCPUs=" << leader_cpus << "\n";
  std::cout << "  EOF\n";
  std::cout << "  sudo tee /etc/systemd/system/" << follower_slice
            << " <<'EOF'\n";
  std::cout << "  [Slice]\n";
  std::cout << "  AllowedCPUs=" << follower_cpus << "\n";
  std::cout << "  EOF\n";
  std::cout << "  sudo systemctl daemon-reload\n\n";
  std::cout << "When running, use: systemd-run --scope --slice=" << leader_slice
            << " ... (leader)\n";
  std::cout << "and: systemd-run --scope --slice=" << follower_slice
            << " ... (follower)\n";
}

int main(int argc, char **argv) {
  Config cfg;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](const char *name) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << name << "\n";
        usage();
        std::exit(1);
      }
      return std::string(argv[++i]);
    };
    if (a == "--leader-cmd")
      cfg.leader.cmd = need(a.c_str());
    else if (a == "--leader-args")
      cfg.leader.args = need(a.c_str());
    else if (a == "--leader-slice")
      cfg.leader.slice = need(a.c_str());
    else if (a == "--leader-cpus")
      cfg.leader.cpus = need(a.c_str());
    else if (a == "--leader-rt")
      cfg.leader.rtprio = std::stoi(need(a.c_str()));
    else if (a == "--follower-cmd")
      cfg.follower.cmd = need(a.c_str());
    else if (a == "--follower-args")
      cfg.follower.args = need(a.c_str());
    else if (a == "--follower-slice")
      cfg.follower.slice = need(a.c_str());
    else if (a == "--follower-cpus")
      cfg.follower.cpus = need(a.c_str());
    else if (a == "--follower-rt")
      cfg.follower.rtprio = std::stoi(need(a.c_str()));
    else if (a == "--shm")
      cfg.shm = need(a.c_str());
    else if (a == "--dt")
      cfg.dt = std::stod(need(a.c_str()));
    else if (a == "--dur")
      cfg.dur = std::stod(need(a.c_str()));
    else if (a == "--workdir")
      cfg.workdir = need(a.c_str());
    else if (a == "--base-cpus")
      cfg.base_cpus = need(a.c_str());
    else if (a == "--help-base-cpus")
      cfg.help_base_cpus = true;
    else if (a == "--help-slices")
      cfg.help_slices = true;
    else if (a == "--help" || a == "-h") {
      usage();
      return 0;
    } else {
      std::cerr << "Unknown option: " << a << "\n";
      usage();
      return 1;
    }
  }

  // Standalone help modes that do not require leader/follower
  if (cfg.help_base_cpus || cfg.help_slices) {
    if (cfg.help_base_cpus) {
      print_help_base_cpus(cfg.base_cpus);
      if (cfg.help_slices)
        std::cout << "\n";
    }
    if (cfg.help_slices) {
      print_help_slices(cfg.leader, cfg.follower);
    }
    return 0;
  }

  // If commands are missing, print usage and the two help sections by default.
  if (cfg.leader.cmd.empty() || cfg.follower.cmd.empty()) {
    usage();
    std::cout << "\n";
    print_help_base_cpus(cfg.base_cpus);
    std::cout << "\n";
    print_help_slices(cfg.leader, cfg.follower);
    return 0;
  }

  std::ostringstream out;
  if (!cfg.base_cpus.empty()) {
    out << "# Constrain base cgroups to specific CPUs (runtime)\n";
    out << "sudo systemctl set-property --runtime -- user.slice   AllowedCPUs="
        << cfg.base_cpus << "\n";
    out << "sudo systemctl set-property --runtime -- system.slice AllowedCPUs="
        << cfg.base_cpus << "\n";
    out << "sudo systemctl set-property --runtime -- init.scope   AllowedCPUs="
        << cfg.base_cpus << "\n\n";
  }
  out << "# Slice creation (optional)\n";
  if (!cfg.leader.slice.empty() && !cfg.leader.cpus.empty()) {
    out << "sudo tee /etc/systemd/system/" << cfg.leader.slice << " <<'EOF'\n";
    out << "[Slice]\nAllowedCPUs=" << cfg.leader.cpus << "\nEOF\n";
  }
  if (!cfg.follower.slice.empty() && !cfg.follower.cpus.empty()) {
    out << "sudo tee /etc/systemd/system/" << cfg.follower.slice
        << " <<'EOF'\n";
    out << "[Slice]\nAllowedCPUs=" << cfg.follower.cpus << "\nEOF\n";
  }
  if ((!cfg.leader.slice.empty() && !cfg.leader.cpus.empty()) ||
      (!cfg.follower.slice.empty() && !cfg.follower.cpus.empty())) {
    out << "sudo systemctl daemon-reload\n";
  }

  out << "\n# Leader\n";
  out << build_run(cfg.leader, cfg.shm, cfg.dt, cfg.dur, cfg.workdir, true)
      << "\n";
  out << "\n# Follower (no -t/-d; follows SHM timing)\n";
  out << build_run(cfg.follower, cfg.shm, cfg.dt, cfg.dur, cfg.workdir, false)
      << "\n";

  std::cout << out.str();
  return 0;
}
