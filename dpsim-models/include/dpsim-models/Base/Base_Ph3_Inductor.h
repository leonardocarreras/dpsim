// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/AttributeList.h>
#include <dpsim-models/Definitions.h>

namespace CPS {
namespace Base {
namespace Ph3 {
class Inductor {
public:
  /// Inductance [H]
  const CPS::Attribute<Matrix>::Ptr mInductance;

  explicit Inductor(CPS::AttributeList::Ptr attributeList)
      : mInductance(attributeList->create<Matrix>("L")){};

  /// Sets model specific parameters
  void setParameters(Matrix inductance) { **mInductance = inductance; }
};
} // namespace Ph3
} // namespace Base
} // namespace CPS
