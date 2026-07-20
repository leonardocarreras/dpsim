// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// Mixed DP to EMT co-simulation, DP leader, ITM case A: DP leader is the current-source side, EMT follower the voltage-source side.
// Mixed EMT to DP pair: connect through the VILLAS bridge
// (configs/villas-emt-dp-basics.conf).

#include "CosimBasics.h"

using namespace DPsimCosimBasics;

int main(int argc, char *argv[]) {
  return runBasicCosim(argc, argv, Domain::DP, Domain::EMT, Role::Leader,
                       SourceType::Current, "DPEMT_simple_cosim_leader_Isrc");
}
