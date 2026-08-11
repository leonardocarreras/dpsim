// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <DPsim.h>

using namespace DPsim;
using namespace CPS::EMT;

class Example_VS_RLBranch_RLCLoad {
public:
  Example_VS_RLBranch_RLCLoad()
      : mTimeStep(0.0001), mFinalTime(0.2), mFrequency(50.0),
        mSourceVoltage(CPS::Math::polar(1.0, 0.0)),
        mBranchResistance(CPS::Math::singlePhaseParameterToThreePhase(0.2)),
        mBranchInductance(CPS::Math::singlePhaseParameterToThreePhase(0.02)),
        mLoadResistance(CPS::Math::singlePhaseParameterToThreePhase(1.0)),
        mLoadInductance(CPS::Math::singlePhaseParameterToThreePhase(0.05)),
        mLoadCapacitance(CPS::Math::singlePhaseParameterToThreePhase(100e-6)) {}

  void runExampleWithMNAComponents() const {
    String simName = "EMT_Ph3_MNAComponents";

    // Nodes
    auto n1 = SimNode::make("n1", PhaseType::ABC);
    auto n2 = SimNode::make("n2", PhaseType::ABC);
    auto n3 = SimNode::make("n3", PhaseType::ABC);
    auto n4 = SimNode::make("n4", PhaseType::ABC);
    auto n5 = SimNode::make("n5", PhaseType::ABC);

    // Components
    auto vs = Ph3::VoltageSource::make("VS");
    vs->setParameters(
        CPS::Math::singlePhaseVariableToThreePhase(mSourceVoltage), mFrequency);

    auto rBranch = Ph3::Resistor::make("RBranch");
    rBranch->setParameters(mBranchResistance);

    auto lBranch = Ph3::Inductor::make("LBranch");
    lBranch->setParameters(mBranchInductance);

    auto rLoad = Ph3::Resistor::make("RLoad");
    rLoad->setParameters(mLoadResistance);

    auto lLoad = Ph3::Inductor::make("LLoad");
    lLoad->setParameters(mLoadInductance);

    auto cLoad = Ph3::Capacitor::make("CLoad");
    cLoad->setParameters(mLoadCapacitance);

    // Topology
    vs->connect(SimNode::List{SimNode::GND, n1});
    rBranch->connect(SimNode::List{n1, n2});
    lBranch->connect(SimNode::List{n2, n3});

    rLoad->connect(SimNode::List{n3, n4});
    lLoad->connect(SimNode::List{n4, n5});
    cLoad->connect(SimNode::List{n5, SimNode::GND});

    auto sys = SystemTopology(
        mFrequency, SystemNodeList{n1, n2, n3, n4, n5},
        SystemComponentList{vs, rBranch, lBranch, rLoad, lLoad, cLoad});

    // Logging
    Logger::setLogDir("logs/" + simName);
    auto logger = DataLogger::make(simName);
    logger->logAttribute("v3", n3->attribute("v"));
    logger->logAttribute("iBranch", lBranch->attribute("i_intf"));
    logger->logAttribute("iLoad", rLoad->attribute("i_intf"));

    // Simulation
    Simulation sim(simName, Logger::Level::info);
    sim.setSystem(sys);
    sim.addLogger(logger);
    sim.setDomain(Domain::EMT);
    sim.setSolverType(Solver::Type::MNA);
    sim.setTimeStep(mTimeStep);
    sim.setFinalTime(mFinalTime);
    sim.run();
  }

  void runExampleWithRLCLoadSSN() const {
    String simName = "EMT_Ph3_RLCLoadSSN";

    // Nodes
    auto n1 = SimNode::make("n1", PhaseType::ABC);
    auto n2 = SimNode::make("n2", PhaseType::ABC);
    auto n3 = SimNode::make("n3", PhaseType::ABC);
    // n4, n5 do not exist because of SSN

    Matrix aMatrix;
    Matrix bMatrix;
    Matrix cMatrix;
    Matrix dMatrix;
    createStateSpaceMatricesForRLCLoad(aMatrix, bMatrix, cMatrix, dMatrix);

    // Components
    auto vs = Ph3::VoltageSource::make("VS");
    vs->setParameters(
        CPS::Math::singlePhaseVariableToThreePhase(mSourceVoltage), mFrequency);

    auto rBranch = Ph3::Resistor::make("RBranch");
    rBranch->setParameters(mBranchResistance);

    auto lBranch = Ph3::Inductor::make("LBranch");
    lBranch->setParameters(mBranchInductance);

    auto rlcLoad =
        Ph3::GenericTwoTerminalVTypeSSN::make("RLCLoad", Logger::Level::debug);
    rlcLoad->setParameters(aMatrix, bMatrix, cMatrix, dMatrix);

    // Topology
    vs->connect(SimNode::List{SimNode::GND, n1});
    rBranch->connect(SimNode::List{n1, n2});
    lBranch->connect(SimNode::List{n2, n3});

    // Same load orientation as in the previous one-port examples
    rlcLoad->connect(SimNode::List{n3, SimNode::GND});

    auto sys =
        SystemTopology(mFrequency, SystemNodeList{n1, n2, n3},
                       SystemComponentList{vs, rBranch, lBranch, rlcLoad});

    // Logging
    Logger::setLogDir("logs/" + simName);
    auto logger = DataLogger::make(simName);
    logger->logAttribute("v3", n3->attribute("v"));
    logger->logAttribute("iBranch", lBranch->attribute("i_intf"));
    logger->logAttribute("iLoad", rlcLoad->attribute("i_intf"));

    // Simulation
    Simulation sim(simName, Logger::Level::info);
    sim.setSystem(sys);
    sim.addLogger(logger);
    sim.setDomain(Domain::EMT);
    sim.setSolverType(Solver::Type::MNA);
    sim.setTimeStep(mTimeStep);
    sim.setFinalTime(mFinalTime);
    sim.run();
  }

