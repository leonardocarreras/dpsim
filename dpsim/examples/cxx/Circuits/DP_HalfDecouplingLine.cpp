// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

#include <DPsim.h>

using namespace DPsim;
using namespace CPS;

static const Real SURGE_IMPEDANCE = 400.;
static const Real TRAVEL_TIME = 5.e-5;
static const Real RESISTANCE = 1.;
static const Real INDUCTANCE = SURGE_IMPEDANCE * TRAVEL_TIME;
static const Real CAPACITANCE = TRAVEL_TIME / SURGE_IMPEDANCE;
static const Real LOAD_RESISTANCE = 1000.;
static const Real SOURCE_VOLTAGE = 100000.;
static const Real TIME_STEP = 2.e-5;
static const Real COMMUNICATION_STEP = 4.e-5;
static const Real SETTLE_TIME = 0.01;
static const Real MODULATION_START = 0.05;
static const Real FINAL_TIME = 0.25;
static const Real MODULATION_FREQUENCY = 5.;
static const Real MODULATION_DEPTH = 0.2;
static const Real HALVES_BOUND = 1.e-12;
static const Real SEED_BOUND = 1.e-2;
static const Real LUMPED_BOUND_VOLTAGE = 2.e-6;
static const Real LUMPED_BOUND_CURRENT = 1.e-5;
static const Real EMT_BOUND_VOLTAGE = 5.e-6;
static const Real EMT_BOUND_CURRENT = 5.e-5;
static const Real STALE_ATTRIBUTE_FLOOR = 1.e-12;

static Int gFailures = 0;

static Complex seriesImpedance(Real frequency) {
  return Complex(RESISTANCE, 2. * PI * frequency * INDUCTANCE);
}

static Complex loadEndVoltage(Real frequency) {
  Complex admittance = 1. / seriesImpedance(frequency);
  Complex shunt(0., 2. * PI * frequency * CAPACITANCE / 2.);
  return Complex(SOURCE_VOLTAGE, 0) * admittance /
         (admittance + shunt + 1. / LOAD_RESISTANCE);
}

static Complex midPointVoltage(Real frequency) {
  Complex source(SOURCE_VOLTAGE, 0);
  Complex current =
      (source - loadEndVoltage(frequency)) / seriesImpedance(frequency);
  return source - current * RESISTANCE;
}

static Complex nodePhasor(Complex peakPhase) {
  return peakPhase / RMS3PH_TO_PEAK1PH;
}

static MatrixComp balancedPhasor(Complex phasor) {
  MatrixComp value = MatrixComp::Zero(3, 1);
  value(0, 0) = phasor;
  value(1, 0) = phasor * std::polar(1., -2. * PI / 3.);
  value(2, 0) = phasor * std::polar(1., 2. * PI / 3.);
  return value;
}

static MatrixComp balanced(Real magnitude) {
  return balancedPhasor(Complex(magnitude, 0));
}

static Real envelope(Real time) {
  if (time < MODULATION_START)
    return 1.;
  return 1. + MODULATION_DEPTH * sin(2. * PI * MODULATION_FREQUENCY *
                                     (time - MODULATION_START));
}

template <typename T>
static Real relativeRmse(const std::vector<T> &test,
                         const std::vector<T> &reference,
                         const std::vector<Real> &times, Real from, Real to) {
  Real error = 0.;
  Real scale = 0.;
  UInt count = 0;
  for (UInt k = 0; k < times.size(); k++) {
    if (times[k] < from || times[k] >= to)
      continue;
    error += std::norm(test[k] - reference[k]);
    scale += std::norm(reference[k]);
    count++;
  }
  if (count == 0 || scale == 0.)
    return std::numeric_limits<Real>::infinity();
  return sqrt(error / scale);
}

template <typename T>
static Real maxDifference(const std::vector<T> &test,
                          const std::vector<T> &reference) {
  Real worst = 0.;
  for (UInt k = 0; k < test.size(); k++)
    worst = std::max(worst, std::abs(test[k] - reference[k]));
  return worst;
}

