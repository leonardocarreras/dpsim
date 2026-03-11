/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#pragma once

#include <dpsim-models/EMT/EMT_Ph1_Resistor.h>
#include <dpsim-models/Solver/MNAVariableCompInterface.h>

namespace CPS {
namespace EMT {
namespace Ph1 {
/// EMT single-phase resistor with variable (runtime-changeable) resistance.
/// Extends Resistor with hasParameterChanged() so the MNA solver
/// re-stamps the system matrix whenever setParameters() is called mid-sim.
class VariableResistor : public EMT::Ph1::Resistor,
                         public MNAVariableCompInterface,
                         public SharedFactory<VariableResistor> {
protected:
  /// Resistance snapshot taken at last stamp; used to detect changes.
  Real mLastResistance = 0;

public:
  VariableResistor(String uid, String name,
                   Logger::Level logLevel = Logger::Level::off);
  VariableResistor(String name, Logger::Level logLevel = Logger::Level::off)
      : VariableResistor(name, name, logLevel) {}

  /// Calls parent initialize and stores resistance snapshot.
  void mnaCompInitialize(Real omega, Real timeStep,
                         Attribute<Matrix>::Ptr leftVector) override;

  /// Returns true (and refreshes conductance cache) if resistance changed since last call.
  Bool hasParameterChanged() override;
};
} // namespace Ph1
} // namespace EMT
} // namespace CPS
