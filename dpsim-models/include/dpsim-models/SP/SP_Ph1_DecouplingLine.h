// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <vector>

#include <dpsim-models/CompositePowerComp.h>
#include <dpsim-models/SP/SP_Ph1_ControlledCurrentSource.h>
#include <dpsim-models/SP/SP_Ph1_Resistor.h>

namespace CPS {
namespace SP {
namespace Ph1 {
class DecouplingLine : public CompositePowerComp<Complex>,
                       public SharedFactory<DecouplingLine> {
protected:
  Real mDelay;
  Real mSystemOmega = 0.;
  Real mResistance;
  Real mInductance, mCapacitance;
  Real mSurgeImpedance;

  std::shared_ptr<SP::Ph1::Resistor> mRes1, mRes2;
  std::shared_ptr<SP::Ph1::ControlledCurrentSource> mSrc1, mSrc2;
  Attribute<Complex>::Ptr mSrcCur1, mSrcCur2;

  std::vector<Complex> mVolt1, mVolt2, mCur1, mCur2;
  UInt mBufIdx = 0;
  UInt mBufSize;
  Real mAlpha;

  Complex interpolate(std::vector<Complex> &data);

public:
  typedef std::shared_ptr<DecouplingLine> Ptr;

  const Attribute<Complex>::Ptr mSrcCur1Ref;
  const Attribute<Complex>::Ptr mSrcCur2Ref;
  const Attribute<Matrix>::Ptr mStates;

  DecouplingLine(String uid, String name,
                 Logger::Level logLevel = Logger::Level::info);
  DecouplingLine(String name, Logger::Level logLevel = Logger::Level::info)
      : DecouplingLine(name, name, logLevel) {}

  void setParameters(Real resistance, Real inductance, Real capacitance);
  void step(Real time, Int timeStepCount);
  void postStep();

  void createSubComponents() override;
  void initializeParentFromNodesAndTerminals(Real frequency) override;

  void mnaParentInitialize(Real omega, Real timeStep,
                           Attribute<Matrix>::Ptr leftVector) override;
  void mnaParentPreStep(Real time, Int timeStepCount) override;
  void mnaParentPostStep(Real time, Int timeStepCount,
                         Attribute<Matrix>::Ptr &leftVector) override;
  void mnaParentAddPreStepDependencies(
      AttributeBase::List &prevStepDependencies,
      AttributeBase::List &attributeDependencies,
      AttributeBase::List &modifiedAttributes) override;
  void
  mnaParentAddPostStepDependencies(AttributeBase::List &prevStepDependencies,
                                   AttributeBase::List &attributeDependencies,
                                   AttributeBase::List &modifiedAttributes,
                                   Attribute<Matrix>::Ptr &leftVector) override;
  void mnaCompUpdateVoltage(const Matrix &leftVector) override;
  void mnaCompUpdateCurrent(const Matrix &leftVector) override;
};
} // namespace Ph1
} // namespace SP
} // namespace CPS
