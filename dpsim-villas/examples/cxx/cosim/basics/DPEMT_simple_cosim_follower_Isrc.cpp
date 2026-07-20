// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// Mixed DP to EMT co-simulation, DP leader, ITM case B: EMT follower is the current-source side.
// Mixed EMT to DP pair: connect through the VILLAS bridge
// (configs/villas-emt-dp-basics.conf).

#include "CosimBasics.h"

using namespace DPsimCosimBasics;

int main(int argc, char *argv[]) {
  return runBasicCosim(argc, argv, Domain::EMT, Domain::DP, Role::Follower,
                       SourceType::Current, "DPEMT_simple_cosim_follower_Isrc");
}
