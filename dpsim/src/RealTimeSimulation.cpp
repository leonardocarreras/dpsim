/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#include <chrono>
#include <ctime>
#include <dpsim/RealTimeSimulation.h>
#include <iomanip>
#include <spdlog/fmt/chrono.h>

using namespace CPS;
using namespace DPsim;

RealTimeSimulation::RealTimeSimulation(String name, CommandLineArgs &args)
    : Simulation(name, args), mTimer(){};

RealTimeSimulation::RealTimeSimulation(String name, Logger::Level logLevel)
    : Simulation(name, logLevel), mTimer() {

  //addAttribute<Int >("overruns", nullptr, [=](){ return mTimer.overruns(); }, Flags::read);
  //addAttribute<Int >("overruns", nullptr, nullptr, Flags::read);
}

void RealTimeSimulation::run(const Timer::StartClock::duration &startIn) {
  run(Timer::StartClock::now() + startIn);
}

void RealTimeSimulation::run(const Timer::StartClock::time_point &startAt) {
  if (!mInitialized)
    initialize();

  for (auto lg : mLoggers)
    lg->start();

  SPDLOG_LOGGER_INFO(mLog, "Opening interfaces.");

  for (auto intf : mInterfaces)
    intf->open();

  sync();

  SPDLOG_LOGGER_INFO(
      mLog,
      "Starting simulation at {:%Y-%m-%d %H:%M:%S} (delta_T = {} seconds)",
      startAt, startAt - Timer::StartClock::now());

  mTimer.setStartTime(startAt);
  mTimer.setInterval(**mTimeStep);
  mTimer.start();

  // Collect IO-only tasks (interfaces + loggers) for skip mode.
  CPS::Task::List ioTasks;
  for (auto intf : mInterfaces) {
    for (auto t : intf->getTasks())
      ioTasks.push_back(t);
  }
  for (auto lg : mLoggers) {
    ioTasks.push_back(lg->getTask());
  }

  // main loop
  do {
    mTimer.sleep();
    bool dropComputation = false;
    if (mDropEnabled && mDropThreshold > 0.0 && !mStepTimes.empty()) {
      // Compare last measured step time to threshold * dt
      double lastStep = mStepTimes.back();
      double dt = **mTimeStep;
      if (lastStep >= mDropThreshold * dt) {
        dropComputation = true;
      }
    }

    if (dropComputation) {
      // Skip component computations; only run interfaces and loggers and advance time.
      std::chrono::steady_clock::time_point ioStart;
      if (mLogStepTimes)
        ioStart = std::chrono::steady_clock::now();

      mEvents.handleEvents(mTime);
      for (auto &task : ioTasks) {
        task->execute(mTime, mTimeStepCount);
      }

      mTime += **mTimeStep;
      ++mTimeStepCount;

      if (mLogStepTimes) {
        auto ioEnd = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = ioEnd - ioStart;
        mStepTimes.push_back(diff.count());
      }
    } else {
      step();
    }

    if (mTimer.ticks() == 1)
      SPDLOG_LOGGER_INFO(mLog, "Simulation started.");
  } while (mTime < **mFinalTime);

  SPDLOG_LOGGER_INFO(mLog, "Simulation finished.");

  mScheduler->stop();

  for (auto intf : mInterfaces)
    intf->close();

  for (auto lg : mLoggers)
    lg->stop();

  mTimer.stop();
}
