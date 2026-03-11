/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#include <dpsim-models/EMT/EMT_Ph1_VariableResistor.h>

using namespace CPS;

EMT::Ph1::VariableResistor::VariableResistor(String uid, String name,
                                             Logger::Level logLevel)
    : Resistor(uid, name, logLevel) {}

void EMT::Ph1::VariableResistor::mnaCompInitialize(
    Real omega, Real timeStep, Attribute<Matrix>::Ptr leftVector) {
  Resistor::mnaCompInitialize(omega, timeStep, leftVector);
  mLastResistance = **mResistance;
}

Bool EMT::Ph1::VariableResistor::hasParameterChanged() {
  if (**mResistance != mLastResistance) {
    mConductance = 1.0 / **mResistance;
    mLastResistance = **mResistance;
    return true;
  }
  return false;
}
