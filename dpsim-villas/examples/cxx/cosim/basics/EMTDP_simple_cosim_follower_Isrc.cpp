// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// Mixed EMT to DP co-simulation, EMT leader, ITM case B: DP follower is the current-source side.
// Mixed EMT to DP pair: connect through the VILLAS bridge
// (configs/villas-emt-dp-basics.conf).

#include "CosimBasics.h"

using namespace DPsimCosimBasics;

int main(int argc, char *argv[]) {
  return runBasicCosim(argc, argv, Domain::DP, Domain::EMT, Role::Follower,
                       SourceType::Current, "EMTDP_simple_cosim_follower_Isrc");
}
