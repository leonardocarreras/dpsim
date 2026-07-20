// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// Basic DP to DP co-simulation, ITM case B, leader process (voltage-source side). Pair with DPDP_simple_cosim_follower_Isrc.

#include "CosimBasics.h"

using namespace DPsimCosimBasics;

int main(int argc, char *argv[]) {
  return runBasicCosim(argc, argv, Domain::DP, Role::Leader,
                       SourceType::Voltage, "DP_Ph1_simple_cosim_leader_Vsrc");
}
