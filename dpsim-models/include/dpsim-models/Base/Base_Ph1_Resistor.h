// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/AttributeList.h>
#include <dpsim-models/Definitions.h>

namespace CPS {
namespace Base {
namespace Ph1 {
class Resistor {
public:
  ///Resistance [ohm]
  const CPS::Attribute<Real>::Ptr mResistance;

  explicit Resistor(CPS::AttributeList::Ptr attributeList)
      : mResistance(attributeList->create<Real>("R")){};

  ///
  void setParameters(Real resistance) { **mResistance = resistance; }
};
} // namespace Ph1
} // namespace Base
} // namespace CPS
