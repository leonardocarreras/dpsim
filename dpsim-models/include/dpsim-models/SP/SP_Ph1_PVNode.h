// SPDX-FileCopyrightText: 2018-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <dpsim-models/SimPowerComp.h>

namespace CPS {
namespace SP {
namespace Ph1 {

class PVNode : public SimPowerComp<Complex>, public SharedFactory<PVNode> {
public:
  const Attribute<Real>::Ptr mVoltageSetPoint;
  const Attribute<Real>::Ptr mPowerSetPoint;
  const Attribute<Real>::Ptr mVoltagePerUnit;

  PVNode(String uid, String name, Logger::Level logLevel = Logger::Level::off);

  PVNode(String uid, String name, Real power, Real vSetPoint,
         Logger::Level logLevel = Logger::Level::off);

  PVNode(String uid, String name, Real power, Real vSetPoint, Real maxQ,
         Real ratedU, Real ratedS, Logger::Level logLevel = Logger::Level::off);
};

} // namespace Ph1
} // namespace SP
} // namespace CPS
