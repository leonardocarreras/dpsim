// SPDX-FileCopyrightText: 2017-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/AttributeList.h>
#include <dpsim-models/Definitions.h>

namespace CPS {
namespace Base {
namespace Ph1 {
class Inductor {
public:
  /// Inductance [H]
  const CPS::Attribute<Real>::Ptr mInductance;

  explicit Inductor(CPS::AttributeList::Ptr attributeList)
      : mInductance(attributeList->create<Real>("L")){};

  /// Sets model specific parameters
  void setParameters(Real inductance) { **mInductance = inductance; }
};
} // namespace Ph1
} // namespace Base
} // namespace CPS
