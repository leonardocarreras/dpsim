// SPDX-FileCopyrightText: 2017-2025 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <signal.h>

#include <chrono>

#include <dpsim/Config.h>
#include <dpsim/Simulation.h>
#include <dpsim/Timer.h>

namespace DPsim {
/// Extending Simulation class by real-time functionality.
class RealTimeSimulation : public Simulation {

protected:
  Timer mTimer;

public:
  RealTimeSimulation(String name, CommandLineArgs &args);
  /// Standard constructor
  RealTimeSimulation(String name,
                     CPS::Logger::Level logLevel = CPS::Logger::Level::info);

  /** Perform the main simulation loop in real time.
   *
   * @param startSynch If true, the simulation waits for the first external value before starting the timing.
   */
  void
  run(const Timer::StartClock::duration &startIn = std::chrono::seconds(1));

  void run(const Timer::StartClock::time_point &startAt);

  void run(Int startIn) { run(std::chrono::seconds(startIn)); }
};
} // namespace DPsim
