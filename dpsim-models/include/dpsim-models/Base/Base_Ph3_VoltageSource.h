// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/AttributeList.h>
#include <dpsim-models/Definitions.h>

namespace CPS {
namespace Base {
namespace Ph3 {
class VoltageSource {
public:
  /// Sets model specific parameters
  virtual void setParameters(Complex voltageRef, Real srcFreq = -1){} = 0;
  virtual void updateVoltage() = 0;
};
} // namespace Ph3
} // namespace Base
} // namespace CPS
