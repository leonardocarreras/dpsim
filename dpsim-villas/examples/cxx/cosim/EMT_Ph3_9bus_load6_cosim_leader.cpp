// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// EMT leader of the WSCC 9-bus SFA<->EMT co-simulation.
//
// ITM (interface transmission) role: this side is the VOLTAGE-SOURCE side. It
// imports the interface voltage and drives a controlled voltage source with it,
// then measures and exports the resulting branch current. The DP follower is
// the complementary CURRENT-SOURCE side.
//
// This process owns the EMT (3-phase, instantaneous) side of the coupling at
// bus 6. It holds only the local sub-network around the interface:
//   GND -> vs -> nvrEMT -> rvs -> nriEMT -> ivs -> n1EMT -> load6 -> GND
// where load6 is the actual NineBus load-6 impedance (so the interface sees the
// correct steady-state current at t=0) and vs is a controlled voltage source
// driven by the bus-6 voltage the DP follower reports.
//
// Interface signal flow (must match configs/villas-emt-dp-cosim.conf):
//   import (bridge -> leader): seq counters + VA/VB/VC  -> drive vs
//   export (leader -> bridge): seq counters + IA/IB/IC  (load-6 branch current)
//
// Start-time synchronisation uses InterfaceCosimSync (TCP rendezvous): this
// process is the Leader and publishes the absolute start instant, timestep and
// duration; the follower waits for and adopts them.

#include "../examples/cxx/Examples.h"

#include <DPsim.h>
#include <dpsim-villas/InterfaceVillasQueueless.h>
#include <dpsim/InterfaceCosimSync.h>

using namespace DPsim;
using namespace CPS;

CPS::CIM::Examples::Grids::IEEE9::ScenarioConfig ninebus;

/// @brief Build this process's VILLAS shmem node configuration.
///
/// Node and signal names must match configs/villas-emt-dp-cosim.conf: the leader
/// writes currents to /dpsim.follower-to-leader and reads voltages from
/// /dpsim.leader-to-follower.
const std::string buildVillasConfig() {
  const std::string signalOutConfig = fmt::format(
      R"STRING("out": {{
      "name": "/dpsim.follower-to-leader",
      "signals": [
        {{ "name": "seq_to_self",     "type": "integer" }},
        {{ "name": "seq_to_external", "type": "integer" }},
        {{ "name": "IA", "type": "float", "unit": "A" }},
        {{ "name": "IB", "type": "float", "unit": "A" }},
        {{ "name": "IC", "type": "float", "unit": "A" }}
      ]}})STRING");
  const std::string signalInConfig = fmt::format(
      R"STRING("in": {{
      "name": "/dpsim.leader-to-follower",
      "signals": [
        {{ "name": "seq_from_self",     "type": "integer" }},
        {{ "name": "seq_from_external", "type": "integer" }},
        {{ "name": "VA", "type": "float", "unit": "V" }},
        {{ "name": "VB", "type": "float", "unit": "V" }},
        {{ "name": "VC", "type": "float", "unit": "V" }}
      ]}})STRING");
  return fmt::format(R"STRING({{ "type": "shmem", {}, {} }})STRING",
                     signalOutConfig, signalInConfig);
}

