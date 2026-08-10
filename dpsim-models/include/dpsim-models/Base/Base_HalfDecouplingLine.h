// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/CompositePowerComp.h>
#include <dpsim-models/Definitions.h>

namespace CPS {
namespace Base {

template <typename VarType>
class HalfDecouplingLine : public CompositePowerComp<VarType> {
protected:
  Real mDelay = 0;
  Real mSystemOmega = 0;
  UInt mNumPhases;

  Matrix mResistance;
  Matrix mInductance;
  Matrix mCapacitance;
  Matrix mSurgeImpedance;

  MatrixVar<VarType> mSurgeImpedanceVar;
  MatrixVar<VarType> mLumpedResistanceVar;
  MatrixVar<VarType> mTerminatingImpedanceInv;
  MatrixVar<VarType> mDenomInv;

  std::vector<MatrixVar<VarType>> mVoltBuf;
  std::vector<MatrixVar<VarType>> mCurBuf;
  UInt mBufIdx = 0;
  UInt mBufSize = 0;
  UInt mBufShift = 0;
  Real mAlpha = 1.;
  Real mTimeStep = 0;

  MatrixVar<VarType> mNearVolt;
  MatrixVar<VarType> mNearCur;

  Real mCommunicationStep = 0;
  Real mFarTimeStep = 0;
  UInt mSendBlockLen = 1;
  UInt mReceiveBlockLen = 1;
  UInt mStepsSincePublish = 0;

  MatrixComp mInitialInjection;
  Bool mInjectionSet = false;
  Bool mSourceReversed = false;
  /// Node phasors are RMS line to line in every domain; EMT works in peak
  /// phase instantaneous quantities and so scales them, DP and SP do not.
  Real mNodeVoltageScale = 1.;

  UInt blockLength(Real timeStep) const;
  MatrixVar<VarType>
  sampleFromHistory(const std::vector<MatrixVar<VarType>> &data,
                    Real offset) const;
  MatrixVar<VarType>
  historyBlock(const std::vector<MatrixVar<VarType>> &data) const;
  void sizeHistory(Real timeStep);
  void computeSourceCurrent(Int timeStepCount);
  void recordHistory();

  MatrixComp distributedSteadyStateCurrent(const MatrixComp &voltNear,
                                           const MatrixComp &voltFar) const;
  MatrixComp injectionSteadyStateCurrent(const MatrixComp &voltNear) const;

  static VarType carrierRotation(Real omega, Real delay);
  static MatrixVar<VarType> sampleAtLag(const MatrixComp &phasor, Real omega,
                                        Real lag);

  virtual void applySourceCurrent() = 0;
  virtual MatrixVar<VarType> historyVoltage() = 0;
  virtual MatrixVar<VarType> historyCurrent() = 0;

public:
  const typename Attribute<MatrixVar<VarType>>::Ptr mSrcCtrledCurrent;
  const Attribute<Matrix>::Ptr mSrcRes;
  const typename Attribute<MatrixVar<VarType>>::Ptr mReceivingVolt;
  const typename Attribute<MatrixVar<VarType>>::Ptr mReceivingCur;
  const typename Attribute<MatrixVar<VarType>>::Ptr mSendingVolt;
  const typename Attribute<MatrixVar<VarType>>::Ptr mSendingCur;
  /// Terminal voltage phasor, exchanged at frozen time before the first step
  const Attribute<MatrixComp>::Ptr mSendingInitVolt;
  const Attribute<MatrixComp>::Ptr mReceivingInitVolt;

  HalfDecouplingLine(String uid, String name, UInt numPhases,
                     Logger::Level logLevel);

  void setParameters(Matrix resistance, Matrix inductance, Matrix capacitance);
  void
  setCouplingSource(typename Attribute<MatrixVar<VarType>>::Ptr receivingVolt,
                    typename Attribute<MatrixVar<VarType>>::Ptr receivingCur);
  void setInitialCouplingSource(Attribute<MatrixComp>::Ptr receivingInitVolt);
  /// Communication period, at most the travel time, and the far end time step
  void setCommunicationStep(Real communicationStep, Real farTimeStep);
  /// Reverses the internal history source connection. Every half buffers the
  /// terminal voltage with the same sign, so the source is connected the same
  /// way in every domain; the one exception is a domain whose current source
  /// family carries the opposite convention, which is Ph3 today. Set this when
  /// a domain's current source family changes, instead of editing the wiring.
  void setSourceReversed(Bool reversed) { mSourceReversed = reversed; }
  /// Terminal injection from the power flow, in the sign convention of the
  /// terminal. When it is set the seed needs no far-end quantity at all.
  void setInitialInjection(const MatrixComp &power);
  /// Publishes the terminal voltage phasor for the far end to read
  void publishInitialVoltage();
  /// Re-runnable frozen-time seed: exchanging again and calling this again is
  /// one boundary iteration at t=0
  void initializeSteadyState(Real omega, Real timeStep);

  Real delay() const { return mDelay; }
  UInt bufferSize() const { return mBufSize; }
  UInt sendBlockLength() const { return mSendBlockLen; }
  UInt receiveBlockLength() const { return mReceiveBlockLen; }
};
} // namespace Base
} // namespace CPS
