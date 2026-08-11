// SPDX-FileCopyrightText: 2017-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim/Scheduler.h>

#include <vector>

namespace DPsim {
class OpenMPLevelScheduler : public Scheduler {
public:
  OpenMPLevelScheduler(Int threads = -1, String outMeasurementFile = String());
  void createSchedule(const CPS::Task::List &tasks, const Edges &inEdges,
                      const Edges &outEdges);
  void step(Real time, Int timeStepCount);
  void stop();

private:
  Int mNumThreads;
  String mOutMeasurementFile;
  std::vector<CPS::Task::List> mLevels;
};
}; // namespace DPsim