/// @brief Assemble the EMT interface sub-network and register the interface
///        signals.
///
/// Builds the controlled-source Thevenin chain feeding load 6, wires the
/// imports (bus-6 voltages) and exports (branch current) onto @p intf, and adds
/// the CSV log attributes.
/// @param intf   VILLAS interface the imports/exports are registered on.
/// @param logger Optional CSV logger for the interface signals (may be null).
SystemTopology buildTopology(CommandLineArgs &args,
                             std::shared_ptr<Interface> intf,
                             std::shared_ptr<DataLoggerInterface> logger) {
  String simNameEMT = args.name + "_EMT";
  CPS::Logger::setLogDir("logs/" + simNameEMT);

  // n1EMT: coupling bus (load-6 node). nriEMT/nvrEMT: internal nodes of the
  // controlled-source Thevenin chain that injects the follower's bus-6 voltage.
  auto n1EMT = SimNode<Real>::make("BUS6_eq", PhaseType::ABC);
  auto nriEMT = SimNode<Real>::make("BUS6_ri", PhaseType::ABC);
  auto nvrEMT = SimNode<Real>::make("BUS6_vr", PhaseType::ABC);

  // Constant-impedance load 6 (P, Q from the NineBus scenario at 230 kV base).
  auto load6 =
      EMT::Ph3::RXLoad::make(ninebus.load6.Name, CPS::Logger::Level::off);
  load6->setParameters(
      Math::singlePhasePowerToThreePhase(ninebus.load6.RealPower),
      Math::singlePhasePowerToThreePhase(ninebus.load6.ReactivePower),
      ninebus.load6.BaseVoltage);

  // Controlled voltage source (follows the follower's bus-6 voltage) plus a
  // small series R-L to avoid an algebraic loop at the interface.
  auto vs = EMT::Ph3::ControlledVoltageSource::make("vs");
  vs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0));
  auto rvs = EMT::Ph3::Resistor::make("rvs");
  rvs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0.1));
  auto ivs = EMT::Ph3::Inductor::make("ivs");
  ivs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0.01));

  vs->connect({SimNode<Real>::GND, nvrEMT});
  rvs->connect({nvrEMT, nriEMT});
  ivs->connect({nriEMT, n1EMT});
  load6->connect({n1EMT});

  auto systemEMT =
      SystemTopology(ninebus.nomFreq, SystemNodeList{n1EMT, nvrEMT, nriEMT},
                     SystemComponentList{vs, rvs, ivs, load6});

  // Sequence counters exchanged alongside the physical signals so each side can
  // detect a stale or dropped frame from its peer.
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

  // Import: seq counters + bus-6 voltages -> vs reference. Start-of-sim sync is
  // handled by InterfaceCosimSync, so imports do not block on a first read.
  intf->addImport(seqFromExternal, false, false);
  intf->addImport(seqFromDPsim, false, false);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(0, 0), false, false);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(1, 0), false, false);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(2, 0), false, false);

  // Export: seq counters + load-6 branch current (through rvs) to the follower.
  intf->addExport(seqToDPsim);
  intf->addExport(seqFromExternal);
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(0, 0));
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(1, 0));
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(2, 0));

  if (logger) {
    logger->logAttribute("a", n1EMT->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b", n1EMT->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c", n1EMT->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("ax", nvrEMT->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("bx", nvrEMT->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("cx", nvrEMT->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("a_i", rvs->mIntfCurrent->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b_i", rvs->mIntfCurrent->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c_i", rvs->mIntfCurrent->deriveCoeff<Real>(2, 0));
  }

  systemEMT.renderToFile("logs/" + simNameEMT + ".svg");
  return systemEMT;
}

int main(int argc, char *argv[]) {
  CommandLineArgs args(argc, argv, "EMT_3Ph_9bus_load6_cosim_leader", 0.00005,
                       60.0, ninebus.nomFreq, -1, CPS::Logger::Level::info,
                       CPS::Logger::Level::info, false, false, false,
                       CPS::Domain::EMT);

  CPS::Logger::setLogDir("logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  auto villasIntf = std::make_shared<InterfaceVillasQueueless>(
      buildVillasConfig(), "VillasInterface", spdlog::level::off);

  // Start-time rendezvous (TCP). The leader binds a port; the follower connects.
  // Host is ignored for a leader; port is overridable via env or -o port=.
  uint16_t port = 8888;
  if (const char *env = std::getenv("DPSIM_SYNC_PORT"))
    port = static_cast<uint16_t>(std::stoi(env));
  if (args.options.find("port") != args.options.end())
    port = static_cast<uint16_t>(args.getOptionInt("port"));
  auto syncIntf = std::make_shared<InterfaceCosimSync>(
      "CosimSync", "", port, InterfaceCosimSync::Role::Leader);
  syncIntf->open();

  std::shared_ptr<DataLoggerInterface> logger = nullptr;
  std::filesystem::path logFilename =
      "logs/" + args.name + "/" + args.name + ".csv";
  if (log) {
    // The RT sim logs the t=0 step and the final step, i.e. two more than
    // duration/timestep; size the preallocated logger with that headroom so its
    // bounds check is not tripped on the last step.
    size_t rows = static_cast<size_t>(args.duration / args.timeStep + 0.5) + 2;
    logger = RealTimeDataLogger::make(logFilename, rows);
  }

  auto sys = buildTopology(args, villasIntf, logger);

  RealTimeSimulation sim(args.name, args);
  sim.setSystem(sys);
  sim.addInterface(villasIntf);
  sim.setDomain(Domain::EMT);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);
  if (logger)
    sim.addLogger(logger);

  if (args.options.find("threads") != args.options.end())
    sim.setScheduler(
        std::make_shared<OpenMPLevelScheduler>(args.getOptionInt("threads")));

  // Publish an absolute start instant a few seconds out so the follower has
  // time to connect, then run both sides from the same instant.
  auto startAt = std::chrono::system_clock::now() + std::chrono::seconds(5);
  uint64_t dtNs = static_cast<uint64_t>(args.timeStep * 1e9);
  uint64_t durNs = static_cast<uint64_t>(args.duration * 1e9);
  if (!syncIntf->publishConfig(startAt, dtNs, durNs, 1, 30000)) {
    CPS::Logger::get(args.name)->error(
        "Follower did not acknowledge sync config");
    return 1;
  }

  sim.run(startAt);

  sim.logStepTimes(args.name + "_step_times");
  sim.checkForOverruns(args.name + "_overruns");
  CPS::Logger::get(args.name)->info("Simulation finished.");
  return 0;
}
