// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/Definitions.h>
#include <dpsim-models/TopologicalPowerComp.h>

namespace CPS {
namespace Base {
/// @brief Base model of average inverter
class AvVoltageSourceInverterDQ {
protected:
  /// filter parameters
  Real mLf;
  Real mCf;
  Real mRf;
  Real mRc;

  /// transformer
  Real mTransformerNominalVoltageEnd1;
  Real mTransformerNominalVoltageEnd2;
  Real mTransformerRatedPower;
  Real mTransformerResistance;
  Real mTransformerInductance;
  Real mTransformerRatioAbs;
  Real mTransformerRatioPhase;

public:
  /// Setter for filter parameters
  void setFilterParameters(Real Lf, Real Cf, Real Rf, Real Rc);
  /// Setter for optional connection transformer
  void setTransformerParameters(Real nomVoltageEnd1, Real nomVoltageEnd2,
                                Real ratedPower, Real ratioAbs, Real ratioPhase,
                                Real resistance, Real inductance);
};
} // namespace Base
} // namespace CPS
