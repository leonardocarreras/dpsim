// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <dpsim-models/SP/SP_Ph1_VDNode.h>

using namespace CPS;

SP::Ph1::VDNode::VDNode(String uid, String name, Logger::Level logLevel)
    : SimPowerComp<Complex>(uid, name, logLevel),
      mDeltaSetPoint(mAttributes->create<Real>("Delta_set")),
      mVoltageSetPointPerUnit(mAttributes->create<Real>("V_set_pu")) {}

SP::Ph1::VDNode::VDNode(String uid, String name, Real vSetPointPerUnit,
                        Logger::Level logLevel)
    : VDNode(uid, name, logLevel) {

  **mVoltageSetPointPerUnit = vSetPointPerUnit;
  **mDeltaSetPoint = 0.;
}

SP::Ph1::VDNode::VDNode(String uid, String name, Real vSetPointPerUnit,
                        Real delta, Logger::Level logLevel)
    : VDNode(uid, name, logLevel) {

  **mVoltageSetPointPerUnit = vSetPointPerUnit;
  **mDeltaSetPoint = delta;
}