static void report(const String &name, Real value, Real limit) {
  bool passed = value > STALE_ATTRIBUTE_FLOOR && value < limit;
  std::cout << (passed ? "  PASS  " : "  FAIL  ") << std::left << std::setw(60)
            << name << std::right << std::scientific << std::setprecision(3)
            << STALE_ATTRIBUTE_FLOOR << " < " << value << " < " << limit
            << std::endl;
  if (!passed)
    ++gFailures;
}

static void reportBelow(const String &name, Real value, Real limit) {
  bool passed = value < limit;
  std::cout << (passed ? "  PASS  " : "  FAIL  ") << std::left << std::setw(60)
            << name << std::right << std::scientific << std::setprecision(3)
            << value << " < " << limit << std::endl;
  if (!passed)
    ++gFailures;
}

static void reportExact(const String &name, Real difference) {
  bool passed = difference == 0.;
  std::cout << (passed ? "  PASS  " : "  FAIL  ") << std::left << std::setw(60)
            << name << std::right << std::scientific << std::setprecision(3)
            << "max difference " << difference << " == 0" << std::endl;
  if (!passed)
    ++gFailures;
}

struct DpRig {
  std::shared_ptr<Simulation> sim;
  std::shared_ptr<DP::Ph1::ControlledVoltageSource> source;
  DP::SimNode::Ptr node2;
  std::vector<Complex> voltage;
  std::vector<Complex> current;
};

static DpRig makeDpRig(const String &name, Real frequency,
                       SystemComponentList lineComponents, DP::SimNode::Ptr n1,
                       DP::SimNode::Ptr n2, SimNode<Complex>::List extraNodes) {
  DpRig rig;

  rig.source = DP::Ph1::ControlledVoltageSource::make(name + "_vs");
  rig.source->setParameters(Complex(SOURCE_VOLTAGE, 0));
  rig.source->connect({DP::SimNode::GND, n1});

  auto load = DP::Ph1::Resistor::make(name + "_load");
  load->setParameters(LOAD_RESISTANCE);
  load->connect({n2, DP::SimNode::GND});

  SystemNodeList nodes{n1, n2};
  for (auto node : extraNodes)
    nodes.push_back(node);

  SystemComponentList components{rig.source, load};
  for (auto component : lineComponents)
    components.push_back(component);

  auto system = SystemTopology(frequency, nodes, components);

  auto logger = DataLogger::make(name, false);
  logger->logAttribute("i1", rig.source->attribute("i_intf"));

  rig.sim = std::make_shared<Simulation>(name, Logger::Level::off);
  rig.sim->setSystem(system);
  rig.sim->setTimeStep(TIME_STEP);
  rig.sim->setFinalTime(FINAL_TIME);
  rig.sim->setDomain(Domain::DP);
  rig.sim->setSolverType(Solver::Type::MNA);
  rig.sim->doSplitSubnets(true);
  rig.sim->addLogger(logger);

  rig.node2 = n2;
  return rig;
}

static DpRig halvesRig(const String &tag, Real frequency,
                       Real communicationStep) {
  auto n1 = DP::SimNode::make(tag + "_half_n1");
  auto n2 = DP::SimNode::make(tag + "_half_n2");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(loadEndVoltage(frequency));

  auto halfA =
      DP::Ph1::HalfDecouplingLine::make(tag + "_half_a", Logger::Level::off);
  auto halfB =
      DP::Ph1::HalfDecouplingLine::make(tag + "_half_b", Logger::Level::off);

  halfA->connect({n1});
  halfA->setParameters(RESISTANCE, INDUCTANCE, CAPACITANCE);
  halfA->setCouplingSource(halfB->mSendingVolt, halfB->mSendingCur);
  halfA->setInitialCouplingSource(halfB->mSendingInitVolt);
  halfA->setCommunicationStep(communicationStep, TIME_STEP);

  halfB->connect({n2});
  halfB->setParameters(RESISTANCE, INDUCTANCE, CAPACITANCE);
  halfB->setCouplingSource(halfA->mSendingVolt, halfA->mSendingCur);
  halfB->setInitialCouplingSource(halfA->mSendingInitVolt);
  halfB->setCommunicationStep(communicationStep, TIME_STEP);

  halfA->publishInitialVoltage();
  halfB->publishInitialVoltage();

  return makeDpRig(tag + "_DP_HalfDecouplingLine_halves", frequency,
                   SystemComponentList{halfA, halfB}, n1, n2,
                   SimNode<Complex>::List{});
}

