// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <dpsim-models/Base/Base_HalfDecouplingLine.h>
#include <dpsim-models/DP/DP_Ph1_CurrentSource.h>
#include <dpsim-models/DP/DP_Ph1_Resistor.h>
#include <dpsim-models/Definitions.h>
#include <dpsim-models/Solver/MNAInterface.h>

namespace CPS {
namespace DP {
namespace Ph1 {
/// One end of a Bergeron travelling-wave line, carrying the envelope of the
/// travelling quantities, so that two of these replace a single
/// DP::Ph1::DecouplingLine and can live in different systems or simulators.

class HalfDecouplingLine : public Base::HalfDecouplingLine<Complex>,
                           public SharedFactory<HalfDecouplingLine> {
protected:
  /// Current source carrying the history term
  std::shared_ptr<DP::Ph1::CurrentSource> mSubCtrledCurrentSource;
  /// Terminating impedance calculated from line parameters
  std::shared_ptr<DP::Ph1::Resistor> mSubRes;

  void applySourceCurrent() override;
  MatrixComp historyVoltage() override;
  MatrixComp historyCurrent() override;

public:
  typedef std::shared_ptr<HalfDecouplingLine> Ptr;

  /// Defines UID, name and logging level
  HalfDecouplingLine(String name, Logger::Level logLevel = Logger::Level::off)
      : HalfDecouplingLine(name, name, logLevel) {}
  HalfDecouplingLine(String uid, String name,
                     Logger::Level logLevel = Logger::Level::off);

  void setParameters(Real resistance, Real inductance, Real capacitance);

  // #### General ####
  void createSubComponents() override;
  void initializeParentFromNodesAndTerminals(Real frequency) override;

  // #### MNA section ####
  void mnaParentInitialize(Real omega, Real timeStep,
                           Attribute<Matrix>::Ptr leftVector) override;
  /// Updates internal current variable of the component
  void mnaCompUpdateCurrent(const Matrix &leftVector) override;
  /// Updates internal voltage variable of the component
  void mnaCompUpdateVoltage(const Matrix &leftVector) override;
  /// MNA pre step operations
  void mnaParentPreStep(Real time, Int timeStepCount) override;
  /// MNA post step operations
  void mnaParentPostStep(Real time, Int timeStepCount,
                         Attribute<Matrix>::Ptr &leftVector) override;
  /// Add MNA pre step dependencies
  void mnaParentAddPreStepDependencies(
      AttributeBase::List &prevStepDependencies,
      AttributeBase::List &attributeDependencies,
      AttributeBase::List &modifiedAttributes) override;
  /// Add MNA post step dependencies
  void
  mnaParentAddPostStepDependencies(AttributeBase::List &prevStepDependencies,
                                   AttributeBase::List &attributeDependencies,
                                   AttributeBase::List &modifiedAttributes,
                                   Attribute<Matrix>::Ptr &leftVector) override;
};
} // namespace Ph1
} // namespace DP
} // namespace CPS
