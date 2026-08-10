// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <dpsim-models/MathUtils.h>
#include <dpsim-models/SP/SP_Ph1_HalfDecouplingLine.h>

using namespace CPS;

SP::Ph1::HalfDecouplingLine::HalfDecouplingLine(String uid, String name,
                                                Logger::Level logLevel)
    : Base::HalfDecouplingLine<Complex>(uid, name, 1, logLevel) {}

void SP::Ph1::HalfDecouplingLine::setParameters(Real resistance,
                                                Real inductance,
                                                Real capacitance) {
  Base::HalfDecouplingLine<Complex>::setParameters(
      Matrix::Constant(1, 1, resistance), Matrix::Constant(1, 1, inductance),
      Matrix::Constant(1, 1, capacitance));
}

void SP::Ph1::HalfDecouplingLine::createSubComponents() {
  if (mSubCompCreated)
    return;
  mSubCompCreated = true;

  mSubRes = SP::Ph1::Resistor::make(**mName + "_r", mLogLevel);
  mSubRes->setParameters((**mSrcRes)(0, 0));
  mSubRes->connect({mTerminals[0]->node(), CPS::SimNode<Complex>::GND});
  addMNASubComponent(mSubRes, MNA_SUBCOMP_TASK_ORDER::NO_TASK,
                     MNA_SUBCOMP_TASK_ORDER::TASK_BEFORE_PARENT, false);

  mSubCtrledCurrentSource =
      SP::Ph1::ControlledCurrentSource::make(**mName + "_i", mLogLevel);
  mSubCtrledCurrentSource->setParameters((**mSrcCtrledCurrent)(0, 0));
  if (mSourceReversed)
    mSubCtrledCurrentSource->connect(
        {CPS::SimNode<Complex>::GND, mTerminals[0]->node()});
  else
    mSubCtrledCurrentSource->connect(
        {mTerminals[0]->node(), CPS::SimNode<Complex>::GND});
  addMNASubComponent(mSubCtrledCurrentSource, MNA_SUBCOMP_TASK_ORDER::NO_TASK,
                     MNA_SUBCOMP_TASK_ORDER::TASK_BEFORE_PARENT, true);
}

void SP::Ph1::HalfDecouplingLine::initializeParentFromNodesAndTerminals(
    Real frequency) {

  (**mIntfVoltage)(0, 0) = initialSingleVoltage(0);
  (**mIntfCurrent)(0, 0) = 0;
  publishInitialVoltage();
}

void SP::Ph1::HalfDecouplingLine::mnaParentInitialize(
    Real omega, Real timeStep, Attribute<Matrix>::Ptr leftVector) {
  initializeSteadyState(omega, timeStep);
}

void SP::Ph1::HalfDecouplingLine::applySourceCurrent() {
  mSubCtrledCurrentSource->mCurrentRef->set((**mSrcCtrledCurrent)(0, 0));
}

MatrixComp SP::Ph1::HalfDecouplingLine::historyVoltage() {
  return -mSubRes->intfVoltage();
}

MatrixComp SP::Ph1::HalfDecouplingLine::historyCurrent() {
  return -mSubRes->intfCurrent() + **mSrcCtrledCurrent;
}

void SP::Ph1::HalfDecouplingLine::mnaParentPreStep(Real time,
                                                   Int timeStepCount) {
  computeSourceCurrent(timeStepCount);
  mSubCtrledCurrentSource->mnaPreStep(time, timeStepCount);
  mnaCompApplyRightSideVectorStamp(**mRightVector);
}

void SP::Ph1::HalfDecouplingLine::mnaParentPostStep(
    Real time, Int timeStepCount, Attribute<Matrix>::Ptr &leftVector) {
  mnaCompUpdateVoltage(**leftVector);
  mnaCompUpdateCurrent(**leftVector);
  recordHistory();
}

void SP::Ph1::HalfDecouplingLine::mnaCompUpdateVoltage(
    const Matrix &leftVector) {
  (**mIntfVoltage)(0, 0) =
      Math::complexFromVectorElement(leftVector, matrixNodeIndex(0));
}

void SP::Ph1::HalfDecouplingLine::mnaCompUpdateCurrent(
    const Matrix &leftVector) {
  **mIntfCurrent = -mSubRes->intfCurrent() + **mSrcCtrledCurrent;
}

void SP::Ph1::HalfDecouplingLine::mnaParentAddPreStepDependencies(
    AttributeBase::List &prevStepDependencies,
    AttributeBase::List &attributeDependencies,
    AttributeBase::List &modifiedAttributes) {
  prevStepDependencies.push_back(mIntfCurrent);
  prevStepDependencies.push_back(mIntfVoltage);
  prevStepDependencies.push_back(mReceivingVolt);
  prevStepDependencies.push_back(mReceivingCur);
  modifiedAttributes.push_back(mRightVector);
}

void SP::Ph1::HalfDecouplingLine::mnaParentAddPostStepDependencies(
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