static DpRig fullRig(const String &tag, Real frequency) {
  auto n1 = DP::SimNode::make(tag + "_full_n1");
  auto n2 = DP::SimNode::make(tag + "_full_n2");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(loadEndVoltage(frequency));

  auto line =
      DP::Ph1::DecouplingLine::make(tag + "_full_line", Logger::Level::off);
  line->setParameters(RESISTANCE, INDUCTANCE, CAPACITANCE);
  line->connect({n1, n2});

  return makeDpRig(tag + "_DP_HalfDecouplingLine_full", frequency,
                   SystemComponentList{line}, n1, n2, SimNode<Complex>::List{});
}

static DpRig lumpedRig(const String &tag, Real frequency) {
  auto n1 = DP::SimNode::make(tag + "_pi_n1");
  auto n2 = DP::SimNode::make(tag + "_pi_n2");
  auto vn = DP::SimNode::make(tag + "_pi_vn");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(loadEndVoltage(frequency));
  vn->setInitialVoltage(midPointVoltage(frequency));

  auto res = DP::Ph1::Resistor::make(tag + "_pi_res");
  res->setParameters(RESISTANCE);
  res->connect({n1, vn});
  auto ind = DP::Ph1::Inductor::make(tag + "_pi_ind");
  ind->setParameters(INDUCTANCE);
  ind->connect({vn, n2});
  auto cap1 = DP::Ph1::Capacitor::make(tag + "_pi_cap1");
  cap1->setParameters(CAPACITANCE / 2.);
  cap1->connect({n1, DP::SimNode::GND});
  auto cap2 = DP::Ph1::Capacitor::make(tag + "_pi_cap2");
  cap2->setParameters(CAPACITANCE / 2.);
  cap2->connect({n2, DP::SimNode::GND});

  return makeDpRig(tag + "_DP_HalfDecouplingLine_lumped", frequency,
                   SystemComponentList{res, ind, cap1, cap2}, n1, n2,
                   SimNode<Complex>::List{vn});
}

static std::vector<Real> runDp(std::vector<DpRig *> rigs) {
  std::vector<Real> times;
  for (auto rig : rigs)
    rig->sim->start();

  Real time = 0.;
  while (time < FINAL_TIME) {
    time = rigs[0]->sim->time();
    for (auto rig : rigs)
      rig->source->mVoltageRef->set(
          Complex(SOURCE_VOLTAGE * envelope(time), 0));
    for (auto rig : rigs)
      rig->sim->next();
    for (auto rig : rigs) {
      rig->voltage.push_back(rig->node2->singleVoltage());
      rig->current.push_back(rig->source->intfCurrent()(0, 0));
    }
    times.push_back(time);
  }

  for (auto rig : rigs)
    rig->sim->stop();
  return times;
}

