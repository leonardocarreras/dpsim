// SPDX-FileCopyrightText: 2018-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/Config.h>
#include <dpsim-models/Definitions.h>

namespace CPS {
/// MNA interface to be used by elements that require recomputing of the system matrix
class MNAVariableCompInterface {
public:
  typedef std::shared_ptr<MNAVariableCompInterface> Ptr;
  typedef std::vector<Ptr> List;

  std::vector<std::pair<UInt, UInt>> mVariableSystemMatrixEntries;

  /// Returns true if one of the element paramters has changed
  virtual Bool hasParameterChanged() = 0;
};
} // namespace CPS
