// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <dpsim-models/Base/Base_HalfDecouplingLine.h>

using namespace CPS;

template <typename VarType>
Base::HalfDecouplingLine<VarType>::HalfDecouplingLine(String uid, String name,
                                                      UInt numPhases,
                                                      Logger::Level logLevel)
    : CompositePowerComp<VarType>(uid, name, true, true, logLevel),
      mNumPhases(numPhases),
      mSrcCtrledCurrent(this->mAttributes->template create<MatrixVar<VarType>>(
          "i_src_ctrl", MatrixVar<VarType>::Zero(numPhases, 1))),
      mSrcRes(this->mAttributes->template create<Matrix>(
          "src_res", Matrix::Zero(numPhases, numPhases))),
      mReceivingVolt(
          this->mAttributes->template createDynamic<MatrixVar<VarType>>(
              "receiving_volt")),
      mReceivingCur(
          this->mAttributes->template createDynamic<MatrixVar<VarType>>(
              "receiving_cur")),
      mSendingVolt(this->mAttributes->template create<MatrixVar<VarType>>(
          "sending_volt", MatrixVar<VarType>::Zero(numPhases, 1))),
      mSendingCur(this->mAttributes->template create<MatrixVar<VarType>>(
          "sending_cur", MatrixVar<VarType>::Zero(numPhases, 1))),
      mSendingInitVolt(this->mAttributes->template create<MatrixComp>(
          "sending_init_volt", MatrixComp::Zero(numPhases, 1))),
      mReceivingInitVolt(this->mAttributes->template createDynamic<MatrixComp>(
          "receiving_init_volt")) {

  this->setVirtualNodeNumber(0);
  this->setTerminalNumber(1);
  **this->mIntfVoltage = MatrixVar<VarType>::Zero(numPhases, 1);
  **this->mIntfCurrent = MatrixVar<VarType>::Zero(numPhases, 1);
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::setParameters(Matrix resistance,
                                                      Matrix inductance,
                                                      Matrix capacitance) {
  mResistance = resistance;
  mInductance = inductance;
  mCapacitance = capacitance;

  mSurgeImpedance = (inductance * capacitance.inverse()).array().sqrt();
  mDelay = (inductance.array() * capacitance.array()).sqrt().maxCoeff();

  **mSrcRes = mSurgeImpedance + mResistance / 4;

  mSurgeImpedanceVar = mSurgeImpedance.template cast<VarType>();
  mLumpedResistanceVar = (mResistance / 4).template cast<VarType>();
  mTerminatingImpedanceInv =
      (mSurgeImpedanceVar + mLumpedResistanceVar).inverse();
  mDenomInv = ((mSurgeImpedanceVar + mLumpedResistanceVar) *
               (mSurgeImpedanceVar + mLumpedResistanceVar))
                  .inverse();

  SPDLOG_LOGGER_INFO(this->mSLog, "surge impedance: {}", mSurgeImpedance);
  SPDLOG_LOGGER_INFO(this->mSLog, "delay: {}", mDelay);

  this->mParametersSet = true;
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::setCouplingSource(
    typename Attribute<MatrixVar<VarType>>::Ptr receivingVolt,
    typename Attribute<MatrixVar<VarType>>::Ptr receivingCur) {
  mReceivingVolt->setReference(receivingVolt);
  mReceivingCur->setReference(receivingCur);
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::setInitialCouplingSource(
    Attribute<MatrixComp>::Ptr receivingInitVolt) {
  mReceivingInitVolt->setReference(receivingInitVolt);
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::setInitialInjection(
    const MatrixComp &power) {
  mInitialInjection = power;
  mInjectionSet = true;
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::publishInitialVoltage() {
  **mSendingInitVolt = this->initialVoltage(0);
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::setCommunicationStep(
    Real communicationStep, Real farTimeStep) {
  mCommunicationStep = communicationStep;
  mFarTimeStep = farTimeStep;
}

template <typename VarType>
UInt Base::HalfDecouplingLine<VarType>::blockLength(Real timeStep) const {
  Real ratio = mCommunicationStep / timeStep;
  UInt count = static_cast<UInt>(round(ratio));
  if (count < 1 || fabs(ratio - count) > 1e-9 * ratio)
    throw SystemError(
        "Communication step is not an integer multiple of the time step");
  return count;
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::sizeHistory(Real timeStep) {
  if (mDelay < timeStep)
    throw SystemError("Timestep too large for decoupling");

  mTimeStep = timeStep;
  mBufSize = static_cast<UInt>(ceil(mDelay / timeStep));
  mAlpha = 1 - (mBufSize - mDelay / timeStep);

  if (mCommunicationStep == 0.)
    mCommunicationStep = timeStep;
  if (mFarTimeStep == 0.)
    mFarTimeStep = timeStep;
  if (mCommunicationStep > mDelay)
    throw SystemError("Communication step longer than the travel time");

  mReceiveBlockLen = blockLength(timeStep);
  mSendBlockLen = blockLength(mFarTimeStep);

  Real ratio = mFarTimeStep / mTimeStep;
  mBufShift = ratio < 1. ? static_cast<UInt>(ceil(1. - ratio)) : 0;

  SPDLOG_LOGGER_INFO(this->mSLog,
                     "bufsize {} alpha {} send block {} receive block {}",
                     mBufSize, mAlpha, mSendBlockLen, mReceiveBlockLen);
}

template <typename VarType>
MatrixComp Base::HalfDecouplingLine<VarType>::injectionSteadyStateCurrent(
    const MatrixComp &voltNear) const {
  return (mInitialInjection.array() / voltNear.array()).conjugate();
}

template <typename VarType>
MatrixComp Base::HalfDecouplingLine<VarType>::distributedSteadyStateCurrent(
    const MatrixComp &voltNear, const MatrixComp &voltFar) const {
  Real theta = mSystemOmega * mDelay;
  Real cosTheta = cos(theta);
  Real sinTheta = sin(theta);

  MatrixComp identity = MatrixComp::Identity(mNumPhases, mNumPhases);
  MatrixComp admittance =
      mSurgeImpedance.cast<Complex>().inverse() / Complex(0., sinTheta);
  MatrixComp lumped = (mResistance / 4).cast<Complex>();

  MatrixComp sum =
      (identity + admittance * lumped * (cosTheta - 1.)).inverse() *
      (admittance * (cosTheta - 1.)) * (voltNear + voltFar);
  MatrixComp difference =
      (identity + admittance * lumped * (cosTheta + 1.)).inverse() *
      (admittance * (cosTheta + 1.)) * (voltNear - voltFar);

  return 0.5 * (sum + difference);
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::initializeSteadyState(Real omega,
                                                              Real timeStep) {
  mSystemOmega = omega;
  sizeHistory(timeStep);

  MatrixComp voltNode = this->initialVoltage(0);
  MatrixComp curNode =
      mInjectionSet
          ? injectionSteadyStateCurrent(voltNode)
          : distributedSteadyStateCurrent(voltNode, **mReceivingInitVolt);

  SPDLOG_LOGGER_INFO(this->mSLog, "steady state seed: v_k {} i_k {} from {}",
                     voltNode, curNode,
                     mInjectionSet ? "terminal injection" : "line ends");

  UInt length = mBufSize + mBufShift;
  mVoltBuf.resize(length);
  mCurBuf.resize(length);
  for (UInt idx = 0; idx < length; idx++) {
    Real lag = (length - idx) * mTimeStep;
    mVoltBuf[idx] = sampleAtLag(voltNode, omega, lag);
    mCurBuf[idx] = sampleAtLag(curNode, omega, lag);
  }
  mBufIdx = 0;
  mStepsSincePublish = 0;

  mNearVolt = sampleFromHistory(mVoltBuf, 0.);
  mNearCur = sampleFromHistory(mCurBuf, 0.);
  **mSendingVolt = historyBlock(mVoltBuf);
  **mSendingCur = historyBlock(mCurBuf);
}

template <typename VarType>
MatrixVar<VarType> Base::HalfDecouplingLine<VarType>::sampleFromHistory(
    const std::vector<MatrixVar<VarType>> &data, Real offset) const {
  UInt length = static_cast<UInt>(data.size());
  Real position = mBufShift + (1. - mAlpha) + offset;
  UInt whole = static_cast<UInt>(floor(position));
  Real weight = mAlpha - (offset - Real(whole) + Real(mBufShift));
  const MatrixVar<VarType> &c1 = data[(mBufIdx + whole) % length];
  const MatrixVar<VarType> &c2 = data[(mBufIdx + whole + 1) % length];
  return weight * c1 + (1 - weight) * c2;
}

template <typename VarType>
MatrixVar<VarType> Base::HalfDecouplingLine<VarType>::historyBlock(
    const std::vector<MatrixVar<VarType>> &data) const {
  MatrixVar<VarType> block(mNumPhases, mSendBlockLen);
  Real ratio = mFarTimeStep / mTimeStep;
  for (UInt col = 0; col < mSendBlockLen; col++)
    block.col(col) = sampleFromHistory(data, (col + 1) * ratio - 1.);
  return block;
}

template <>
Real Base::HalfDecouplingLine<Real>::carrierRotation(Real omega, Real delay) {
  return 1.;
}

template <>
Complex Base::HalfDecouplingLine<Complex>::carrierRotation(Real omega,
                                                           Real delay) {
  return std::polar(1., -omega * delay);
}

template <>
Matrix Base::HalfDecouplingLine<Real>::sampleAtLag(const MatrixComp &phasor,
                                                   Real omega, Real lag) {
  return (phasor * std::polar(1., -omega * lag)).real();
}

template <>
MatrixComp
Base::HalfDecouplingLine<Complex>::sampleAtLag(const MatrixComp &phasor,
                                               Real omega, Real lag) {
  return phasor;
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::computeSourceCurrent(
    Int timeStepCount) {
  if ((**mReceivingVolt).cols() != static_cast<Int>(mReceiveBlockLen))
    throw SystemError("Received history block has the wrong length");

  UInt column = static_cast<UInt>(timeStepCount) % mReceiveBlockLen;
  const MatrixVar<VarType> &voltNear = mNearVolt;
  const MatrixVar<VarType> &curNear = mNearCur;
  const auto voltFar = (**mReceivingVolt).col(column);
  const auto curFar = (**mReceivingCur).col(column);

  **mSrcCtrledCurrent =
      -mSurgeImpedanceVar * mDenomInv *
          (voltFar + (mSurgeImpedanceVar - mLumpedResistanceVar) * curFar) -
      mLumpedResistanceVar * mDenomInv *
          (voltNear + (mSurgeImpedanceVar - mLumpedResistanceVar) * curNear);
  **mSrcCtrledCurrent *= carrierRotation(mSystemOmega, mDelay);

  applySourceCurrent();
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::recordHistory() {
  mVoltBuf[mBufIdx] = historyVoltage();
  mCurBuf[mBufIdx] = historyCurrent();

  mBufIdx++;
  if (mBufIdx == mVoltBuf.size())
    mBufIdx = 0;

  mNearVolt = sampleFromHistory(mVoltBuf, 0.);
  mNearCur = sampleFromHistory(mCurBuf, 0.);

  mStepsSincePublish++;
  if (mStepsSincePublish == mReceiveBlockLen) {
    **mSendingVolt = historyBlock(mVoltBuf);
    **mSendingCur = historyBlock(mCurBuf);
    mStepsSincePublish = 0;
  }
}

template class CPS::Base::HalfDecouplingLine<Real>;
template class CPS::Base::HalfDecouplingLine<Complex>;
