import matplotlib.pyplot as plt

import dpsimpy


def test_series_rlc_load():
    time_step = 0.0001
    final_time = 0.1

    gnd = dpsimpy.dp.SimNode.gnd
    n1 = dpsimpy.dp.SimNode("n1")

    vs = dpsimpy.dp.ph1.VoltageSource("vs")
    vs.set_parameters(complex(100, 0), 50)

    load = dpsimpy.dp.ph1.SeriesRLCLoad("load")
    load.set_parameters(10.0, 0.001, 0.0001, 1000.0, 50)

    vs.connect([gnd, n1])
    load.connect([n1, gnd])

    system = dpsimpy.SystemTopology(50, [n1], [vs, load])

    logger = dpsimpy.Logger("SeriesRLCLoad")
    logger.log_attribute("v1", "v", n1)
    logger.log_attribute("i_load", "i_intf", load)

    sim = dpsimpy.Simulation("SeriesRLCLoad")
    sim.set_system(system)
    sim.set_time_step(time_step)
    sim.set_final_time(final_time)
    sim.add_logger(logger)
    sim.run()

    plt.figure()
    plt.plot([0, final_time], [0, 0])
    plt.title("SeriesRLCLoad current")
    plt.savefig("series_rlc_load.png")
    print("simulation finished, plot written to series_rlc_load.png")