static void runDpCase(const String &tag, Real frequency) {
  DpRig halves = halvesRig(tag, frequency, TIME_STEP);
  DpRig blocked = halvesRig(tag + "_blocked", frequency, COMMUNICATION_STEP);
  DpRig full = fullRig(tag, frequency);
  DpRig lumped = lumpedRig(tag, frequency);

  std::vector<Real> times = runDp({&halves, &blocked, &full, &lumped});

  std::cout << std::defaultfloat << "DP, system frequency " << frequency
            << " Hz, electrical length " << 2. * PI * frequency * TRAVEL_TIME
            << " rad" << std::endl;

  reportBelow(tag + " DP steady halves against the full line",
              relativeRmse(halves.voltage, full.voltage, times, SETTLE_TIME,
                           MODULATION_START),
              HALVES_BOUND);
  reportBelow(tag + " DP steady halves current against the full line",
              relativeRmse(halves.current, full.current, times, SETTLE_TIME,
                           MODULATION_START),
              HALVES_BOUND);
  reportBelow(tag + " DP modulated halves against the full line",
              relativeRmse(halves.voltage, full.voltage, times,
                           MODULATION_START, FINAL_TIME),
              HALVES_BOUND);
  reportBelow(tag + " DP modulated halves current against the full line",
              relativeRmse(halves.current, full.current, times,
                           MODULATION_START, FINAL_TIME),
              HALVES_BOUND);
  report(tag + " DP startup halves against the full line",
         relativeRmse(halves.voltage, full.voltage, times, 0., SETTLE_TIME),
         SEED_BOUND);
  report(tag + " DP startup halves current against the full line",
         relativeRmse(halves.current, full.current, times, 0., SETTLE_TIME),
         SEED_BOUND);

  report(tag + " DP steady halves against lumped elements",
         relativeRmse(halves.voltage, lumped.voltage, times, SETTLE_TIME,
                      MODULATION_START),
         LUMPED_BOUND_VOLTAGE);
  report(tag + " DP steady halves current against lumped elements",
         relativeRmse(halves.current, lumped.current, times, SETTLE_TIME,
                      MODULATION_START),
         LUMPED_BOUND_CURRENT);
  report(tag + " DP steady full line against lumped elements",
         relativeRmse(full.voltage, lumped.voltage, times, SETTLE_TIME,
                      MODULATION_START),
         LUMPED_BOUND_VOLTAGE);
  report(tag + " DP steady full line current against lumped elements",
         relativeRmse(full.current, lumped.current, times, SETTLE_TIME,
                      MODULATION_START),
         LUMPED_BOUND_CURRENT);

  reportExact(tag + " DP blocked exchange against every step",
              maxDifference(blocked.voltage, halves.voltage));
  reportExact(tag + " DP blocked exchange current against every step",
              maxDifference(blocked.current, halves.current));
}

struct EmtPh1Rig {
  std::shared_ptr<Simulation> sim;
  std::shared_ptr<EMT::Ph1::VoltageSource> source;
  EMT::SimNode::Ptr node2;
  std::vector<Real> voltage;
  std::vector<Real> current;
};

static EmtPh1Rig makeEmtPh1Rig(const String &name, Real frequency,
                               SystemComponentList lineComponents,
                               EMT::SimNode::Ptr n1, EMT::SimNode::Ptr n2,
                               SimNode<Real>::List extraNodes) {
  EmtPh1Rig rig;

  rig.source = EMT::Ph1::VoltageSource::make(name + "_vs");
  rig.source->setParameters(Complex(SOURCE_VOLTAGE, 0), frequency);
  rig.source->connect({EMT::SimNode::GND, n1});

  auto load = EMT::Ph1::Resistor::make(name + "_load");
  load->setParameters(LOAD_RESISTANCE);
  load->connect({n2, EMT::SimNode::GND});

  SystemNodeList nodes{n1, n2};
  for (auto node : extraNodes)
    nodes.push_back(node);

  SystemComponentList components{rig.source, load};
  for (auto component : lineComponents)
    components.push_back(component);

  auto logger = DataLogger::make(name, false);
  logger->logAttribute("i1", rig.source->attribute("i_intf"));

  rig.sim = std::make_shared<Simulation>(name, Logger::Level::off);
  rig.sim->setSystem(SystemTopology(frequency, nodes, components));
  rig.sim->setTimeStep(TIME_STEP);
  rig.sim->setFinalTime(FINAL_TIME);
  rig.sim->setDomain(Domain::EMT);
  rig.sim->setSolverType(Solver::Type::MNA);
  rig.sim->addLogger(logger);

  rig.node2 = n2;
  return rig;
}

