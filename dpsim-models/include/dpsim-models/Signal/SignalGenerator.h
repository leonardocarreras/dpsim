// SPDX-FileCopyrightText: 2021-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/SimSignalComp.h>

namespace CPS {
namespace Signal {
/// \brief Model to generate generic signals
///
/// Abstract model to generate different types of signals.
/// Acts as a base class for more specific signal generator classes, such as SineWaveGenerator.
class SignalGenerator : public SimSignalComp {
public:
  typedef std::shared_ptr<SignalGenerator> Ptr;
  typedef std::vector<Ptr> List;

  const CPS::Attribute<Complex>::Ptr mSigOut;
  const CPS::Attribute<Real>::Ptr mFreq;

  SignalGenerator(String uid, String name,
                  Logger::Level logLevel = Logger::Level::off);

  SignalGenerator(String name, Logger::Level logLevel = Logger::Level::off)
      : SignalGenerator(name, name, logLevel) {
    SPDLOG_LOGGER_INFO(mSLog, "Create {} {}", type(), name);
  }

  /// updates current signal
  virtual void step(Real time) = 0;
  /// returns current signal value without updating it
  Complex getSignal();
};
} // namespace Signal
} // namespace CPS
