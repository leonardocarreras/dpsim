from pathlib import Path
import os
import subprocess
import sys


def cleanup_shm() -> None:
    print("interface configuration: cleaning stale shared-memory files", flush=True)
    for shm_file in ["/dev/shm/dpsim.follower-to-leader", "/dev/shm/dpsim.leader-to-follower"]:
        try:
            os.remove(shm_file)
            print(f"interface configuration: removed {shm_file}", flush=True)
        except FileNotFoundError:
            print(f"interface configuration: not present {shm_file}", flush=True)


def villas_config() -> dict:
    return {
        "type": "shmem",
        "out": {"name": "/dpsim.follower-to-leader"},
        "in": {"name": "/dpsim.leader-to-follower"},
    }


def build_topology(dpsimpy, interface):
    print("component creation: start", flush=True)
    n1 = dpsimpy.emt.SimNode("BUS1", dpsimpy.PhaseType.ABC)
    nri = dpsimpy.emt.SimNode("BUS1_RI", dpsimpy.PhaseType.ABC)
    nvr = dpsimpy.emt.SimNode("BUS1_VR", dpsimpy.PhaseType.ABC)
    gnd = dpsimpy.emt.SimNode.gnd

    vl = dpsimpy.emt.ph3.VoltageSource("vl")
    vs = dpsimpy.emt.ph3.ControlledVoltageSource("vs")
    rvs = dpsimpy.emt.ph3.Resistor("rvs")
    ivs = dpsimpy.emt.ph3.Inductor("ivs")
    print("component creation: done", flush=True)

    print("parameter setting: start", flush=True)
    vl_ref = dpsimpy.Math.single_phase_variable_to_three_phase(dpsimpy.Math.polar(1000, 0))
    vl.set_parameters(vl_ref, 50)
    vs.set_parameters(dpsimpy.Math.single_phase_parameter_to_three_phase(0))
    rvs.set_parameters(dpsimpy.Math.single_phase_parameter_to_three_phase(0.1))
    ivs.set_parameters(dpsimpy.Math.single_phase_parameter_to_three_phase(0.01))
    print("parameter setting: done", flush=True)

    print("topology/system creation: connecting components", flush=True)
    vl.connect([n1, gnd])
    vs.connect([gnd, nvr])
    rvs.connect([nvr, nri])
    ivs.connect([nri, n1])

    print("interface configuration: mapping imports/exports", flush=True)
    interface.import_attribute(vs.attr("V_ref").derive_coeff(0, 0), 2, True, True, "VA", "V")
    interface.import_attribute(vs.attr("V_ref").derive_coeff(1, 0), 3, True, True, "VB", "V")
    interface.import_attribute(vs.attr("V_ref").derive_coeff(2, 0), 4, True, True, "VC", "V")
    interface.export_attribute(rvs.mIntfCurrent.derive_coeff(0, 0), 2, True, "IA", "A")
    interface.export_attribute(rvs.mIntfCurrent.derive_coeff(1, 0), 3, True, "IB", "A")
    interface.export_attribute(rvs.mIntfCurrent.derive_coeff(2, 0), 4, True, "IC", "A")

    system = dpsimpy.SystemTopology(50, [n1, nvr, nri], [vl, vs, rvs, ivs])
    print("topology/system creation: done", flush=True)

    refs = {
        "n1": n1,
        "nri": nri,
        "nvr": nvr,
        "vl": vl,
        "vs": vs,
        "rvs": rvs,
        "ivs": ivs,
    }
    return system, refs


def configure_logger(logger, refs) -> None:
    print("logger setup: wiring logger attributes", flush=True)
    logger.log_attribute("n1.v", "v", refs["n1"])
    logger.log_attribute("rvs.i", "i_intf", refs["rvs"])
    print("logger setup: done", flush=True)


def main() -> None:
    import numpy as np

    print("pid:", os.getpid())
    print("python:", sys.executable)
    print("numpy:", np.__version__)
    print("dpsimpy:", "not imported yet")

    print("imports: resolving repository paths", flush=True)
    root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip())
    build_path = root / "build"
    python_src_path = root / "python" / "src"
    for p in [build_path, python_src_path]:
        if str(p) not in sys.path:
            sys.path.insert(0, str(p))

    print("imports: importing dpsimpy and dpsimpyvillas", flush=True)
    import dpsimpy
    import dpsimpyvillas

    print(f"imports: dpsimpy -> {dpsimpy.__file__}", flush=True)
    print(f"imports: dpsimpyvillas -> {dpsimpyvillas.__file__}", flush=True)

    sim_name = "EMT_3Ph_simple_cosim_follower_v2_py"
    time_step = 0.01
    final_time = 10.0
    start_in_seconds = 5

    log_dir = root / "logs" / sim_name
    if not os.access(log_dir.parent, os.W_OK):
        log_dir = Path("/tmp/dpsim-logs") / root.name / sim_name
        print(f"logger setup: repo logs directory is not writable, using {log_dir}", flush=True)
    log_dir.mkdir(parents=True, exist_ok=True)
    dpsimpy.Logger.set_log_dir(str(log_dir))
    log_path = str(log_dir / f"{sim_name}.csv")
    print(f"logger setup: log file -> {log_path}", flush=True)

    print("interface configuration: creating VILLAS interface", flush=True)
    interface = dpsimpyvillas.InterfaceVillas(villas_config(), name="VillasInterface")
    print("interface configuration: interface created", flush=True)

    system, refs = build_topology(dpsimpy, interface)

    print("logger setup: creating real-time logger", flush=True)
    logger = dpsimpy.RealTimeDataLogger(log_path, final_time, time_step)
    configure_logger(logger, refs)

    print("before simulation run: creating simulation object", flush=True)
    sim = dpsimpy.RealTimeSimulation(sim_name, dpsimpy.LogLevel.info)
    sim.set_time_step(time_step)
    sim.set_final_time(final_time)
    sim.set_domain(dpsimpy.Domain.EMT)
    sim.do_system_matrix_recomputation(True)
    sim.set_system(system)
    sim.add_interface(interface)
    sim.add_logger(logger)

    # Keep Python references alive for all C++-backed objects until run completes.
    _keepalive = [sim, system, interface, logger] + list(refs.values())

    print("before simulation run: starting sim.run", flush=True)
    sim.run(start_in_seconds)
    print("after simulation run: sim.run returned", flush=True)
    print(f"after simulation run: log file -> {log_path}", flush=True)

    if not _keepalive:
        raise RuntimeError("unreachable")


if __name__ == "__main__":
    main()
