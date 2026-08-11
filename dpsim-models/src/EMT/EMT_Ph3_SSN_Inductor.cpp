// SPDX-FileCopyrightText: 2019-2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <dpsim-models/EMT/EMT_Ph3_SSN_Inductor.h>

using namespace CPS;

EMT::Ph3::SSN::Inductor::Inductor(String uid, String name,
                                  Logger::Level logLevel)
    : TwoTerminalVTypeSSNComp(uid, name, logLevel),
      Base::Ph3::Inductor(mAttributes) {}

SimPowerComp<Real>::Ptr EMT::Ph3::SSN::Inductor::clone(String name) {
  auto copy = SharedFactory<Inductor>::make(name, mLogLevel);
  copy->setParameters(**mInductance);
  return copy;
}

void EMT::Ph3::SSN::Inductor::setParameters(const Matrix &inductance) {
  if (inductance.rows() != 3 || inductance.cols() != 3)
    throw std::invalid_argument("Inductance matrix must have size 3 x 3.");

  **mInductance = inductance;

  Matrix inverseInductance = inductance.inverse();
  Matrix identity3 = Matrix::Identity(3, 3);

  // State choice:
  // x = i_abc  -> 3x1
  // u = v_abc  -> 3x1
  // y = i_abc  -> 3x1
  Matrix aMatrix = Matrix::Zero(3, 3);
  Matrix bMatrix = inverseInductance;
  Matrix cMatrix = identity3;
  Matrix dMatrix = Matrix::Zero(3, 3);

  SSNComp::setParameters(aMatrix, bMatrix, cMatrix, dMatrix);
}
