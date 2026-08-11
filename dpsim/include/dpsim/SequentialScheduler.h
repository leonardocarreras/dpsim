// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim/Scheduler.h>

#include <chrono>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace DPsim {
class SequentialScheduler : public Scheduler {
public:
  SequentialScheduler(String outMeasurementFile = String(),
                      CPS::Logger::Level logLevel = CPS::Logger::Level::info)
      : Scheduler(logLevel), mOutMeasurementFile(outMeasurementFile) {}

  void createSchedule(const CPS::Task::List &tasks, const Edges &inEdges,
                      const Edges &outEdges);
  void step(Real time, Int timeStepCount);
  void stop();

private:
  CPS::Task::List mSchedule;

  std::unordered_map<size_t, std::vector<std::chrono::nanoseconds>>
      mMeasurements;
  std::vector<std::chrono::nanoseconds> mStepMeasurements;
  CPS::String mOutMeasurementFile;
};
} // namespace DPsim
