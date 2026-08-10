// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <vector>

#include <dpsim-models/Definitions.h>

namespace CPS {
namespace Signal {

class PhaseRingBuffer {
public:
  explicit PhaseRingBuffer(UInt capacity);

  void push(Real sample);
  Real sampleAgo(UInt stepsAgo) const;
  UInt capacity() const;
  UInt filled() const;

private:
  std::vector<Real> mSamples;
  UInt mNewest;
  UInt mFilled;
};

class SFAConversionOperators {
public:
  enum class Arm { SpaceVector, AnalyticCompanion };

  explicit SFAConversionOperators(Arm arm = Arm::SpaceVector);

  Arm arm() const;

  Complex demodulatePositiveSequence(const Matrix &vAbc, Real t,
                                     Real omega0) const;
  MatrixComp demodulatePerPhase(const Matrix &vAbc, const Matrix &vQuadAbc,
                                Real t, Real omega0) const;

  static Matrix remodulate(const MatrixComp &envAbc, Real t, Real omega0);

  static MatrixComp positiveSequenceToPhases(const Complex &pos);
  static Complex phasesToPositiveSequence(const MatrixComp &envAbc);

  static Complex demodulateWindowedDft(const PhaseRingBuffer &vPhase, Real t,
                                       Real omega0, UInt windowSamples);
  static UInt integerWindowSamples(Real timeStep, Real nominalFrequency);

  static Real scaleIdentityResidual(Real t, Real omega0);

private:
  static Complex carrier(Real t, Real omega0);
  static Complex spaceVector(const Matrix &vAbc);
  static void requireThreePhase(Matrix::Index rows, Matrix::Index cols);

  Arm mArm;
};

} // namespace Signal
} // namespace CPS
