// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <dpsim-models/Definitions.h>

namespace CPS {
struct PQData {
  Real p;
  Real q;
};

struct PowerProfile {
  std::map<Real, PQData> pqData;
  std::map<Real, Real> weightingFactors;
};
} // namespace CPS
