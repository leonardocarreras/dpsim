// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// Basic EMT to EMT co-simulation, ITM case B, leader process.
// This side is EMT and the voltage-source side: it holds the excitation and a
// controlled voltage source driven by the interface voltage, and exports the
// resulting current. It also leads the start synchronisation. Pair with
// EMTEMT_simple_cosim_follower_Isrc.

#include "CosimBasics.h"

using namespace DPsimCosimBasics;

int main(int argc, char *argv[]) {
  return runBasicCosim(argc, argv, Domain::EMT, Role::Leader,
                       SourceType::Voltage, "EMT_3Ph_simple_cosim_leader_Vsrc");
}