static EmtPh1Rig emtPh1DecoupledRig(const String &tag, Real frequency) {
  auto n1 = EMT::SimNode::make(tag + "_emt1_dec_n1");
  auto n2 = EMT::SimNode::make(tag + "_emt1_dec_n2");
  n1->setInitialVoltage(nodePhasor(Complex(SOURCE_VOLTAGE, 0)));
  n2->setInitialVoltage(nodePhasor(loadEndVoltage(frequency)));

  auto line = EMT::Ph1::DecouplingLine::make(tag + "_emt1_dec_line",
                                             Logger::Level::off);
  line->setParameters(RESISTANCE, INDUCTANCE, CAPACITANCE);
  line->connect({n1, n2});

  return makeEmtPh1Rig(tag + "_EMT_Ph1_decoupled", frequency,
                       SystemComponentList{line}, n1, n2,
                       SimNode<Real>::List{});
}

static EmtPh1Rig emtPh1LumpedRig(const String &tag, Real frequency) {
  auto n1 = EMT::SimNode::make(tag + "_emt1_pi_n1");
  auto n2 = EMT::SimNode::make(tag + "_emt1_pi_n2");
  auto vn = EMT::SimNode::make(tag + "_emt1_pi_vn");
  n1->setInitialVoltage(nodePhasor(Complex(SOURCE_VOLTAGE, 0)));
  n2->setInitialVoltage(nodePhasor(loadEndVoltage(frequency)));
  vn->setInitialVoltage(nodePhasor(midPointVoltage(frequency)));

  auto res = EMT::Ph1::Resistor::make(tag + "_emt1_pi_res");
  res->setParameters(RESISTANCE);
  res->connect({n1, vn});
  auto ind = EMT::Ph1::Inductor::make(tag + "_emt1_pi_ind");
  ind->setParameters(INDUCTANCE);
  ind->connect({vn, n2});
  auto cap1 = EMT::Ph1::Capacitor::make(tag + "_emt1_pi_cap1");
  cap1->setParameters(CAPACITANCE / 2.);
  cap1->connect({n1, EMT::SimNode::GND});
  auto cap2 = EMT::Ph1::Capacitor::make(tag + "_emt1_pi_cap2");
  cap2->setParameters(CAPACITANCE / 2.);
  cap2->connect({n2, EMT::SimNode::GND});

  return makeEmtPh1Rig(tag + "_EMT_Ph1_lumped", frequency,
                       SystemComponentList{res, ind, cap1, cap2}, n1, n2,
                       SimNode<Real>::List{vn});
}

static std::vector<Real> runEmtPh1(std::vector<EmtPh1Rig *> rigs) {
  std::vector<Real> times;
  for (auto rig : rigs)
    rig->sim->start();

  Real time = 0.;
  while (time < FINAL_TIME) {
    time = rigs[0]->sim->time();
    for (auto rig : rigs)
      rig->source->mVoltageRef->set(
          Complex(SOURCE_VOLTAGE * envelope(time), 0));
    for (auto rig : rigs)
      rig->sim->next();
    for (auto rig : rigs) {
      rig->voltage.push_back(rig->node2->singleVoltage());
      rig->current.push_back(rig->source->intfCurrent()(0, 0));
    }
    times.push_back(time);
  }

  for (auto rig : rigs)
    rig->sim->stop();
  return times;
}

