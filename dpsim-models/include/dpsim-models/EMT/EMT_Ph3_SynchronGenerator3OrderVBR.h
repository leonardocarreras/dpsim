// SPDX-FileCopyrightText: 2017-2021 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <dpsim-models/EMT/EMT_Ph3_ReducedOrderSynchronGeneratorVBR.h>

namespace CPS {
namespace EMT {
namespace Ph3 {
/// @brief Voltage-Behind-Reactance (VBR) implementation
/// of 3rd order synchronous generator model
class SynchronGenerator3OrderVBR
    : public ReducedOrderSynchronGeneratorVBR,
      public SharedFactory<SynchronGenerator3OrderVBR> {

public:
  // ### Model specific elements ###
  /// transient voltage
  const Attribute<Matrix>::Ptr mEdq0_t;

protected:
  /// history term of VBR
  Matrix mEhs_vbr;

public:
  ///
  SynchronGenerator3OrderVBR(const String &uid, const String &name,
                             Logger::Level logLevel = Logger::Level::off);
  ///
  SynchronGenerator3OrderVBR(const String &name,
                             Logger::Level logLevel = Logger::Level::off);

  // #### General Functions ####
  ///
  void specificInitialization() final;
  ///
  void stepInPerUnit() final;
};
} // namespace Ph3
} // namespace EMT
} // namespace CPS
