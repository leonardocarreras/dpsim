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
void Base::HalfDecouplingLine<VarType>::sizeHistory(Real timeStep) {
  if (mDelay < timeStep)
    throw SystemError("Timestep too large for decoupling");

  mTimeStep = timeStep;
  mBufSize = static_cast<UInt>(ceil(mDelay / timeStep));
  mAlpha = 1 - (mBufSize - mDelay / timeStep);
  SPDLOG_LOGGER_INFO(this->mSLog, "bufsize {} alpha {}", mBufSize, mAlpha);
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

  MatrixComp voltNear = mHistorySign * voltNode;
  MatrixComp curNear = mHistorySign * curNode;

  SPDLOG_LOGGER_INFO(this->mSLog, "steady state seed: v_k {} i_k {} from {}",
                     voltNear, curNear,
                     mInjectionSet ? "terminal injection" : "line ends");

  mVoltBuf.resize(mBufSize);
  mCurBuf.resize(mBufSize);
  for (UInt idx = 0; idx < mBufSize; idx++) {
    Real lag = (mBufSize - idx) * mTimeStep;
    mVoltBuf[idx] = sampleAtLag(voltNear, omega, lag);
    mCurBuf[idx] = sampleAtLag(curNear, omega, lag);
  }
  mBufIdx = 0;

  **mSendingVolt = interpolate(mVoltBuf);
  **mSendingCur = interpolate(mCurBuf);
}

template <typename VarType>
MatrixVar<VarType> Base::HalfDecouplingLine<VarType>::interpolate(
    const std::vector<MatrixVar<VarType>> &data) const {
  const MatrixVar<VarType> &c1 = data[mBufIdx];
  const MatrixVar<VarType> &c2 =
      mBufIdx == mBufSize - 1 ? data[0] : data[mBufIdx + 1];
  return mAlpha * c1 + (1 - mAlpha) * c2;
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
  const MatrixVar<VarType> &voltNear = **mSendingVolt;
  const MatrixVar<VarType> &curNear = **mSendingCur;
  const MatrixVar<VarType> &voltFar = **mReceivingVolt;
  const MatrixVar<VarType> &curFar = **mReceivingCur;

  if (timeStepCount == 0) {
    **mSrcCtrledCurrent = curNear - mTerminatingImpedanceInv * voltNear;
  } else {
    **mSrcCtrledCurrent =
        -mSurgeImpedanceVar * mDenomInv *
            (voltFar + (mSurgeImpedanceVar - mLumpedResistanceVar) * curFar) -
        mLumpedResistanceVar * mDenomInv *
            (voltNear + (mSurgeImpedanceVar - mLumpedResistanceVar) * curNear);
    **mSrcCtrledCurrent *= carrierRotation(mSystemOmega, mDelay);
  }

  applySourceCurrent();
}

template <typename VarType>
void Base::HalfDecouplingLine<VarType>::recordHistory() {
  mVoltBuf[mBufIdx] = historyVoltage();
  mCurBuf[mBufIdx] = historyCurrent();

  mBufIdx++;
  if (mBufIdx == mBufSize)
    mBufIdx = 0;

  **mSendingVolt = interpolate(mVoltBuf);
  **mSendingCur = interpolate(mCurBuf);
}

template class CPS::Base::HalfDecouplingLine<Real>;
template class CPS::Base::HalfDecouplingLine<Complex>;
