/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#pragma once

#include <dpsim/ThreadScheduler.h>

namespace DPsim {
class ThreadLevelScheduler : public ThreadScheduler {
public:
  struct FitReport {
    Bool valid = false;
    Bool fitsExpectedStep = false;
    Real expectedStepTime = 0.0;
    Real predictedStepTime = 0.0;
    Real criticalPathTime = 0.0;
    std::vector<Real> levelMaxTimes;
    std::vector<Real> threadLoads;
    Int numLevels = 0;
    Int numTasks = 0;
  };

  ThreadLevelScheduler(Int threads = 1, String outMeasurementFile = String(),
                       String inMeasurementFile = String(),
                       Bool useConditionVariables = false,
                       Bool sortTaskTypes = false);

  void createSchedule(const CPS::Task::List &tasks, const Edges &inEdges,
                      const Edges &outEdges);

  void setExpectedStepTime(Real timeStep) { mExpectedStepTime = timeStep; }
  void setFitReportEnabled(Bool enabled) { mFitReportEnabled = enabled; }
  const FitReport &getFitReport() const { return mFitReport; }

private:
  std::vector<TaskTime::rep>
  scheduleLevel(const CPS::Task::List &tasks,
                const std::unordered_map<String, TaskTime::rep> &measurements,
                const Edges &inEdges);
  void sortTasksByType(CPS::Task::List::iterator begin,
                       CPS::Task::List::iterator end);
  void buildFitReport(const CPS::Task::List &ordered,
                      const std::vector<CPS::Task::List> &levels,
                      const std::unordered_map<String, TaskTime::rep> &measurements,
                      const std::vector<std::vector<TaskTime::rep>> &levelThreadLoads,
                      const Edges &inEdges);
  static Real measurementToSeconds(TaskTime::rep time);

  String mInMeasurementFile;
  Bool mSortTaskTypes;
  Real mExpectedStepTime = 0.0;
  Bool mFitReportEnabled = true;
  FitReport mFitReport;
};
}; // namespace DPsim
