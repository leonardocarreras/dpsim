// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// DP follower of the WSCC 9-bus SFA<->EMT co-simulation.
//
// ITM (interface transmission) role: this side is the CURRENT-SOURCE side. It
// imports the interface current and drives a controlled current source at bus 6
// with it, then measures and exports the resulting bus voltage. The EMT leader
// is the complementary VOLTAGE-SOURCE side.
//
// This process owns the dynamic-phasor (DP) side: the full 9-bus network with
// three 4th-order VBR synchronous generators (plus exciters and governors),
// initialised from a power-flow solution. Load 6 is removed and replaced by a
// controlled current source; the current injected there is what the EMT leader
// reports at the interface, and the bus-6 voltage this network computes is sent
// back to the leader.
//
// Interface signal flow (must match configs/villas-emt-dp-cosim.conf):
//   import (bridge -> follower): seq counters + IA (bus-6 injection current)
//   export (follower -> bridge): seq counters + VA/VB/VC (bus-6 voltage)
//
// DP is single-phase (positive-sequence) here while the EMT side is 3-phase, so
// the coupling assumes a balanced interface: the single DP voltage phasor is
// fanned out to three balanced phases (VB/VC via a 120/240 deg shift) for the
// EMT side, and only the positive-sequence current is taken back. A 3-phase DP
// interface is a planned extension, not covered here.
//
// Start-time synchronisation uses InterfaceCosimSync (TCP): this process is the
// Follower and adopts the timestep, duration and start instant the leader sends.

#include "../examples/cxx/Examples.h"

#include <DPsim.h>

#include <dpsim-villas/InterfaceVillasQueueless.h>
#include <dpsim/InterfaceCosimSync.h>

using namespace DPsim;
using namespace CPS;

CPS::CIM::Examples::Grids::IEEE9::ScenarioConfig ninebus;

/// @brief Build this process's VILLAS shmem node configuration.
///
/// Node and signal names must match configs/villas-emt-dp-cosim.conf: the
/// follower writes voltage phasors to /dpsim.dp-follower-to-bridge and reads the
/// current phasor from /dpsim.bridge-to-dp-follower.
const std::string buildVillasConfig() {
  const std::string signalOutConfig = fmt::format(
      R"STRING("out": {{
      "name": "/dpsim.dp-follower-to-bridge",
      "signals": [
        {{ "name": "seq_to_self",     "type": "integer" }},
        {{ "name": "seq_to_external", "type": "integer" }},
        {{ "name": "VA", "type": "complex", "unit": "V" }},
        {{ "name": "VB", "type": "complex", "unit": "V" }},
        {{ "name": "VC", "type": "complex", "unit": "V" }}
      ]}})STRING");
  const std::string signalInConfig = fmt::format(
      R"STRING("in": {{
      "name": "/dpsim.bridge-to-dp-follower",
      "signals": [
        {{ "name": "seq_from_self",     "type": "integer" }},
        {{ "name": "seq_from_external", "type": "integer" }},
        {{ "name": "IA", "type": "complex", "unit": "A" }}
      ]}})STRING");
  return fmt::format(R"STRING({{ "type": "shmem", {}, {} }})STRING",
                     signalOutConfig, signalInConfig);
}

