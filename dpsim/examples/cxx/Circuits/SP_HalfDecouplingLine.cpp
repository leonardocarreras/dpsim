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

struct SpRig {
  std::shared_ptr<Simulation> sim;
  std::shared_ptr<SP::Ph1::ControlledVoltageSource> source;
  SP::SimNode::Ptr node2;
  std::vector<Complex> voltage;
  std::vector<Complex> current;
};

static SpRig makeSpRig(const String &name, Real frequency,
                       SystemComponentList lineComponents, SP::SimNode::Ptr n1,
                       SP::SimNode::Ptr n2, SimNode<Complex>::List extraNodes) {
  SpRig rig;

  rig.source = SP::Ph1::ControlledVoltageSource::make(name + "_vs");
  rig.source->setParameters(Complex(SOURCE_VOLTAGE, 0));
  rig.source->connect({SP::SimNode::GND, n1});

  auto load = SP::Ph1::Resistor::make(name + "_load");
  load->setParameters(LOAD_RESISTANCE);
  load->connect({n2, SP::SimNode::GND});

  SystemNodeList nodes{n1, n2};
  for (auto node : extraNodes)
    nodes.push_back(node);

  SystemComponentList components{rig.source, load};
  for (auto component : lineComponents)
    components.push_back(component);

  auto system = SystemTopology(frequency, nodes, components);

  rig.sim = std::make_shared<Simulation>(name, Logger::Level::off);
  rig.sim->setSystem(system);
  rig.sim->setTimeStep(TIME_STEP);
  rig.sim->setFinalTime(FINAL_TIME);
  rig.sim->setDomain(Domain::SP);
  rig.sim->setSolverType(Solver::Type::MNA);
  rig.sim->doSplitSubnets(true);
  rig.sim->keepAlive(rig.source->attribute("i_intf"));

  rig.node2 = n2;
  return rig;
}

static SpRig halvesRig(const String &tag, Real frequency,
                       Real communicationStep) {
  auto n1 = SP::SimNode::make(tag + "_half_n1");
  auto n2 = SP::SimNode::make(tag + "_half_n2");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(loadEndVoltage(frequency));

  auto halfA =
      SP::Ph1::HalfDecouplingLine::make(tag + "_half_a", Logger::Level::off);
  auto halfB =
      SP::Ph1::HalfDecouplingLine::make(tag + "_half_b", Logger::Level::off);

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

  return makeSpRig(tag + "_SP_HalfDecouplingLine_halves", frequency,
                   SystemComponentList{halfA, halfB}, n1, n2,
                   SimNode<Complex>::List{});
}

static SpRig fullRig(const String &tag, Real frequency) {
  auto n1 = SP::SimNode::make(tag + "_full_n1");
  auto n2 = SP::SimNode::make(tag + "_full_n2");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(loadEndVoltage(frequency));

  auto line =
      SP::Ph1::DecouplingLine::make(tag + "_full_line", Logger::Level::off);
  line->setParameters(RESISTANCE, INDUCTANCE, CAPACITANCE);
  line->connect({n1, n2});

  return makeSpRig(tag + "_SP_HalfDecouplingLine_full", frequency,
                   SystemComponentList{line}, n1, n2, SimNode<Complex>::List{});
}

static SpRig lumpedRig(const String &tag, Real frequency) {
  auto n1 = SP::SimNode::make(tag + "_pi_n1");
  auto n2 = SP::SimNode::make(tag + "_pi_n2");
  auto vn = SP::SimNode::make(tag + "_pi_vn");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(loadEndVoltage(frequency));
  vn->setInitialVoltage(midPointVoltage(frequency));

  auto res = SP::Ph1::Resistor::make(tag + "_pi_res");
  res->setParameters(RESISTANCE);
  res->connect({n1, vn});
  auto ind = SP::Ph1::Inductor::make(tag + "_pi_ind");
  ind->setParameters(INDUCTANCE);
  ind->connect({vn, n2});
  auto cap1 = SP::Ph1::Capacitor::make(tag + "_pi_cap1");
  cap1->setParameters(CAPACITANCE / 2.);
  cap1->connect({n1, SP::SimNode::GND});
  auto cap2 = SP::Ph1::Capacitor::make(tag + "_pi_cap2");
  cap2->setParameters(CAPACITANCE / 2.);
  cap2->connect({n2, SP::SimNode::GND});

  return makeSpRig(tag + "_SP_HalfDecouplingLine_lumped", frequency,
                   SystemComponentList{res, ind, cap1, cap2}, n1, n2,
                   SimNode<Complex>::List{vn});
}

