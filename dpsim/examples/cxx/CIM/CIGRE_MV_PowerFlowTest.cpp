// SPDX-FileCopyrightText: 2019-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <DPsim.h>
#include <dpsim-models/CIM/Reader.h>

using namespace std;
using namespace DPsim;
using namespace CPS;
using namespace CPS::CIM;

/*
 * This example runs the powerflow for the CIGRE MV benchmark system (neglecting the tap changers of the transformers)
 */
int main(int argc, char **argv) {

  // Find CIM files
  std::list<fs::path> filenames;
  if (argc <= 1) {
    filenames = DPsim::Utils::findFiles(
        {"Rootnet_FULL_NE_06J16h_DI.xml", "Rootnet_FULL_NE_06J16h_EQ.xml",
         "Rootnet_FULL_NE_06J16h_SV.xml", "Rootnet_FULL_NE_06J16h_TP.xml"},
        "build/_deps/cim-data-src/CIGRE_MV/NEPLAN/"
        "CIGRE_MV_no_tapchanger_With_LoadFlow_Results",
        "CIMPATH");
  } else {
    filenames = std::list<fs::path>(argv + 1, argv + argc);
  }

  String simName = "CIGRE-MV-NoTap";
  CPS::Real system_freq = 50;

  CIM::Reader reader(simName, Logger::Level::info, Logger::Level::info);
  SystemTopology system =
      reader.loadCIM(system_freq, filenames, CPS::Domain::SP);

  auto logger = DPsim::DataLogger::make(simName);
  for (auto node : system.mNodes) {
    logger->logAttribute(node->name() + ".V", node->attribute("v"));
  }

  Simulation sim(simName, Logger::Level::info);
  sim.setSystem(system);
  sim.setTimeStep(1);
  sim.setFinalTime(1);
  sim.setDomain(Domain::SP);
  sim.setSolverType(Solver::Type::NRP);
  sim.setSolverAndComponentBehaviour(Solver::Behaviour::Simulation);
  sim.doInitFromNodesAndTerminals(true);
  sim.addLogger(logger);

  sim.run();

  return 0;
}
