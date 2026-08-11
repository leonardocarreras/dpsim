// SPDX-FileCopyrightText: 2022-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/DP/DP_Ph1_ReducedOrderSynchronGeneratorVBR.h>

namespace CPS {
namespace DP {
namespace Ph1 {
/// @brief Voltage-Behind-Reactance (VBR) implementation
/// of 4th order synchronous generator model
class SynchronGenerator4OrderVBR
    : public ReducedOrderSynchronGeneratorVBR,
      public SharedFactory<SynchronGenerator4OrderVBR> {
protected:
  // #### Model specific variables ####
  /// voltage behind transient reactance
  const Attribute<Matrix>::Ptr mEdq_t;
  /// history term of voltage behind the transient reactance
  Matrix mEh_vbr;

public:
  ///
  SynchronGenerator4OrderVBR(const String &uid, const String &name,
                             Logger::Level logLevel = Logger::Level::off);
  ///
  SynchronGenerator4OrderVBR(const String &name,
                             Logger::Level logLevel = Logger::Level::off);

  // #### General Functions ####
  /// Initializes component from power flow data
  void specificInitialization() final;
  ///
  void stepInPerUnit() final;
};
} // namespace Ph1
} // namespace DP
} // namespace CPS