/// @brief Build the SP power-flow network used to initialise the dynamic DP run.
SystemTopology buildPowerFlow(CommandLineArgs &args) {
  auto n1 = SimNode<Complex>::make("BUS1", PhaseType::Single);
  auto n2 = SimNode<Complex>::make("BUS2", PhaseType::Single);
  auto n3 = SimNode<Complex>::make("BUS3", PhaseType::Single);
  auto n4 = SimNode<Complex>::make("BUS4", PhaseType::Single);
  auto n5 = SimNode<Complex>::make("BUS5", PhaseType::Single);
  auto n6 = SimNode<Complex>::make("BUS6", PhaseType::Single);
  auto n7 = SimNode<Complex>::make("BUS7", PhaseType::Single);
  auto n8 = SimNode<Complex>::make("BUS8", PhaseType::Single);
  auto n9 = SimNode<Complex>::make("BUS9", PhaseType::Single);

  auto gen1 = SP::Ph1::SynchronGenerator::make(ninebus.gen1.Name,
                                               CPS::Logger::Level::off);
  gen1->setParameters(ninebus.gen1.RatedPower, ninebus.gen1.RatedVoltage,
                      ninebus.gen1.InitialPower, ninebus.gen1.InitialVoltage,
                      ninebus.gen1.BusType);
  gen1->setBaseVoltage(ninebus.gen1.RatedVoltage);
  auto gen2 = SP::Ph1::SynchronGenerator::make(ninebus.gen2.Name,
                                               CPS::Logger::Level::off);
  gen2->setParameters(ninebus.gen2.RatedPower, ninebus.gen2.RatedVoltage,
                      ninebus.gen2.InitialPower, ninebus.gen2.InitialVoltage,
                      ninebus.gen2.BusType);
  gen2->setBaseVoltage(ninebus.gen2.RatedVoltage);
  auto gen3 = SP::Ph1::SynchronGenerator::make(ninebus.gen3.Name,
                                               CPS::Logger::Level::off);
  gen3->setParameters(ninebus.gen3.RatedPower, ninebus.gen3.RatedVoltage,
                      ninebus.gen3.InitialPower, ninebus.gen3.InitialVoltage,
                      ninebus.gen3.BusType);
  gen3->setBaseVoltage(ninebus.gen3.RatedVoltage);

  auto load5 = SP::Ph1::Load::make(ninebus.load5.Name, CPS::Logger::Level::off);
  load5->setParameters(ninebus.load5.RealPower, ninebus.load5.ReactivePower,
                       ninebus.load5.BaseVoltage);
  load5->modifyPowerFlowBusType(PowerflowBusType::PQ);
  auto load6 = SP::Ph1::Load::make(ninebus.load6.Name, CPS::Logger::Level::off);
  load6->setParameters(ninebus.load6.RealPower, ninebus.load6.ReactivePower,
                       ninebus.load6.BaseVoltage);
  load6->modifyPowerFlowBusType(PowerflowBusType::PQ);
  auto load8 = SP::Ph1::Load::make(ninebus.load8.Name, CPS::Logger::Level::off);
  load8->setParameters(ninebus.load8.RealPower, ninebus.load8.ReactivePower,
                       ninebus.load8.BaseVoltage);
  load8->modifyPowerFlowBusType(PowerflowBusType::PQ);

  auto makeLine = [](const CPS::CIM::Examples::Grids::IEEE9::Line &l) {
    auto line = SP::Ph1::PiLine::make(l.Name, CPS::Logger::Level::off);
    line->setParameters(l.Resistance, l.Inductance, l.Capacitance,
                        l.Conductance);
    line->setBaseVoltage(l.BaseVoltage);
    return line;
  };
  auto line54 = makeLine(ninebus.line54);
  auto line64 = makeLine(ninebus.line64);
  auto line75 = makeLine(ninebus.line75);
  auto line96 = makeLine(ninebus.line96);
  auto line78 = makeLine(ninebus.line78);
  auto line89 = makeLine(ninebus.line89);

  auto makeTransf = [](const CPS::CIM::Examples::Grids::IEEE9::Transformer &t) {
    auto tr = SP::Ph1::Transformer::make(t.Name, CPS::Logger::Level::off);
    tr->setParameters(t.VoltageLVSide, t.VoltageHVSide, t.Ratio, 0.0,
                      t.Resistance, t.Inductance);
    tr->setBaseVoltage(t.VoltageHVSide);
    return tr;
  };
  auto transf14 = makeTransf(ninebus.transf14);
  auto transf27 = makeTransf(ninebus.transf27);
  auto transf39 = makeTransf(ninebus.transf39);

  gen1->connect({n1});
  gen2->connect({n2});
  gen3->connect({n3});
  load5->connect({n5});
  load6->connect({n6});
  load8->connect({n8});
  line54->connect({n5, n4});
  line64->connect({n6, n4});
  line75->connect({n7, n5});
  line96->connect({n9, n6});
  line78->connect({n7, n8});
  line89->connect({n8, n9});
  transf14->connect({n1, n4});
  transf27->connect({n2, n7});
  transf39->connect({n3, n9});

  return SystemTopology(
      ninebus.nomFreq, SystemNodeList{n1, n2, n3, n4, n5, n6, n7, n8, n9},
      SystemComponentList{gen1, gen2, gen3, load5, load6, load8, line54, line64,
                          line75, line96, line78, line89, transf14, transf27,
                          transf39});
}

