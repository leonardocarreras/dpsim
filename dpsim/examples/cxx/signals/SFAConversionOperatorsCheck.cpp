// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <vector>

#include <dpsim-models/Signal/SFAConversionOperators.h>

using namespace CPS;
using namespace CPS::Signal;

static const Real F0 = 50.;
static const Real OMEGA0 = 2. * PI * F0;
static const Real DT = 50.e-6;
static const Real DURATION = 0.5;
static const Real EXACT = 1e-12;
static const Real STEP_TIME = 0.2;
static const Real K_NEG = 0.2;
static const Real K_ZERO = 0.15;
static const Real DF_OFF = 0.5;
static const Real REL_TIMEBASE_ERROR = 1e-3;

static Int gFailures = 0;

static void report(const String &name, Real value, const String &relation,
                   Real limit, bool passed) {
  std::cout << (passed ? "  PASS  " : "  FAIL  ") << std::left << std::setw(62)
            << name << std::right << std::scientific << std::setprecision(3)
            << value << " " << relation << " " << limit << std::endl;
  if (!passed)
    ++gFailures;
}

static void expectBelow(const String &name, Real value, Real limit) {
  report(name, value, "<", limit, value < limit);
}

static void expectAbove(const String &name, Real value, Real limit) {
  report(name, value, ">", limit, value > limit);
}

static void expectThrow(const String &name, const std::function<void()> &call) {
  bool threw = false;
  try {
    call();
  } catch (const std::exception &) {
    threw = true;
  }
  std::cout << (threw ? "  PASS  " : "  FAIL  ") << std::left << std::setw(62)
            << name << "throws" << std::endl;
  if (!threw)
    ++gFailures;
}

static MatrixComp balancedSet(Complex amplitude) {
  return SFAConversionOperators::positiveSequenceToPhases(amplitude);
}

static MatrixComp negativeSet(Complex amplitude) {
  MatrixComp set = MatrixComp::Zero(3, 1);
  set << amplitude, amplitude * SHIFT_TO_PHASE_C, amplitude * SHIFT_TO_PHASE_B;
  return set;
}

static MatrixComp zeroSet(Complex amplitude) {
  MatrixComp set = MatrixComp::Zero(3, 1);
  set << amplitude, amplitude, amplitude;
  return set;
}

struct HarmonicComponent {
  Int order;
  MatrixComp env;
};

typedef std::function<std::vector<HarmonicComponent>(Real)> SignalClass;

struct InstantaneousSample {
  Matrix vAbc;
  Matrix quadAbc;
  MatrixComp envFundamental;
};

static InstantaneousSample sampleAt(const SignalClass &signal, Real t,
                                    Real omega0) {
  InstantaneousSample sample;
  sample.vAbc = Matrix::Zero(3, 1);
  sample.quadAbc = Matrix::Zero(3, 1);
  sample.envFundamental = MatrixComp::Zero(3, 1);

  for (const auto &component : signal(t)) {
    const Complex rotation = std::polar(
        1.,
        std::fmod(static_cast<Real>(component.order) * omega0 * t, 2. * PI));
    for (Matrix::Index phase = 0; phase < 3; ++phase) {
      const Complex value = component.env(phase, 0) * rotation;
      sample.vAbc(phase, 0) += value.real();
      sample.quadAbc(phase, 0) += value.imag();
      if (component.order == 1)
        sample.envFundamental(phase, 0) += component.env(phase, 0);
    }
  }
  return sample;
}

static SignalClass balancedSteady() {
  return [](Real) {
    return std::vector<HarmonicComponent>{{1, balancedSet(Complex(1., 0.))}};
  };
}

static SignalClass balancedStep() {
  return [](Real t) {
    return std::vector<HarmonicComponent>{
        {1, balancedSet(Complex(t < STEP_TIME ? 1. : 0.7, 0.))}};
  };
}

static SignalClass unbalancedSteady(Real kNegative, Real kZero) {
  return [kNegative, kZero](Real) {
    return std::vector<HarmonicComponent>{
        {1, MatrixComp(balancedSet(Complex(1., 0.)) +
                       negativeSet(Complex(kNegative, 0.)) +
                       zeroSet(Complex(kZero, 0.)))}};
  };
}

static SignalClass offNominal() {
  return [](Real t) {
    return std::vector<HarmonicComponent>{
        {1, balancedSet(std::polar(1., 2. * PI * DF_OFF * t))}};
  };
}

static SignalClass harmonics() {
  return [](Real) {
    return std::vector<HarmonicComponent>{{1, balancedSet(Complex(1., 0.))},
                                          {5, negativeSet(Complex(0.05, 0.))},
                                          {7, balancedSet(Complex(0.03, 0.))}};
  };
}

static UInt sampleCount(Real duration) {
  return static_cast<UInt>(duration / DT);
}

