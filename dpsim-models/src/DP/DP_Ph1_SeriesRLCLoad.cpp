#include <dpsim-models/DP/DP_Ph1_SeriesRLCLoad.h>

#include <iostream>

using namespace CPS;

DP::Ph1::SeriesRLCLoad::SeriesRLCLoad(String uid, String name,
                                      Logger::Level logLevel)
    : MNASimPowerComp<Complex>(uid, name, true, true, true, logLevel),
      mConductance(mAttributes->create<Complex>("conductance", 0)) {
  mPhaseType = PhaseType::Single;
  setTerminalNumber(2);
  **mIntfVoltage = MatrixComp::Zero(1, 1);
  **mIntfCurrent = MatrixComp::Zero(1, 1);
}

void DP::Ph1::SeriesRLCLoad::setParameters(Real resistance, Real inductance,
                                           Real capacitance, Real ratedPower,
                                           Real srcFreq) {
  mResistance = resistance;
  mInductance = inductance;
  mCapacitance = capacitance;
  mRatedPower = ratedPower;
  mSrcFreq = srcFreq;
  mParametersSet = true;
}

SimPowerComp<Complex>::Ptr DP::Ph1::SeriesRLCLoad::clone(String name) {
  auto copy = SeriesRLCLoad::make(name, mLogLevel);
  copy->setParameters(mResistance, mInductance, mCapacitance, mRatedPower,
                      mSrcFreq);
  return copy;
}

void DP::Ph1::SeriesRLCLoad::initializeFromNodesAndTerminals(Real frequency) {
  Real voltMag = std::abs(initialSingleVoltage(1) - initialSingleVoltage(0));
  Real peakVolt = voltMag * 0.8165;
  (**mIntfVoltage)(0, 0) = Complex(peakVolt, 0);
  (**mIntfCurrent)(0, 0) =
      (**mIntfVoltage)(0, 0) * mRatedPower / (voltMag * voltMag);
}

void DP::Ph1::SeriesRLCLoad::mnaCompInitialize(
    Real omega, Real timeStep, Attribute<Matrix>::Ptr leftVector) {
  updateMatrixNodeIndices();
  mTimeStep = timeStep;
  mOmegaSource = 2.0 * PI * mSrcFreq;

  Real condReal = mTimeStep / (2.0 * mInductance) + 1.0 / mResistance;
  mEquivCond = Complex(condReal, -mOmegaSource * mCapacitance);
  mEquivCurrent = Complex(0, 0);
  mCondHistory.clear();
  **mConductance = mEquivCond;
}

void DP::Ph1::SeriesRLCLoad::mnaCompApplySystemMatrixStamp(
    SparseMatrixRow &systemMatrix) {
  if (terminalNotGrounded(0))
    Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0),
                             matrixNodeIndex(0), mEquivCond);
  if (terminalNotGrounded(1))
    Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1),
                             matrixNodeIndex(1), mEquivCond);
  if (terminalNotGrounded(0) && terminalNotGrounded(1)) {
    Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0),
                             matrixNodeIndex(1), mEquivCond);
    Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1),
                             matrixNodeIndex(0), mEquivCond);
  }
}

void DP::Ph1::SeriesRLCLoad::mnaCompApplyRightSideVectorStamp(
    Matrix &rightVector) {
  mEquivCurrent = mPrevCurrent + mEquivCond * mPrevVoltage;
  if (terminalNotGrounded(0))
    Math::setVectorElement(rightVector, matrixNodeIndex(0), mEquivCurrent);
  if (terminalNotGrounded(1))
    Math::setVectorElement(rightVector, matrixNodeIndex(1), -mEquivCurrent);
}

void DP::Ph1::SeriesRLCLoad::mnaCompPreStep(Real time, Int timeStepCount) {
  Complex modulation = Complex(std::cos(mOmegaSource * time), 0);
  mEquivCond = mEquivCond * modulation;
  mCondHistory.push_back(mEquivCond);
  **mConductance = mEquivCond;
  mnaApplyRightSideVectorStamp(**mRightVector);
}

void DP::Ph1::SeriesRLCLoad::mnaCompPostStep(
    Real time, Int timeStepCount, Attribute<Matrix>::Ptr &leftVector) {
  mnaCompUpdateVoltage(**leftVector);
  mnaCompUpdateCurrent(**leftVector);
  if (std::abs((**mIntfCurrent)(0, 0)) < 1e-9)
    std::cout << "SeriesRLCLoad " << **mName
              << " carries no current at t=" << time << std::endl;
}

void DP::Ph1::SeriesRLCLoad::mnaCompAddPreStepDependencies(
    AttributeBase::List &prevStepDependencies,
    AttributeBase::List &attributeDependencies,
    AttributeBase::List &modifiedAttributes) {
  modifiedAttributes.push_back(mRightVector);
}

void DP::Ph1::SeriesRLCLoad::mnaCompAddPostStepDependencies(
    AttributeBase::List &prevStepDependencies,
    AttributeBase::List &attributeDependencies,
    AttributeBase::List &modifiedAttributes,
    Attribute<Matrix>::Ptr &leftVector) {
  attributeDependencies.push_back(leftVector);
  modifiedAttributes.push_back(mIntfVoltage);
  modifiedAttributes.push_back(mIntfCurrent);
}

void DP::Ph1::SeriesRLCLoad::mnaCompUpdateVoltage(const Matrix &leftVector) {
  (**mIntfVoltage)(0, 0) = 0;
  if (terminalNotGrounded(1))
    (**mIntfVoltage)(0, 0) =
        Math::complexFromVectorElement(leftVector, matrixNodeIndex(1));
  if (terminalNotGrounded(0))
    (**mIntfVoltage)(0, 0) =
        (**mIntfVoltage)(0, 0) -
        Math::complexFromVectorElement(leftVector, matrixNodeIndex(0));
  mPrevVoltage = (**mIntfVoltage)(0, 0);
}

void DP::Ph1::SeriesRLCLoad::mnaCompUpdateCurrent(const Matrix &leftVector) {
  (**mIntfCurrent)(0, 0) = mEquivCond * (**mIntfVoltage)(0, 0) + mEquivCurrent;
  mPrevCurrent = (**mIntfCurrent)(0, 0);
}
