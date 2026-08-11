// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/AttributeList.h>
#include <dpsim-models/Definitions.h>

namespace CPS {
namespace Base {
namespace Ph1 {
class Capacitor {
public:
  /// Capacitance [F]
  const CPS::Attribute<Real>::Ptr mCapacitance;

  explicit Capacitor(CPS::AttributeList::Ptr attributeList)
      : mCapacitance(attributeList->create<Real>("C")){};

  /// Sets model specific parameters
  void setParameters(Real capacitance) { **mCapacitance = capacitance; }
};
} // namespace Ph1
} // namespace Base
} // namespace CPS