static void runEmtPh1Case(const String &tag, Real frequency) {
  EmtPh1Rig decoupled = emtPh1DecoupledRig(tag, frequency);
  EmtPh1Rig lumped = emtPh1LumpedRig(tag, frequency);

  std::vector<Real> times = runEmtPh1({&decoupled, &lumped});

  report(tag + " EMT Ph1 steady decoupled against lumped elements",
         relativeRmse(decoupled.voltage, lumped.voltage, times, SETTLE_TIME,
                      MODULATION_START),
         EMT_BOUND_VOLTAGE);
  report(tag + " EMT Ph1 steady decoupled current against lumped elements",
         relativeRmse(decoupled.current, lumped.current, times, SETTLE_TIME,
                      MODULATION_START),
         EMT_BOUND_CURRENT);
}

struct EmtPh3Rig {
  std::shared_ptr<Simulation> sim;
  std::shared_ptr<EMT::Ph3::VoltageSource> source;
  EMT::SimNode::Ptr node2;
  std::vector<Real> voltage;
  std::vector<Real> current;
};

static EmtPh3Rig makeEmtPh3Rig(const String &name, Real frequency,
                               SystemComponentList lineComponents,
                               EMT::SimNode::Ptr n1, EMT::SimNode::Ptr n2,
                               SimNode<Real>::List extraNodes) {
  EmtPh3Rig rig;

  rig.source = EMT::Ph3::VoltageSource::make(name + "_vs");
  rig.source->setParameters(balanced(SOURCE_VOLTAGE), frequency);
  rig.source->connect({EMT::SimNode::GND, n1});

  auto load = EMT::Ph3::Resistor::make(name + "_load");
  load->setParameters(Math::singlePhaseParameterToThreePhase(LOAD_RESISTANCE));
  load->connect({n2, EMT::SimNode::GND});

  SystemNodeList nodes{n1, n2};
  for (auto node : extraNodes)
    nodes.push_back(node);

  SystemComponentList components{rig.source, load};
  for (auto component : lineComponents)
    components.push_back(component);

  auto logger = DataLogger::make(name, false);
  logger->logAttribute("i1", rig.source->attribute("i_intf"));

  rig.sim = std::make_shared<Simulation>(name, Logger::Level::off);
  rig.sim->setSystem(SystemTopology(frequency, nodes, components));
  rig.sim->setTimeStep(TIME_STEP);
  rig.sim->setFinalTime(FINAL_TIME);
  rig.sim->setDomain(Domain::EMT);
  rig.sim->setSolverType(Solver::Type::MNA);
  rig.sim->addLogger(logger);

  rig.node2 = n2;
  return rig;
}

static EmtPh3Rig emtPh3DecoupledRig(const String &tag, Real frequency) {
  auto n1 = EMT::SimNode::make(tag + "_emt3_dec_n1", PhaseType::ABC);
  auto n2 = EMT::SimNode::make(tag + "_emt3_dec_n2", PhaseType::ABC);
  n1->setInitialVoltage(balanced(SOURCE_VOLTAGE));
  n2->setInitialVoltage(balancedPhasor(loadEndVoltage(frequency)));

  auto line = EMT::Ph3::DecouplingLine::make(tag + "_emt3_dec_line",
                                             Logger::Level::off);
  line->setParameters(Math::singlePhaseParameterToThreePhase(RESISTANCE),
                      Math::singlePhaseParameterToThreePhase(INDUCTANCE),
                      Math::singlePhaseParameterToThreePhase(CAPACITANCE));
  line->connect({n1, n2});

  return makeEmtPh3Rig(tag + "_EMT_Ph3_decoupled", frequency,
                       SystemComponentList{line}, n1, n2,
                       SimNode<Real>::List{});
}

