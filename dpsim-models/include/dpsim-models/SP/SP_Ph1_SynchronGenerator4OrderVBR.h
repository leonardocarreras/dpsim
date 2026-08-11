// SPDX-FileCopyrightText: 2022-2024 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/SP/SP_Ph1_ReducedOrderSynchronGeneratorVBR.h>

namespace CPS {
namespace SP {
namespace Ph1 {
/// @brief Voltage-Behind-Reactance (VBR) implementation
/// of 4th order synchronous generator model
class SynchronGenerator4OrderVBR
    : public ReducedOrderSynchronGeneratorVBR,
      public SharedFactory<SynchronGenerator4OrderVBR> {
public:
  // ### Model specific elements ###
  /// transient voltage
  const Attribute<Matrix>::Ptr mEdq_t;

protected:
  /// history term of VBR
  Matrix mEh_vbr;

public:
  ///
  SynchronGenerator4OrderVBR(const String &uid, const String &name,
                             Logger::Level logLevel = Logger::Level::off);
  ///
  SynchronGenerator4OrderVBR(const String &name,
                             Logger::Level logLevel = Logger::Level::off);

  // #### General Functions ####
  ///
  void specificInitialization() final;
  ///
  void stepInPerUnit() final;
};
} // namespace Ph1
} // namespace SP
} // namespace CPS
