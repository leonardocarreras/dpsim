// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <dpsim-models/Base/Base_AvVoltageSourceInverterDQ.h>

using namespace CPS;

void Base::AvVoltageSourceInverterDQ::setTransformerParameters(
    Real nomVoltageEnd1, Real nomVoltageEnd2, Real ratedPower, Real ratioAbs,
    Real ratioPhase, Real resistance, Real inductance) {

  mTransformerNominalVoltageEnd1 = nomVoltageEnd1;
  mTransformerNominalVoltageEnd2 = nomVoltageEnd2;
  mTransformerRatedPower = ratedPower;
  mTransformerResistance = resistance;
  mTransformerInductance = inductance;
  mTransformerRatioAbs = ratioAbs;
  mTransformerRatioPhase = ratioPhase;
};

void Base::AvVoltageSourceInverterDQ::setFilterParameters(Real Lf, Real Cf,
                                                          Real Rf, Real Rc) {
  mLf = Lf;
  mCf = Cf;
  mRf = Rf;
  mRc = Rc;
}
