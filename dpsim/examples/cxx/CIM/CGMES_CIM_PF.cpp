/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#include <DPsim.h>
#include <dpsim-models/CIM/Reader.h>

using namespace std;
using namespace DPsim;
using namespace CPS;
using namespace CPS::CIM;

/*
 * Runs a power flow over a CGMES dataset using the CIM reader in CGMES
 * power-flow mapping mode. Pass the CGMES files (EQ/SSH/SV/TP/DL) as
 * command-line arguments.
 */
int main(int argc, char **argv) {
  if (argc <= 1)
    throw SystemError("Provide the CGMES CIM files as command-line arguments.");

  std::list<fs::path> filenames(argv + 1, argv + argc);

  String simName = "CGMES_CIM_PF";
  CPS::Real system_freq = 50;

  CIM::Reader reader(simName, Logger::Level::info, Logger::Level::info);
  reader.setMappingMode(CPS::CIM::MappingMode::CgmesPowerFlow);
  SystemTopology system =
      reader.loadCIM(system_freq, filenames, CPS::Domain::SP,
                     CPS::PhaseType::Single, CPS::GeneratorType::PVNode);

  auto logger = DPsim::DataLogger::make(simName);
  for (auto node : system.mNodes)
    logger->logAttribute(node->name() + ".V", node->attribute("v"));

  Simulation sim(simName, Logger::Level::info);
  sim.setSystem(system);
  sim.setTimeStep(1);
  sim.setFinalTime(1);
  sim.setDomain(Domain::SP);
  sim.setSolverType(Solver::Type::NRP);
  sim.setSolverAndComponentBehaviour(Solver::Behaviour::Initialization);
  sim.doInitFromNodesAndTerminals(true);
  sim.addLogger(logger);

  sim.run();

  return 0;
}