/// @brief Assemble the dynamic DP 9-bus network and register interface signals.
///
/// Solves the power flow, builds the DP network with 4th-order VBR generators,
/// replaces load 6 with a controlled current source, and wires the imports
/// (bus-6 current) and exports (bus-6 voltage) onto @p intf.
/// @param intf   VILLAS interface the imports/exports are registered on.
/// @param logger Optional CSV logger for the interface and generator states.
SystemTopology buildTopology(CommandLineArgs &args,
                             std::shared_ptr<Interface> intf,
                             std::shared_ptr<DataLoggerInterface> logger) {
  String simNamePF = args.name + "_PF";
  CPS::Logger::setLogDir("logs/" + simNamePF);

  auto systemPF = buildPowerFlow(args);
  Simulation simPF(simNamePF, CPS::Logger::Level::off);
  simPF.setSystem(systemPF);
  simPF.setTimeStep(args.timeStep);
  simPF.setFinalTime(1 * args.timeStep);
  simPF.setDomain(Domain::SP);
  simPF.setSolverType(Solver::Type::NRP);
  simPF.setSolverAndComponentBehaviour(Solver::Behaviour::Simulation);
  simPF.run();

  // ----- Dynamic DP network -----
  String simNameDP = args.name + "_DP";
  CPS::Logger::setLogDir("logs/" + simNameDP);

  auto n1 = SimNode<Complex>::make("BUS1", PhaseType::Single);
  auto n2 = SimNode<Complex>::make("BUS2", PhaseType::Single);
  auto n3 = SimNode<Complex>::make("BUS3", PhaseType::Single);
  auto n4 = SimNode<Complex>::make("BUS4", PhaseType::Single);
  auto n5 = SimNode<Complex>::make("BUS5", PhaseType::Single);
  auto n6 = SimNode<Complex>::make("BUS6", PhaseType::Single);
  auto n7 = SimNode<Complex>::make("BUS7", PhaseType::Single);
  auto n8 = SimNode<Complex>::make("BUS8", PhaseType::Single);
  auto n9 = SimNode<Complex>::make("BUS9", PhaseType::Single);

  const CPS::Real T4 = 1.0, T5 = 1.0;
  auto makeGen = [&](const CPS::CIM::Examples::Grids::IEEE9::Gen &g,
                     const CPS::CIM::Examples::Grids::IEEE9::Exciter &e,
                     const CPS::CIM::Examples::Grids::IEEE9::Governor &gov) {
    auto gen = DP::Ph1::SynchronGenerator4OrderVBR::make(
        g.Name, CPS::Logger::Level::off);
    gen->setOperationalParametersPerUnit(
        g.RatedPower, g.RatedVoltage, ninebus.nomFreq, g.H, g.Xd, g.Xq, g.Xa,
        g.XdPrime, g.XqPrime, g.TdoPrime, g.TqoPrime);

    auto excParams = std::make_shared<Signal::ExciterDC1SimpParameters>();
    excParams->Ta = e.TA;
    excParams->Ka = e.KA;
    excParams->Tef = e.TE;
    excParams->Kef = e.KE;
    excParams->Tf = e.TF;
    excParams->Kf = e.KF;
    excParams->Tr = 0.01;
    excParams->MaxVa = e.VRmax;
    excParams->MinVa = e.VRmin;
    excParams->Bef = std::log(e.S_EX2 / e.S_EX1) / (e.EX2 - e.EX1);
    excParams->Aef = e.S_EX1 / std::exp(excParams->Bef * e.EX1);
    auto exc = Signal::ExciterDC1Simp::make(g.Name + "_Exciter",
                                            CPS::Logger::Level::off);
    exc->setParameters(excParams);
    gen->addExciter(exc);

    auto tg = Signal::TurbineGovernorType1::make(g.Name + "_TurbineGovernor",
                                                 CPS::Logger::Level::off);
    tg->setParameters(gov.T2, T4, T5, gov.T3, gov.T1, gov.R, gov.Vmin, gov.Vmax,
                      1.0);
    gen->addGovernor(tg);
    return gen;
  };
  auto gen1 = makeGen(ninebus.gen1, ninebus.exc1, ninebus.gov1);
  auto gen2 = makeGen(ninebus.gen2, ninebus.exc2, ninebus.gov2);
  auto gen3 = makeGen(ninebus.gen3, ninebus.exc3, ninebus.gov3);

  auto load5 =
      DP::Ph1::RXLoad::make(ninebus.load5.Name, CPS::Logger::Level::off);
  load5->setParameters(ninebus.load5.RealPower, ninebus.load5.ReactivePower,
                       ninebus.load5.BaseVoltage);
  auto load6 =
      DP::Ph1::RXLoad::make(ninebus.load6.Name, CPS::Logger::Level::off);
  load6->setParameters(ninebus.load6.RealPower, ninebus.load6.ReactivePower,
                       ninebus.load6.BaseVoltage);
  auto load8 =
      DP::Ph1::RXLoad::make(ninebus.load8.Name, CPS::Logger::Level::off);
  load8->setParameters(ninebus.load8.RealPower, ninebus.load8.ReactivePower,
                       ninebus.load8.BaseVoltage);

  auto makeLine = [](const CPS::CIM::Examples::Grids::IEEE9::Line &l) {
    auto line = DP::Ph1::PiLine::make(l.Name, CPS::Logger::Level::off);
    line->setParameters(l.Resistance, l.Inductance, l.Capacitance,
                        l.Conductance);
    return line;
  };
  auto line54 = makeLine(ninebus.line54);
  auto line64 = makeLine(ninebus.line64);
  auto line75 = makeLine(ninebus.line75);
  auto line96 = makeLine(ninebus.line96);
  auto line78 = makeLine(ninebus.line78);
  auto line89 = makeLine(ninebus.line89);

  // The DP transformer keeps the shared rated power of transf14 for all three,
  // matching the tested scenario setup.
  auto makeTransf =
      [&](const CPS::CIM::Examples::Grids::IEEE9::Transformer &t) {
        auto tr = DP::Ph1::Transformer::make(t.Name, CPS::Logger::Level::off);
        tr->setParameters(t.VoltageLVSide, t.VoltageHVSide,
                          ninebus.transf14.RatedPower, t.Ratio, 0.0,
                          t.Resistance, t.Inductance);
        return tr;
      };
  auto transf14 = makeTransf(ninebus.transf14);
  auto transf27 = makeTransf(ninebus.transf27);
  auto transf39 = makeTransf(ninebus.transf39);

  gen1->connect({n1});
  gen2->connect({n2});
  gen3->connect({n3});
  load5->connect({n5});
  load6->connect({n6});
  load8->connect({n8});
  line54->connect({n5, n4});
  line64->connect({n6, n4});
  line75->connect({n7, n5});
  line96->connect({n9, n6});
  line78->connect({n7, n8});
  line89->connect({n8, n9});
  transf14->connect({n1, n4});
  transf27->connect({n2, n7});
  transf39->connect({n3, n9});

  auto systemDP = SystemTopology(
      ninebus.nomFreq, SystemNodeList{n1, n2, n3, n4, n5, n6, n7, n8, n9},
      SystemComponentList{gen1, gen2, gen3, load5, load6, load8, line54, line64,
                          line75, line96, line78, line89, transf14, transf27,
                          transf39});
  systemDP.initWithPowerflow(systemPF, Domain::DP);

  // Replace load 6 with a controlled current source: the interface current the
  // EMT leader injects at bus 6.
  auto cs = DP::Ph1::CurrentSource::make("cs");
  cs->setParameters(Complex(0, 0));
  cs->connect({n6, DP::SimNode::GND});
  cs->initializeFromNodesAndTerminals(ninebus.nomFreq);
  systemDP.addComponent(cs);

  // Sequence counters exchanged alongside the physical signals (see leader).
  auto seqFromExternal = CPS::AttributeStatic<Int>::make(0);
  auto seqFromDPsim = CPS::AttributeStatic<Int>::make(0);
  auto seqToDPsim = CPS::AttributeDynamic<Int>::make(0);
  auto updateFn = std::make_shared<CPS::AttributeUpdateTask<Int, Int>::Actor>(
      [](std::shared_ptr<Int> &dependent, CPS::Attribute<Int>::Ptr) {
        *dependent = *dependent + 1;
      });
  seqToDPsim->addTask(
      CPS::UpdateTaskKind::UPDATE_ON_GET,
      CPS::AttributeUpdateTask<Int, Int>::make(
          CPS::UpdateTaskKind::UPDATE_ON_GET, *updateFn, seqFromDPsim));

  // Import: seq counters + bus-6 injection current. blockOnRead is true so the
  // follower stays aligned with the bridge frame by frame.
  intf->addImport(seqFromExternal, true, true);
  intf->addImport(seqFromDPsim, true, true);
  intf->addImport(cs->mCurrentRef, true, true);

  // Export: seq counters + bus-6 voltage. VB/VC are the positive-sequence
  // voltage rotated by -120/-240 deg (balanced-interface assumption).
  intf->addExport(seqToDPsim);
  intf->addExport(seqFromExternal);
  intf->addExport(n6->mVoltage->deriveCoeff<Complex>(0, 0));
  intf->addExport(
      n6->mVoltage->deriveCoeff<Complex>(0, 0)->deriveScaled(SHIFT_TO_PHASE_B));
  intf->addExport(
      n6->mVoltage->deriveCoeff<Complex>(0, 0)->deriveScaled(SHIFT_TO_PHASE_C));

  if (logger) {
    logger->logAttribute("BUS6", n6->attribute("v"));
    logger->logAttribute("BUS6_A", n6->mVoltage->deriveCoeff<Complex>(0, 0));
    logger->logAttribute("BUS6_i", cs->mIntfCurrent);
    for (auto comp : systemDP.mComponents) {
      if (std::dynamic_pointer_cast<CPS::DP::Ph1::SynchronGenerator4OrderVBR>(
              comp)) {
        logger->logAttribute(comp->name() + ".I", comp->attribute("i_intf"));
        logger->logAttribute(comp->name() + ".V", comp->attribute("v_intf"));
        logger->logAttribute(comp->name() + ".omega", comp->attribute("w_r"));
        logger->logAttribute(comp->name() + ".delta", comp->attribute("delta"));
      }
    }
  }

  // Remove the power-flow load 6; the current source now feeds bus 6.
  systemDP.removeComponent(ninebus.load6.Name);
  systemDP.renderToFile("logs/" + simNameDP + ".svg");
  return systemDP;
}