  void runExampleWithRLBranchSSN() const {
    String simName = "EMT_Ph3_RLBranchSSN";

    // Nodes
    auto n1 = SimNode::make("n1", PhaseType::ABC);
    // n2 does not exist because of SSN
    auto n3 = SimNode::make("n3", PhaseType::ABC);
    auto n4 = SimNode::make("n4", PhaseType::ABC);
    auto n5 = SimNode::make("n5", PhaseType::ABC);

    Matrix aMatrix;
    Matrix bMatrix;
    Matrix cMatrix;
    Matrix dMatrix;
    createStateSpaceMatricesForRLBranch(aMatrix, bMatrix, cMatrix, dMatrix);

    // Components
    auto vs = Ph3::VoltageSource::make("VS");
    vs->setParameters(
        CPS::Math::singlePhaseVariableToThreePhase(mSourceVoltage), mFrequency);

    auto rlBranch =
        Ph3::GenericTwoTerminalVTypeSSN::make("RLBranch", Logger::Level::debug);
    rlBranch->setParameters(aMatrix, bMatrix, cMatrix, dMatrix);

    auto rLoad = Ph3::Resistor::make("RLoad");
    rLoad->setParameters(mLoadResistance);

    auto lLoad = Ph3::Inductor::make("LLoad");
    lLoad->setParameters(mLoadInductance);

    auto cLoad = Ph3::Capacitor::make("CLoad");
    cLoad->setParameters(mLoadCapacitance);

    // Topology
    vs->connect(SimNode::List{SimNode::GND, n1});
    rlBranch->connect(SimNode::List{n1, n3});

    rLoad->connect(SimNode::List{n3, n4});
    lLoad->connect(SimNode::List{n4, n5});
    cLoad->connect(SimNode::List{n5, SimNode::GND});

    auto sys =
        SystemTopology(mFrequency, SystemNodeList{n1, n3, n4, n5},
                       SystemComponentList{vs, rlBranch, rLoad, lLoad, cLoad});

    // Logging
    Logger::setLogDir("logs/" + simName);
    auto logger = DataLogger::make(simName);
    logger->logAttribute("v3", n3->attribute("v"));
    logger->logAttribute("iBranch", rlBranch->attribute("i_intf"));
    logger->logAttribute("iLoad", rLoad->attribute("i_intf"));

    // Simulation
    Simulation sim(simName, Logger::Level::info);
    sim.setSystem(sys);
    sim.addLogger(logger);
    sim.setDomain(Domain::EMT);
    sim.setSolverType(Solver::Type::MNA);
    sim.setTimeStep(mTimeStep);
    sim.setFinalTime(mFinalTime);
    sim.run();
  }

private:
  void createStateSpaceMatricesForRLCLoad(Matrix &aMatrix, Matrix &bMatrix,
                                          Matrix &cMatrix,
                                          Matrix &dMatrix) const {
    Matrix inverseInductance = mLoadInductance.inverse();
    Matrix inverseCapacitance = mLoadCapacitance.inverse();

    // State choice:
    // x = [uC_abc; i_abc]  -> 6x1
    // u = v_abc            -> 3x1
    // y = i_abc            -> 3x1
    aMatrix = Matrix::Zero(6, 6);
    aMatrix.block(0, 3, 3, 3) = inverseCapacitance;
    aMatrix.block(3, 0, 3, 3) = -inverseInductance;
    aMatrix.block(3, 3, 3, 3) = -inverseInductance * mLoadResistance;

    bMatrix = Matrix::Zero(6, 3);
    bMatrix.block(3, 0, 3, 3) = inverseInductance;

    cMatrix = Matrix::Zero(3, 6);
    cMatrix.block(0, 3, 3, 3) = Matrix::Identity(3, 3);

    dMatrix = Matrix::Zero(3, 3);
  }

  void createStateSpaceMatricesForRLBranch(Matrix &aMatrix, Matrix &bMatrix,
                                           Matrix &cMatrix,
                                           Matrix &dMatrix) const {
    Matrix inverseInductance = mBranchInductance.inverse();
    Matrix identity3 = Matrix::Identity(3, 3);

    // State choice:
    // x = i_abc            -> 3x1
    // u = v_abc            -> 3x1
    // y = i_abc            -> 3x1
    //
    // RL branch:
    //   L di/dt = u - R i
    // => di/dt = -L^{-1} R i + L^{-1} u
    //    y = i
    aMatrix = -inverseInductance * mBranchResistance;
    bMatrix = inverseInductance;
    cMatrix = identity3;
    dMatrix = Matrix::Zero(3, 3);
  }

private:
  Real mTimeStep;
  Real mFinalTime;
  Real mFrequency;
  Complex mSourceVoltage;

  Matrix mBranchResistance;
  Matrix mBranchInductance;

  Matrix mLoadResistance;
  Matrix mLoadInductance;
  Matrix mLoadCapacitance;
};

int main(int argc, char *argv[]) {
  Example_VS_RLBranch_RLCLoad example;

  example.runExampleWithMNAComponents();
  example.runExampleWithRLCLoadSSN();
  example.runExampleWithRLBranchSSN();

  return 0;
}
