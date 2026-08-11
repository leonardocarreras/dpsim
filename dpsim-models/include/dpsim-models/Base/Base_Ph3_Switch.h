// SPDX-FileCopyrightText: 2018-2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/AttributeList.h>
#include <dpsim-models/Definitions.h>
namespace CPS {
namespace Base {
namespace Ph3 {
/// Dynamic Phasor Three-Phase Switch
class Switch {
protected:
  Bool mIsClosedPrev = false;

public:
  /// Resistance if switch is open [ohm]
  const CPS::Attribute<Matrix>::Ptr mOpenResistance;
  /// Resistance if switch is closed [ohm]
  const CPS::Attribute<Matrix>::Ptr mClosedResistance;
  /// Defines if Switch is open or closed
  const CPS::Attribute<Bool>::Ptr mIsClosed;

  explicit Switch(CPS::AttributeList::Ptr attributeList)
      : mOpenResistance(attributeList->create<Matrix>("R_open")),
        mClosedResistance(attributeList->create<Matrix>("R_closed")),
        mIsClosed(attributeList->create<Bool>("is_closed")){};

  ///
  void setParameters(Matrix openResistance, Matrix closedResistance,
                     Bool closed = false) {
    **mOpenResistance = openResistance;
    **mClosedResistance = closedResistance;
    **mIsClosed = closed;
  }
  void closeSwitch() { **mIsClosed = true; }
  void openSwitch() { **mIsClosed = false; }

  /// Check if switch is closed
  Bool isClosed() { return **mIsClosed; }
};
} // namespace Ph3
} // namespace Base
} // namespace CPS