int main(int argc, char *argv[]) {
  CommandLineArgs args(argc, argv, "DP_Ph1_9bus_4order_cosim_follower", 0.00005,
                       60.0, ninebus.nomFreq, -1, CPS::Logger::Level::info,
                       CPS::Logger::Level::off, false, false, false,
                       CPS::Domain::DP);

  std::error_code ec;
  std::filesystem::create_directories("./logs", ec);
  CPS::Logger::setLogDir("./logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  auto villasIntf = std::make_shared<InterfaceVillasQueueless>(
      buildVillasConfig(), "VillasInterface", spdlog::level::off);

  // Start-time rendezvous (TCP): connect to the leader and adopt its config.
  std::string host = "127.0.0.1";
  uint16_t port = 8888;
  if (const char *env = std::getenv("DPSIM_SYNC_HOST"))
    host = env;
  if (const char *env = std::getenv("DPSIM_SYNC_PORT"))
    port = static_cast<uint16_t>(std::stoi(env));
  if (args.options.find("host") != args.options.end())
    host = args.getOptionString("host");
  if (args.options.find("port") != args.options.end())
    port = static_cast<uint16_t>(args.getOptionInt("port"));

  auto syncIntf = std::make_shared<InterfaceCosimSync>(
      "CosimSync", host, port, InterfaceCosimSync::Role::Follower);
  syncIntf->open();

  InterfaceCosimSync::ConfigNs cfg;
  if (!syncIntf->waitForConfig(cfg, 30000)) {
    CPS::Logger::get(args.name)->error(
        "Timed out waiting for leader sync config");
    return 1;
  }
  args.timeStep = static_cast<double>(cfg.time_step_ns) / 1e9;
  args.duration = static_cast<double>(cfg.duration_ns) / 1e9;
  auto startAt = InterfaceCosimSync::toTimePoint(cfg.start_time_ns);

  std::shared_ptr<DataLoggerInterface> logger = nullptr;
  std::filesystem::path logFilename = "./logs/" + args.name + ".csv";
  if (log) {
    // See the leader: two extra rows of headroom for the t=0 and final steps.
    size_t rows = static_cast<size_t>(args.duration / args.timeStep + 0.5) + 2;
    logger = RealTimeDataLogger::make(logFilename, rows);
  }

  auto sys = buildTopology(args, villasIntf, logger);

  RealTimeSimulation sim(args.name, args);
  sim.setSystem(sys);
  sim.addInterface(villasIntf);
  sim.setDomain(Domain::DP);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);
  if (logger)
    sim.addLogger(logger);

  sim.run(startAt);

  sim.logStepTimes(args.name + "_step_times");
  sim.checkForOverruns(args.name + "_overruns");
  CPS::Logger::get(args.name)->info("Simulation finished.");
  return 0;
}
