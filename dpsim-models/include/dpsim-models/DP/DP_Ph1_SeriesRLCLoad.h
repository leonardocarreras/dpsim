/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#pragma once

#include <dpsim-models/MNASimPowerComp.h>
#include <dpsim-models/Solver/MNAInterface.h>

namespace CPS {
namespace DP {
namespace Ph1 {
/// \brief Series RLC load with an oscillating conductance setpoint
///
/// The load is discretized with the trapezoidal rule and stamped as an
/// equivalent conductance in parallel with a history current source. The
/// conductance setpoint is modulated by a source frequency so the load can be
/// used to excite a network at a chosen frequency.
class SeriesRLCLoad : public MNASimPowerComp<Complex>,
                      public SharedFactory<SeriesRLCLoad> {
protected:
  /// Series resistance [Ohm]
  Real mResistance;
  /// Series inductance [H]
  Real mInductance;
  /// Series capacitance [F]
  Real mCapacitance;
  /// Rated power of the load [W]
  Real mRatedPower;
  /// Modulation frequency of the conductance setpoint [Hz]
  Real mSrcFreq;
  /// Angular frequency of the conductance modulation [rad/s]
  Real mOmegaSource;
  /// Equivalent conductance of the companion model [S]
  Complex mEquivCond;
  /// History current source of the companion model [A]
  Complex mEquivCurrent;
  /// Current through the branch at the previous time step [A]
  Complex mPrevCurrent;
  /// Voltage across the branch at the previous time step [V]
  Complex mPrevVoltage;
  /// Time step of the simulation [s]
  Real mTimeStep;
  /// Recorded conductance of every step, used by the post-processing helper
  std::vector<Complex> mCondHistory;

public:
  /// Instantaneous conductance, exposed for logging
  const Attribute<Complex>::Ptr mConductance;
  /// Defines UID, name and log level
  SeriesRLCLoad(String uid, String name,
                Logger::Level logLevel = Logger::Level::off);
  /// Defines name and log level
  SeriesRLCLoad(String name, Logger::Level logLevel = Logger::Level::off)
      : SeriesRLCLoad(name, name, logLevel) {}

  // #### General ####
  /// Sets the branch parameters
  void setParameters(Real resistance, Real inductance, Real capacitance,
                     Real ratedPower, Real srcFreq);
  /// Return new instance with the same parameters
  SimPowerComp<Complex>::Ptr clone(String name) override;
  /// Initializes states from power flow data
  void initializeFromNodesAndTerminals(Real frequency) override;

  // #### MNA section ####
  /// Initializes MNA specific variables
  void mnaCompInitialize(Real omega, Real timeStep,
                         Attribute<Matrix>::Ptr leftVector) override;
  /// Stamps the equivalent conductance into the system matrix
  void mnaCompApplySystemMatrixStamp(SparseMatrixRow &systemMatrix) override;
  /// Stamps the history current source into the right side vector
  void mnaCompApplyRightSideVectorStamp(Matrix &rightVector) override;
  /// Updates the conductance setpoint before the solve
  void mnaCompPreStep(Real time, Int timeStepCount) override;
  /// Reads back the branch quantities after the solve
  void mnaCompPostStep(Real time, Int timeStepCount,
                       Attribute<Matrix>::Ptr &leftVector) override;
  /// Declares the attributes the pre step reads and writes
  void mnaCompAddPreStepDependencies(
      AttributeBase::List &prevStepDependencies,
      AttributeBase::List &attributeDependencies,
      AttributeBase::List &modifiedAttributes) override;
  /// Declares the attributes the post step reads and writes
  void
  mnaCompAddPostStepDependencies(AttributeBase::List &prevStepDependencies,
                                 AttributeBase::List &attributeDependencies,
                                 AttributeBase::List &modifiedAttributes,
                                 Attribute<Matrix>::Ptr &leftVector) override;
  /// Updates the branch voltage from the solution vector
  void mnaCompUpdateVoltage(const Matrix &leftVector) override;
  /// Updates the branch current from the companion model
  void mnaCompUpdateCurrent(const Matrix &leftVector);
};
} // namespace Ph1
} // namespace DP
} // namespace CPS
