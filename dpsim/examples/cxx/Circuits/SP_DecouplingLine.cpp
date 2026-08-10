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
static const Real PILINE_CONDUCTANCE = 1.e-12;
static const Real LOAD_RESISTANCE = 1000.;
static const Real SOURCE_VOLTAGE = 100000.;
static const Real TIME_STEP = 2.e-5;
static const Real SETTLE_TIME = 0.01;
static const Real MODULATION_START = 0.05;
static const Real FINAL_TIME = 0.25;
static const Real MODULATION_FREQUENCY = 5.;
static const Real MODULATION_DEPTH = 0.2;
static const Real STEADY_BOUND_VOLTAGE = 2.e-6;
static const Real STEADY_BOUND_CURRENT = 1.e-5;
static const Real MODULATED_BOUND_VOLTAGE = 5.e-4;
static const Real MODULATED_BOUND_CURRENT = 2.e-3;
static const Real STALE_ATTRIBUTE_FLOOR = 1.e-12;

static Int gFailures = 0;

struct Rig {
  std::shared_ptr<Simulation> sim;
  std::shared_ptr<SP::Ph1::ControlledVoltageSource> source;
  SP::SimNode::Ptr node2;
  std::shared_ptr<SP::Ph1::Resistor> load;
  std::vector<Complex> voltage;
  std::vector<Complex> current;
};

static Complex sourceValue(Real time) {
  if (time < MODULATION_START)
    return Complex(SOURCE_VOLTAGE, 0);
  Real envelope = 1. + MODULATION_DEPTH * sin(2. * PI * MODULATION_FREQUENCY *
                                              (time - MODULATION_START));
  return Complex(SOURCE_VOLTAGE * envelope, 0);
}

static Rig makeRig(const String &name, Real frequency,
                   SimPowerComp<Complex>::Ptr line, SP::SimNode::Ptr n1,
                   SP::SimNode::Ptr n2, SimNode<Complex>::List extraNodes,
                   SystemComponentList extraComponents) {
  Rig rig;

  rig.source = SP::Ph1::ControlledVoltageSource::make(name + "_vs");
  rig.source->setParameters(Complex(SOURCE_VOLTAGE, 0));
  rig.source->connect({SP::SimNode::GND, n1});

  rig.load = SP::Ph1::Resistor::make(name + "_load");
  rig.load->setParameters(LOAD_RESISTANCE);
  rig.load->connect({n2, SP::SimNode::GND});

  SystemNodeList nodes{n1, n2};
  for (auto node : extraNodes)
    nodes.push_back(node);

  SystemComponentList components{rig.source, line, rig.load};
  for (auto component : extraComponents)
    components.push_back(component);

  auto system = SystemTopology(frequency, nodes, components);

  auto logger = DataLogger::make(name);
  logger->logAttribute("v2", n2->attribute("v"));
  logger->logAttribute("i1", rig.source->attribute("i_intf"));
  logger->logAttribute("i2", rig.load->attribute("i_intf"));

  rig.sim = std::make_shared<Simulation>(name, Logger::Level::off);
  rig.sim->setSystem(system);
  rig.sim->setTimeStep(TIME_STEP);
  rig.sim->setFinalTime(FINAL_TIME);
  rig.sim->setDomain(Domain::SP);
  rig.sim->setSolverType(Solver::Type::MNA);
  rig.sim->addLogger(logger);

  rig.node2 = n2;
  return rig;
}

static Rig decoupledRig(const String &tag, Real frequency) {
  auto n1 = SP::SimNode::make(tag + "_dec_n1");
  auto n2 = SP::SimNode::make(tag + "_dec_n2");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));

  auto line =
      SP::Ph1::DecouplingLine::make(tag + "_dec_line", Logger::Level::off);
  line->setParameters(RESISTANCE, INDUCTANCE, CAPACITANCE);
  line->connect({n1, n2});

  return makeRig(tag + "_SP_DecouplingLine_decoupled", frequency, line, n1, n2,
                 SimNode<Complex>::List{}, SystemComponentList{});
}

static Rig piLineRig(const String &tag, Real frequency) {
  auto n1 = SP::SimNode::make(tag + "_pi_n1");
  auto n2 = SP::SimNode::make(tag + "_pi_n2");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));

  auto line = SP::Ph1::PiLine::make(tag + "_pi_line", Logger::Level::off);
  line->setParameters(RESISTANCE, INDUCTANCE, CAPACITANCE, PILINE_CONDUCTANCE);
  line->connect({n1, n2});

  return makeRig(tag + "_SP_DecouplingLine_piline", frequency, line, n1, n2,
                 SimNode<Complex>::List{}, SystemComponentList{});
}