static void checkScaleIdentity() {
  const SFAConversionOperators spaceVector(
      SFAConversionOperators::Arm::SpaceVector);
  const SFAConversionOperators companion(
      SFAConversionOperators::Arm::AnalyticCompanion);
  const SignalClass unit = balancedSteady();

  Real armAError = 0.;
  Real armDError = 0.;
  Real helperError = 0.;
  Real roundTripError = 0.;

  for (UInt n = 0; n < sampleCount(DURATION); ++n) {
    const Real t = static_cast<Real>(n) * DT;
    const InstantaneousSample sample = sampleAt(unit, t, OMEGA0);

    armAError = std::max(
        armAError, std::abs(std::abs(spaceVector.demodulatePositiveSequence(
                                sample.vAbc, t, OMEGA0)) -
                            1.));

    const MatrixComp perPhase =
        companion.demodulatePerPhase(sample.vAbc, sample.quadAbc, t, OMEGA0);
    armDError = std::max(
        armDError,
        std::abs(std::abs(SFAConversionOperators::phasesToPositiveSequence(
                     perPhase)) -
                 1.));

    helperError = std::max(
        helperError, SFAConversionOperators::scaleIdentityResidual(t, OMEGA0));

    const Matrix reconstructed =
        SFAConversionOperators::remodulate(perPhase, t, OMEGA0);
    roundTripError = std::max(
        roundTripError, (reconstructed - sample.vAbc).cwiseAbs().maxCoeff());
  }

  expectBelow("scale identity, space vector", armAError, EXACT);
  expectBelow("scale identity, analytic companion", armDError, EXACT);
  expectBelow("scale identity, library assertion helper", helperError, EXACT);
  expectBelow("remodulate is the left inverse of demodulate", roundTripError,
              EXACT);
}

static void checkCarrierPhase() {
  const SFAConversionOperators spaceVector(
      SFAConversionOperators::Arm::SpaceVector);
  const SignalClass unit = balancedSteady();

  Real solverTimeError = 0.;
  Real stepCounterError = 0.;

  for (UInt n = 0; n < sampleCount(0.4); ++n) {
    const Real t = static_cast<Real>(n) * DT;
    const Real tCounter = static_cast<Real>(n) * DT * (1. + REL_TIMEBASE_ERROR);
    const InstantaneousSample sample = sampleAt(unit, t, OMEGA0);

    const Complex fromSolverTime =
        spaceVector.demodulatePositiveSequence(sample.vAbc, t, OMEGA0);
    const Complex fromStepCounter =
        spaceVector.demodulatePositiveSequence(sample.vAbc, tCounter, OMEGA0);

    solverTimeError =
        std::max(solverTimeError, std::abs(fromSolverTime - Complex(1., 0.)));
    stepCounterError =
        std::max(stepCounterError, std::abs(fromStepCounter - fromSolverTime));
  }

  expectBelow("carrier from solver time", solverTimeError, EXACT);
  expectAbove("carrier from a step counter, 0.1 percent timebase error",
              stepCounterError, 0.1);
}

static void checkSpaceVectorOnSignalSet() {
  const SFAConversionOperators spaceVector(
      SFAConversionOperators::Arm::SpaceVector);
  const std::vector<std::pair<String, SignalClass>> balanced = {
      {"balanced steady", balancedSteady()},
      {"balanced step", balancedStep()},
      {"off nominal", offNominal()}};

  for (const auto &named : balanced) {
    Real error = 0.;
    for (UInt n = 0; n < sampleCount(DURATION); ++n) {
      const Real t = static_cast<Real>(n) * DT;
      const InstantaneousSample sample = sampleAt(named.second, t, OMEGA0);
      const Complex expected = SFAConversionOperators::phasesToPositiveSequence(
          sample.envFundamental);
      error = std::max(error, std::abs(spaceVector.demodulatePositiveSequence(
                                           sample.vAbc, t, OMEGA0) -
                                       expected));
    }
    expectBelow("space vector envelope, " + named.first, error, EXACT);
  }
}

static Real spaceVectorAbcRoundTrip(Real kNegative, Real kZero) {
  const SFAConversionOperators spaceVector(
      SFAConversionOperators::Arm::SpaceVector);
  const SignalClass signal = unbalancedSteady(kNegative, kZero);

  Real error = 0.;
  for (UInt n = 0; n < sampleCount(DURATION); ++n) {
    const Real t = static_cast<Real>(n) * DT;
    const InstantaneousSample sample = sampleAt(signal, t, OMEGA0);
    const Complex pos =
        spaceVector.demodulatePositiveSequence(sample.vAbc, t, OMEGA0);
    const Matrix reconstructed = SFAConversionOperators::remodulate(
        SFAConversionOperators::positiveSequenceToPhases(pos), t, OMEGA0);
    error =
        std::max(error, (reconstructed - sample.vAbc).cwiseAbs().maxCoeff());
  }
  return error;
}

static void checkSequenceTransport() {
  expectBelow("abc round trip carries the negative sequence",
              spaceVectorAbcRoundTrip(K_NEG, 0.), EXACT);
  expectAbove("abc round trip drops the zero sequence",
              spaceVectorAbcRoundTrip(K_NEG, K_ZERO), 0.1);
}

