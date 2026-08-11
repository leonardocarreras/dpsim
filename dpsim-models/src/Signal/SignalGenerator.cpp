// SPDX-FileCopyrightText: 2021-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <dpsim-models/Signal/SignalGenerator.h>

using namespace CPS;

Signal::SignalGenerator::SignalGenerator(String uid, String name,
                                         Logger::Level logLevel)
    : SimSignalComp(name, logLevel),
      mSigOut(mAttributes->create<Complex>("sigOut")),
      mFreq(mAttributes->createDynamic<Real>("freq")) {

  SPDLOG_LOGGER_INFO(mSLog, "Create {} {}", type(), name);
}

Complex Signal::SignalGenerator::getSignal() { return **mSigOut; }
