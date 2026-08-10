// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <dpsim-models/EMT/EMT_Ph3_HalfDecouplingLine.h>
#include <dpsim-models/MathUtils.h>

using namespace CPS;

EMT::Ph3::HalfDecouplingLine::HalfDecouplingLine(String uid, String name,
                                                 Logger::Level logLevel)
    : Base::HalfDecouplingLine<Real>(uid, name, 3, logLevel) {
  mPhaseType = PhaseType::ABC;
  mHistorySign = -1.;
}

void EMT::Ph3::HalfDecouplingLine::createSubComponents() {
  if (mSubCompCreated)
    return;
  mSubCompCreated = true;

  mSubRes = EMT::Ph3::Resistor::make(**mName + "_r", mLogLevel);
  mSubRes->setParameters(**mSrcRes);
  /* As in EMT::Ph3::DecouplingLine, the terminating resistor is connected from
     GND to the terminal, since the Ph3 resistor has the opposite sign
     convention for voltage and current compared to its Ph1 counterpart. */
  mSubRes->connect({SimNode::GND, mTerminals[0]->node()});
  addMNASubComponent(mSubRes, MNA_SUBCOMP_TASK_ORDER::NO_TASK,
                     MNA_SUBCOMP_TASK_ORDER::TASK_BEFORE_PARENT, false);

  mSubCtrledCurrentSource =
      EMT::Ph3::ControlledCurrentSource::make(**mName + "_i", mLogLevel);
  mSubCtrledCurrentSource->setParameters(**mSrcCtrledCurrent);
  mSubCtrledCurrentSource->connect({mTerminals[0]->node(), SimNode::GND});
  addMNASubComponent(mSubCtrledCurrentSource, MNA_SUBCOMP_TASK_ORDER::NO_TASK,
                     MNA_SUBCOMP_TASK_ORDER::TASK_BEFORE_PARENT, true);
}

void EMT::Ph3::HalfDecouplingLine::initializeParentFromNodesAndTerminals(
    Real frequency) {

  **mIntfVoltage = initialVoltage(0).real();
  **mIntfCurrent = Matrix::Zero(3, 1);

  // The sending quantities follow the internal sign convention of
  // EMT::Ph3::DecouplingLine, in which the recorded voltage is the negated
  // terminal voltage.
  **mSendingVolt = mHistorySign * (**mIntfVoltage);
  **mSendingCur = Matrix::Zero(3, 1);
  publishInitialVoltage();
}

void EMT::Ph3::HalfDecouplingLine::mnaParentInitialize(
    Real omega, Real timeStep, Attribute<Matrix>::Ptr leftVector) {
  initializeSteadyState(omega, timeStep);
}

void EMT::Ph3::HalfDecouplingLine::applySourceCurrent() {
  mSubCtrledCurrentSource->mCurrentRef->set(**mSrcCtrledCurrent);
}

Matrix EMT::Ph3::HalfDecouplingLine::historyVoltage() {
  return mHistorySign * mSubRes->intfVoltage();
}

Matrix EMT::Ph3::HalfDecouplingLine::historyCurrent() {
  return mHistorySign * mSubRes->intfCurrent() + **mSrcCtrledCurrent;
}

void EMT::Ph3::HalfDecouplingLine::mnaParentPreStep(Real time,
                                                    Int timeStepCount) {
  computeSourceCurrent(timeStepCount);
  mSubCtrledCurrentSource->mnaPreStep(time, timeStepCount);
  mnaCompApplyRightSideVectorStamp(**mRightVector);
}

void EMT::Ph3::HalfDecouplingLine::mnaParentPostStep(
    Real time, Int timeStepCount, Attribute<Matrix>::Ptr &leftVector) {
  mnaCompUpdateVoltage(**leftVector);
  mnaCompUpdateCurrent(**leftVector);
  recordHistory();
}

void EMT::Ph3::HalfDecouplingLine::mnaCompUpdateVoltage(
    const Matrix &leftVector) {
  (**mIntfVoltage)(0, 0) =
      Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 0));
  (**mIntfVoltage)(1, 0) =
      Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 1));
  (**mIntfVoltage)(2, 0) =
      Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 2));
}

void EMT::Ph3::HalfDecouplingLine::mnaCompUpdateCurrent(
    const Matrix &leftVector) {
  **mIntfCurrent = -mSubRes->intfCurrent() + **mSrcCtrledCurrent;
}

void EMT::Ph3::HalfDecouplingLine::mnaParentAddPreStepDependencies(
    AttributeBase::List &prevStepDependencies,
    AttributeBase::List &attributeDependencies,
    AttributeBase::List &modifiedAttributes) {
  prevStepDependencies.push_back(mIntfCurrent);
  prevStepDependencies.push_back(mIntfVoltage);
  prevStepDependencies.push_back(mReceivingVolt);
  prevStepDependencies.push_back(mReceivingCur);
  modifiedAttributes.push_back(mRightVector);
}

void EMT::Ph3::HalfDecouplingLine::mnaParentAddPostStepDependencies(
    AttributeBase::List &prevStepDependencies,
    AttributeBase::List &attributeDependencies,
    AttributeBase::List &modifiedAttributes,
    Attribute<Matrix>::Ptr &leftVector) {
  attributeDependencies.push_back(leftVector);
  modifiedAttributes.push_back(mIntfVoltage);
  modifiedAttributes.push_back(mIntfCurrent);
  modifiedAttributes.push_back(mSendingVolt);
  modifiedAttributes.push_back(mSendingCur);
}
