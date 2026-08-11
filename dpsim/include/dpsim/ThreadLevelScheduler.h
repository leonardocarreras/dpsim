// SPDX-FileCopyrightText: 2017-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim/ThreadScheduler.h>

namespace DPsim {
class ThreadLevelScheduler : public ThreadScheduler {
public:
  ThreadLevelScheduler(Int threads = 1, String outMeasurementFile = String(),
                       String inMeasurementFile = String(),
                       Bool useConditionVariables = false,
                       Bool sortTaskTypes = false);

  void createSchedule(const CPS::Task::List &tasks, const Edges &inEdges,
                      const Edges &outEdges);

private:
  void
  scheduleLevel(const CPS::Task::List &tasks,
                const std::unordered_map<String, TaskTime::rep> &measurements,
                const Edges &inEdges);
  void sortTasksByType(CPS::Task::List::iterator begin,
                       CPS::Task::List::iterator end);

  String mInMeasurementFile;
  Bool mSortTaskTypes;
};
}; // namespace DPsim
