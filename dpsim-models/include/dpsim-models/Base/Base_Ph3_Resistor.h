// SPDX-FileCopyrightText: 2018-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/AttributeList.h>
#include <dpsim-models/Definitions.h>
namespace CPS {
namespace Base {
namespace Ph3 {
class Resistor {
public:
  ///Resistance [ohm]
  const CPS::Attribute<Matrix>::Ptr mResistance;

  explicit Resistor(CPS::AttributeList::Ptr attributeList)
      : mResistance(attributeList->create<Matrix>("R")){};

  /// Sets model specific parameters
  void setParameters(Matrix resistance) { **mResistance = resistance; }
};
} // namespace Ph3
} // namespace Base
} // namespace CPS