static std::vector<Real> runSp(std::vector<SpRig *> rigs) {
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

static void runSpCase(const String &tag, Real frequency) {
  SpRig halves = halvesRig(tag, frequency, TIME_STEP);
  SpRig blocked = halvesRig(tag + "_blocked", frequency, COMMUNICATION_STEP);
  SpRig full = fullRig(tag, frequency);
  SpRig lumped = lumpedRig(tag, frequency);

  std::vector<Real> times = runSp({&halves, &blocked, &full, &lumped});

  std::cout << std::defaultfloat << "SP, system frequency " << frequency
            << " Hz, electrical length " << 2. * PI * frequency * TRAVEL_TIME
            << " rad" << std::endl;

  reportBelow(tag + " SP steady halves against the full line",
              relativeRmse(halves.voltage, full.voltage, times, SETTLE_TIME,
                           MODULATION_START),
              HALVES_BOUND);
  reportBelow(tag + " SP steady halves current against the full line",
              relativeRmse(halves.current, full.current, times, SETTLE_TIME,
                           MODULATION_START),
              HALVES_BOUND);
  reportBelow(tag + " SP modulated halves against the full line",
              relativeRmse(halves.voltage, full.voltage, times,
                           MODULATION_START, FINAL_TIME),
              HALVES_BOUND);
  reportBelow(tag + " SP modulated halves current against the full line",
              relativeRmse(halves.current, full.current, times,
                           MODULATION_START, FINAL_TIME),
              HALVES_BOUND);
  report(tag + " SP startup halves against the full line",
         relativeRmse(halves.voltage, full.voltage, times, 0., SETTLE_TIME),
         SEED_BOUND);
  report(tag + " SP startup halves current against the full line",
         relativeRmse(halves.current, full.current, times, 0., SETTLE_TIME),
         SEED_BOUND);

  report(tag + " SP steady halves against lumped elements",
         relativeRmse(halves.voltage, lumped.voltage, times, SETTLE_TIME,
                      MODULATION_START),
         LUMPED_BOUND_VOLTAGE);
  report(tag + " SP steady halves current against lumped elements",
         relativeRmse(halves.current, lumped.current, times, SETTLE_TIME,
                      MODULATION_START),
         LUMPED_BOUND_CURRENT);
  report(tag + " SP steady full line against lumped elements",
         relativeRmse(full.voltage, lumped.voltage, times, SETTLE_TIME,
                      MODULATION_START),
         LUMPED_BOUND_VOLTAGE);
  report(tag + " SP steady full line current against lumped elements",
         relativeRmse(full.current, lumped.current, times, SETTLE_TIME,
                      MODULATION_START),
         LUMPED_BOUND_CURRENT);

  reportExact(tag + " SP blocked exchange against every step",
              maxDifference(blocked.voltage, halves.voltage));
  reportExact(tag + " SP blocked exchange current against every step",
              maxDifference(blocked.current, halves.current));
}

int main(int argc, char *argv[]) {
  Logger::setLogDir("logs/SP_HalfDecouplingLine");

  std::cout << "SP::Ph1::HalfDecouplingLine against the full line and against "
            << "lumped references, surge impedance " << SURGE_IMPEDANCE
            << " ohm, travel time " << TRAVEL_TIME << " s" << std::endl;

  for (Real frequency : {50., 60.})
    runSpCase(frequency == 50. ? "f50" : "f60", frequency);

  std::cout << (gFailures == 0 ? "All checks passed" : "Checks failed")
            << std::endl;
  return gFailures == 0 ? 0 : 1;
}