static Rig elementsRig(const String &tag, Real frequency) {
  auto n1 = SP::SimNode::make(tag + "_el_n1");
  auto n2 = SP::SimNode::make(tag + "_el_n2");
  auto vn = SP::SimNode::make(tag + "_el_vn");
  n1->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  n2->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));
  vn->setInitialVoltage(Complex(SOURCE_VOLTAGE, 0));

  auto res = SP::Ph1::Resistor::make(tag + "_el_res");
  res->setParameters(RESISTANCE);
  res->connect({n1, vn});
  auto ind = SP::Ph1::Inductor::make(tag + "_el_ind");
  ind->setParameters(INDUCTANCE);
  ind->connect({vn, n2});
  auto cap1 = SP::Ph1::Capacitor::make(tag + "_el_cap1");
  cap1->setParameters(CAPACITANCE / 2.);
  cap1->connect({n1, SP::SimNode::GND});
  auto cap2 = SP::Ph1::Capacitor::make(tag + "_el_cap2");
  cap2->setParameters(CAPACITANCE / 2.);
  cap2->connect({n2, SP::SimNode::GND});

  return makeRig(tag + "_SP_DecouplingLine_elements", frequency, res, n1, n2,
                 SimNode<Complex>::List{vn},
                 SystemComponentList{ind, cap1, cap2});
}

static std::vector<Real> runAll(std::vector<Rig *> rigs) {
  std::vector<Real> times;
  for (auto rig : rigs)
    rig->sim->start();

  Real time = 0.;
  while (time < FINAL_TIME) {
    time = rigs[0]->sim->time();
    for (auto rig : rigs)
      rig->source->mVoltageRef->set(sourceValue(time));
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

static Real relativeRmse(const std::vector<Complex> &test,
                         const std::vector<Complex> &reference,
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

static void report(const String &name, Real value, Real limit) {
  bool passed = value > STALE_ATTRIBUTE_FLOOR && value < limit;
  std::cout << (passed ? "  PASS  " : "  FAIL  ") << std::left << std::setw(56)
            << name << std::right << std::scientific << std::setprecision(3)
            << STALE_ATTRIBUTE_FLOOR << " < " << value << " < " << limit
            << std::endl;
  if (!passed)
    ++gFailures;
}

static void runCase(const String &tag, Real frequency) {
  Rig decoupled = decoupledRig(tag, frequency);
  Rig piLine = piLineRig(tag, frequency);
  Rig elements = elementsRig(tag, frequency);

  std::vector<Real> times = runAll({&decoupled, &piLine, &elements});

  std::cout << std::defaultfloat << "system frequency " << frequency
            << " Hz, electrical length " << 2. * PI * frequency * TRAVEL_TIME
            << " rad" << std::endl;

  report(tag + " steady boundary voltage against PiLine",
         relativeRmse(decoupled.voltage, piLine.voltage, times, SETTLE_TIME,
                      MODULATION_START),
         STEADY_BOUND_VOLTAGE);
  report(tag + " steady boundary current against PiLine",
         relativeRmse(decoupled.current, piLine.current, times, SETTLE_TIME,
                      MODULATION_START),
         STEADY_BOUND_CURRENT);
  report(tag + " steady boundary voltage against lumped elements",
         relativeRmse(decoupled.voltage, elements.voltage, times, SETTLE_TIME,
                      MODULATION_START),
         STEADY_BOUND_VOLTAGE);
  report(tag + " steady boundary current against lumped elements",
         relativeRmse(decoupled.current, elements.current, times, SETTLE_TIME,
                      MODULATION_START),
         STEADY_BOUND_CURRENT);
  report(tag + " modulated boundary voltage against PiLine",
         relativeRmse(decoupled.voltage, piLine.voltage, times,
                      MODULATION_START, FINAL_TIME),
         MODULATED_BOUND_VOLTAGE);
  report(tag + " modulated boundary current against PiLine",
         relativeRmse(decoupled.current, piLine.current, times,
                      MODULATION_START, FINAL_TIME),
         MODULATED_BOUND_CURRENT);
}

int main(int argc, char *argv[]) {
  Logger::setLogDir("logs/SP_DecouplingLine");

  std::cout << "SP::Ph1::DecouplingLine against lumped references, "
            << "surge impedance " << SURGE_IMPEDANCE << " ohm, travel time "
            << TRAVEL_TIME << " s" << std::endl;

  runCase("f50", 50.);
  runCase("f60", 60.);

  std::cout << (gFailures == 0 ? "All checks passed" : "Checks failed")
            << std::endl;
  return gFailures == 0 ? 0 : 1;
}
