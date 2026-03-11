/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#pragma once

#include <dpsim-models/SP/SP_Ph3_Resistor.h>
#include <dpsim-models/Solver/MNAVariableCompInterface.h>

namespace CPS {
namespace SP {
namespace Ph3 {
/// SP 3-phase resistor with variable (runtime-changeable) resistance.
/// Extends Resistor with hasParameterChanged() so the MNA solver
/// re-stamps the system matrix whenever setParameters() is called mid-sim.
class VariableResistor : public SP::Ph3::Resistor,
                         public MNAVariableCompInterface,
                         public SharedFactory<VariableResistor> {
protected:
  /// Resistance snapshot taken at last stamp; used to detect changes.
  Matrix mLastResistance = Matrix::Zero(3, 3);

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
} // namespace Ph3
} // namespace SP
} // namespace CPS