static void checkAnalyticCompanionOnSignalSet() {
  const SFAConversionOperators companion(
      SFAConversionOperators::Arm::AnalyticCompanion);
  const std::vector<std::pair<String, SignalClass>> fundamentalOnly = {
      {"balanced steady", balancedSteady()},
      {"balanced step", balancedStep()},
      {"unbalanced steady", unbalancedSteady(K_NEG, K_ZERO)},
      {"off nominal", offNominal()}};

  for (const auto &named : fundamentalOnly) {
    Real error = 0.;
    for (UInt n = 0; n < sampleCount(DURATION); ++n) {
      const Real t = static_cast<Real>(n) * DT;
      const InstantaneousSample sample = sampleAt(named.second, t, OMEGA0);
      const MatrixComp perPhase =
          companion.demodulatePerPhase(sample.vAbc, sample.quadAbc, t, OMEGA0);
      error = std::max(
          error, (perPhase - sample.envFundamental).cwiseAbs().maxCoeff());
    }
    expectBelow("analytic companion envelope, " + named.first, error, EXACT);
  }
}

static void checkAdapters() {
  Real inverseError = 0.;
  Real negativeLeak = 0.;
  for (UInt n = 0; n < 360; ++n) {
    const Complex pos = std::polar(1. + 0.01 * static_cast<Real>(n),
                                   static_cast<Real>(n) * PI / 180.);
    inverseError = std::max(
        inverseError,
        std::abs(SFAConversionOperators::phasesToPositiveSequence(
                     SFAConversionOperators::positiveSequenceToPhases(pos)) -
                 pos));
    negativeLeak = std::max(
        negativeLeak, std::abs(SFAConversionOperators::phasesToPositiveSequence(
                          negativeSet(pos))));
  }
  expectBelow("adapter round trip", inverseError, EXACT);
  expectBelow("negative sequence does not leak into the positive channel",
              negativeLeak, EXACT);

  const MatrixComp unbalanced = balancedSet(Complex(1., 0.)) +
                                negativeSet(Complex(K_NEG, 0.)) +
                                zeroSet(Complex(K_ZERO, 0.));
  const Complex pos =
      SFAConversionOperators::phasesToPositiveSequence(unbalanced);
  expectAbove(
      "a Ph1 boundary discards the negative and zero sequence",
      (SFAConversionOperators::positiveSequenceToPhases(pos) - unbalanced)
          .cwiseAbs()
          .maxCoeff(),
      0.1);
}

static Real windowedDftError(Real nominalFrequency, UInt windowSamples) {
  const Real omega0 = 2. * PI * nominalFrequency;
  const SignalClass signal = harmonics();
  PhaseRingBuffer buffer(windowSamples);

  Real error = 0.;
  for (UInt n = 0; n < sampleCount(DURATION); ++n) {
    const Real t = static_cast<Real>(n) * DT;
    const InstantaneousSample sample = sampleAt(signal, t, omega0);
    buffer.push(sample.vAbc(0, 0));
    if (buffer.filled() < windowSamples)
      continue;
    error =
        std::max(error, std::abs(SFAConversionOperators::demodulateWindowedDft(
                                     buffer, t, omega0, windowSamples) -
                                 sample.envFundamental(0, 0)));
  }
  return error;
}

static void checkWindowedDft() {
  const UInt window = SFAConversionOperators::integerWindowSamples(DT, F0);
  report("integer window at 50 Hz on a 50 us grid", static_cast<Real>(window),
         "==", 400., window == 400);

  expectThrow("integer window rejects 60 Hz on a 50 us grid",
              []() { SFAConversionOperators::integerWindowSamples(DT, 60.); });

  expectBelow("windowed DFT nulls the 5th and 7th harmonic",
              windowedDftError(F0, window), EXACT);
  expectAbove("windowed DFT nulls collapse on a rounded window",
              windowedDftError(60., 333), 1e-4);
}

static void checkArmSelection() {
  const SFAConversionOperators spaceVector(
      SFAConversionOperators::Arm::SpaceVector);
  const SFAConversionOperators companion(
      SFAConversionOperators::Arm::AnalyticCompanion);
  const Matrix zero = Matrix::Zero(3, 1);

  expectThrow("space vector arm refuses the per-phase demodulator", [&]() {
    companion.demodulatePositiveSequence(zero, 0., OMEGA0);
  });
  expectThrow(
      "analytic companion arm refuses the space-vector demodulator",
      [&]() { spaceVector.demodulatePerPhase(zero, zero, 0., OMEGA0); });
  expectThrow("three-phase shape is enforced", [&]() {
    spaceVector.demodulatePositiveSequence(Matrix::Zero(2, 1), 0., OMEGA0);
  });
}

int main(int argc, char *argv[]) {
  checkScaleIdentity();
  checkCarrierPhase();
  checkSpaceVectorOnSignalSet();
  checkSequenceTransport();
  checkAnalyticCompanionOnSignalSet();
  checkAdapters();
  checkWindowedDft();
  checkArmSelection();

  std::cout << std::endl
            << (gFailures == 0 ? "all checks passed" : "checks failed: ")
            << (gFailures == 0 ? "" : std::to_string(gFailures)) << std::endl;
  return gFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
