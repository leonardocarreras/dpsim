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
  Real mHistorySign = 1.;

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
  Real mAlpha = 1.;

  MatrixVar<VarType>
  interpolate(const std::vector<MatrixVar<VarType>> &data) const;
  void sizeHistory(Real timeStep);
  void seedHistory(const MatrixVar<VarType> &volt,
                   const MatrixVar<VarType> &cur);
  void computeSourceCurrent(Int timeStepCount);
  void recordHistory();

  static VarType carrierRotation(Real omega, Real delay);

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

  HalfDecouplingLine(String uid, String name, UInt numPhases,
                     Logger::Level logLevel);

  void setParameters(Matrix resistance, Matrix inductance, Matrix capacitance);
  void
  setCouplingSource(typename Attribute<MatrixVar<VarType>>::Ptr receivingVolt,
                    typename Attribute<MatrixVar<VarType>>::Ptr receivingCur);

  Real delay() const { return mDelay; }
  UInt bufferSize() const { return mBufSize; }
};
} // namespace Base
} // namespace CPS
