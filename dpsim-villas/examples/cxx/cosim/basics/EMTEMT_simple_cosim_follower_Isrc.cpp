// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// Basic EMT to EMT co-simulation, ITM case B, follower process.
// This side is EMT and the current-source side: it injects the interface
// current into a load and exports the node voltage. It follows the leader's
// start synchronisation. Pair with EMTEMT_simple_cosim_leader_Vsrc.

#include "CosimBasics.h"

using namespace DPsimCosimBasics;

int main(int argc, char *argv[]) {
  return runBasicCosim(argc, argv, Domain::EMT, Role::Follower,
                       SourceType::Current,
                       "EMT_3Ph_simple_cosim_follower_Isrc");
}