static EmtPh3Rig emtPh3LumpedRig(const String &tag, Real frequency) {
  auto n1 = EMT::SimNode::make(tag + "_emt3_pi_n1", PhaseType::ABC);
  auto n2 = EMT::SimNode::make(tag + "_emt3_pi_n2", PhaseType::ABC);
  auto vn = EMT::SimNode::make(tag + "_emt3_pi_vn", PhaseType::ABC);
  n1->setInitialVoltage(balanced(SOURCE_VOLTAGE));
  n2->setInitialVoltage(balancedPhasor(loadEndVoltage(frequency)));
  vn->setInitialVoltage(balancedPhasor(midPointVoltage(frequency)));

  auto res = EMT::Ph3::Resistor::make(tag + "_emt3_pi_res");
  res->setParameters(Math::singlePhaseParameterToThreePhase(RESISTANCE));
  res->connect({n1, vn});
  auto ind = EMT::Ph3::Inductor::make(tag + "_emt3_pi_ind");
  ind->setParameters(Math::singlePhaseParameterToThreePhase(INDUCTANCE));
  ind->connect({vn, n2});
  auto cap1 = EMT::Ph3::Capacitor::make(tag + "_emt3_pi_cap1");
  cap1->setParameters(Math::singlePhaseParameterToThreePhase(CAPACITANCE / 2.));
  cap1->connect({n1, EMT::SimNode::GND});
  auto cap2 = EMT::Ph3::Capacitor::make(tag + "_emt3_pi_cap2");
  cap2->setParameters(Math::singlePhaseParameterToThreePhase(CAPACITANCE / 2.));
  cap2->connect({n2, EMT::SimNode::GND});

  return makeEmtPh3Rig(tag + "_EMT_Ph3_lumped", frequency,
                       SystemComponentList{res, ind, cap1, cap2}, n1, n2,
                       SimNode<Real>::List{vn});
}

static std::vector<Real> runEmtPh3(std::vector<EmtPh3Rig *> rigs) {
  std::vector<Real> times;
  for (auto rig : rigs)
    rig->sim->start();

  Real time = 0.;
  while (time < FINAL_TIME) {
    time = rigs[0]->sim->time();
    for (auto rig : rigs)
      rig->source->mVoltageRef->set(balanced(SOURCE_VOLTAGE * envelope(time)));
    for (auto rig : rigs)
      rig->sim->next();
    for (auto rig : rigs) {
      rig->voltage.push_back(rig->node2->singleVoltage(PhaseType::A));
      rig->current.push_back(rig->source->intfCurrent()(0, 0));
    }
    times.push_back(time);
  }

  for (auto rig : rigs)
    rig->sim->stop();
  return times;
}

static void runEmtPh3Case(const String &tag, Real frequency) {
  EmtPh3Rig decoupled = emtPh3DecoupledRig(tag, frequency);
  EmtPh3Rig lumped = emtPh3LumpedRig(tag, frequency);

  std::vector<Real> times = runEmtPh3({&decoupled, &lumped});

  report(tag + " EMT Ph3 steady decoupled against lumped elements",
         relativeRmse(decoupled.voltage, lumped.voltage, times, SETTLE_TIME,
                      MODULATION_START),
         EMT_BOUND_VOLTAGE);
  report(tag + " EMT Ph3 steady decoupled current against lumped elements",
         relativeRmse(decoupled.current, lumped.current, times, SETTLE_TIME,
                      MODULATION_START),
         EMT_BOUND_CURRENT);
}

int main(int argc, char *argv[]) {
  Logger::setLogDir("logs/DP_HalfDecouplingLine");

  std::cout << "DP::Ph1::HalfDecouplingLine against the full line and against "
            << "lumped references, surge impedance " << SURGE_IMPEDANCE
            << " ohm, travel time " << TRAVEL_TIME << " s" << std::endl;

  for (Real frequency : {50., 60.}) {
    String tag = frequency == 50. ? "f50" : "f60";
    runDpCase(tag, frequency);
    runEmtPh1Case(tag, frequency);
    runEmtPh3Case(tag, frequency);
  }

  std::cout << (gFailures == 0 ? "All checks passed" : "Checks failed")
            << std::endl;
  return gFailures == 0 ? 0 : 1;
}
