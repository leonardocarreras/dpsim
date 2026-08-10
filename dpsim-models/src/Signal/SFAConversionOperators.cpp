// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <cmath>
#include <stdexcept>

#include <dpsim-models/MathUtils.h>
#include <dpsim-models/Signal/SFAConversionOperators.h>

using namespace CPS;
using namespace CPS::Signal;

PhaseRingBuffer::PhaseRingBuffer(UInt capacity)
    : mSamples(capacity, 0.), mNewest(0), mFilled(0) {
  if (capacity == 0)
    throw std::invalid_argument(
        "PhaseRingBuffer: capacity must be at least one sample");
}

void PhaseRingBuffer::push(Real sample) {
  mNewest = mFilled == 0 ? 0 : (mNewest + 1) % capacity();
  mSamples[mNewest] = sample;
  if (mFilled < capacity())
    ++mFilled;
}

Real PhaseRingBuffer::sampleAgo(UInt stepsAgo) const {
  if (stepsAgo >= mFilled)
    throw std::invalid_argument(
        "PhaseRingBuffer: requested sample is older than the buffer contents");
  return mSamples[(mNewest + capacity() - stepsAgo) % capacity()];
}

UInt PhaseRingBuffer::capacity() const {
  return static_cast<UInt>(mSamples.size());
}

UInt PhaseRingBuffer::filled() const { return mFilled; }

SFAConversionOperators::SFAConversionOperators(Arm arm) : mArm(arm) {}

SFAConversionOperators::Arm SFAConversionOperators::arm() const { return mArm; }

Complex SFAConversionOperators::carrier(Real t, Real omega0) {
  return std::polar(1., std::fmod(omega0 * t, 2. * PI));
}

Complex SFAConversionOperators::spaceVector(const Matrix &vAbc) {
  return (2. / 3.) * (vAbc(0, 0) + SHIFT_TO_PHASE_C * vAbc(1, 0) +
                      SHIFT_TO_PHASE_B * vAbc(2, 0));
}

void SFAConversionOperators::requireThreePhase(Matrix::Index rows,
                                               Matrix::Index cols) {
  if (rows != 3 || cols != 1)
    throw std::invalid_argument(
        "SFAConversionOperators: expected a 3x1 three-phase quantity");
}

Complex SFAConversionOperators::demodulatePositiveSequence(const Matrix &vAbc,
                                                           Real t,
                                                           Real omega0) const {
  if (mArm != Arm::SpaceVector)
    throw std::invalid_argument("SFAConversionOperators: the space-vector "
                                "demodulator is not the configured arm");
  requireThreePhase(vAbc.rows(), vAbc.cols());
  return spaceVector(vAbc) * std::conj(carrier(t, omega0));
}

MatrixComp SFAConversionOperators::demodulatePerPhase(const Matrix &vAbc,
                                                      const Matrix &vQuadAbc,
                                                      Real t,
                                                      Real omega0) const {
  if (mArm != Arm::AnalyticCompanion)
    throw std::invalid_argument(
        "SFAConversionOperators: the analytic-companion "
        "demodulator is not the configured arm");
  requireThreePhase(vAbc.rows(), vAbc.cols());
  requireThreePhase(vQuadAbc.rows(), vQuadAbc.cols());

  const Complex conjugateCarrier = std::conj(carrier(t, omega0));
  MatrixComp envAbc = MatrixComp::Zero(3, 1);
  for (Matrix::Index phase = 0; phase < 3; ++phase)
    envAbc(phase, 0) =
        Complex(vAbc(phase, 0), vQuadAbc(phase, 0)) * conjugateCarrier;
  return envAbc;
}

Matrix SFAConversionOperators::remodulate(const MatrixComp &envAbc, Real t,
                                          Real omega0) {
  requireThreePhase(envAbc.rows(), envAbc.cols());

  const Complex carrierValue = carrier(t, omega0);
  Matrix vAbc = Matrix::Zero(3, 1);
  for (Matrix::Index phase = 0; phase < 3; ++phase)
    vAbc(phase, 0) = (envAbc(phase, 0) * carrierValue).real();
  return vAbc;
}

MatrixComp
SFAConversionOperators::positiveSequenceToPhases(const Complex &pos) {
  return Math::singlePhaseVariableToThreePhase(pos);
}

Complex
SFAConversionOperators::phasesToPositiveSequence(const MatrixComp &envAbc) {
  requireThreePhase(envAbc.rows(), envAbc.cols());
  return (envAbc(0, 0) + SHIFT_TO_PHASE_C * envAbc(1, 0) +
          SHIFT_TO_PHASE_B * envAbc(2, 0)) /
         3.;
}

UInt SFAConversionOperators::integerWindowSamples(Real timeStep,
                                                  Real nominalFrequency) {
  if (timeStep <= 0. || nominalFrequency <= 0.)
    throw std::invalid_argument("SFAConversionOperators: time step and nominal "
                                "frequency must be positive");

  const Real exact = 1. / (nominalFrequency * timeStep);
  const Real rounded = std::round(exact);
  if (rounded < 1. || std::abs(exact - rounded) > 1e-9 * rounded)
    throw std::invalid_argument(
        "SFAConversionOperators: one fundamental period is not an integer "
        "number of time steps");
  return static_cast<UInt>(rounded);
}

Complex SFAConversionOperators::demodulateWindowedDft(
    const PhaseRingBuffer &vPhase, Real t, Real omega0, UInt windowSamples) {
  if (windowSamples == 0)
    throw std::invalid_argument(
        "SFAConversionOperators: window must span at least one sample");
  if (vPhase.capacity() < windowSamples)
    throw std::invalid_argument(
        "SFAConversionOperators: buffer is shorter than the window");
  if (vPhase.filled() < windowSamples)
    return Complex(0., 0.);

  const Real timeStep = 2. * PI / (omega0 * static_cast<Real>(windowSamples));
  Complex sum(0., 0.);
  for (UInt step = 0; step < windowSamples; ++step)
    sum += vPhase.sampleAgo(step) *
           std::conj(carrier(t - static_cast<Real>(step) * timeStep, omega0));
  return 2. * sum / static_cast<Real>(windowSamples);
}

Real SFAConversionOperators::scaleIdentityResidual(Real t, Real omega0) {
  const Matrix unitBalanced =
      remodulate(positiveSequenceToPhases(Complex(1., 0.)), t, omega0);
  return std::abs(
      std::abs(spaceVector(unitBalanced) * std::conj(carrier(t, omega0))) - 1.);
}
