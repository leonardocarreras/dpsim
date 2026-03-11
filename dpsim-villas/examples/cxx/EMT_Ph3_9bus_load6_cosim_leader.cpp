// Leader for the 9-bus WSCC co-simulation.
// Replaces the simple rl (10 Ω) with the actual load6 impedance from the
// NineBus scenario, so the follower sees the correct steady-state current
// at t=0 and the simulation does not diverge.
//
// Signal flow (same as EMT_Ph3_simple_cosim_leader_passive_sync):
//   Import (from follower):  seq_from_*, VA, VB, VC  (bus6 voltages)
//   Export (to follower):    seq_to_*, IA, IB, IC    (load6 currents)

#include "../../dpsim/examples/cxx/Examples.h"

#include <DPsim.h>
#include <dpsim-villas/InterfaceVillasQueueless.h>
#include <dpsim/InterfaceCosimSyncShmem.h>

using namespace DPsim;
using namespace CPS;

CPS::CIM::Examples::Grids::NineBus::ScenarioConfig ninebus;

const std::string buildVillasConfig(CommandLineArgs &args) {
  std::string signalOutConfig = fmt::format(
      R"STRING("out": {{
      "name": "/dpsim.follower-to-leader",
      "signals": [{{
        "name": "seq_to_self",
        "type": "integer",
        "unit": "",
        "builtin": false
      }},
      {{
        "name": "seq_to_external",
        "type": "integer",
        "unit": "",
        "builtin": false
      }},
      {{
        "name": "IA",
        "type": "float",
        "unit": "A",
        "builtin": false
      }},
      {{
        "name": "IB",
        "type": "float",
        "unit": "A",
        "builtin": false
      }},
      {{
        "name": "IC",
        "type": "float",
        "unit": "A",
        "builtin": false
      }}]
    }})STRING");
  std::string signalInConfig = fmt::format(
      R"STRING("in": {{
      "name": "/dpsim.leader-to-follower",
      "signals": [{{
        "name": "seq_from_self",
        "type": "integer",
        "unit": "",
        "builtin": false
      }},
      {{
        "name": "seq_from_external",
        "type": "integer",
        "unit": "",
        "builtin": false
      }},
      {{
        "name": "VA",
        "type": "float",
        "unit": "V",
        "builtin": false
      }},
      {{
        "name": "VB",
        "type": "float",
        "unit": "V",
        "builtin": false
      }},
      {{
        "name": "VC",
        "type": "float",
        "unit": "V",
        "builtin": false
      }}]
    }})STRING");
  const std::string config = fmt::format(
      R"STRING({{
    "type": "shmem",
    {},
    {}
  }})STRING",
      signalOutConfig, signalInConfig);
  DPsim::Logger::get("9bus_load6_cosim_leader")
      ->info("Config for Node:\n{}", config);
  return config;
}

SystemTopology buildTopology(CommandLineArgs &args,
                             std::shared_ptr<Interface> intf,
                             std::shared_ptr<DataLoggerInterface> logger) {

  String simName = args.name;
  String simNameEMT = simName + "_EMT";
  CPS::Logger::setLogDir("logs/" + simNameEMT);

  // --- Nodes ---
  // n1EMT: the coupling bus (receives current from ivs, drives load6)
  // nriEMT, nvrEMT: internal nodes for the Thevenin source chain
  auto n1EMT   = SimNode<Real>::make("BUS6_eq",   PhaseType::ABC);
  auto nriEMT  = SimNode<Real>::make("BUS6_ri",   PhaseType::ABC);
  auto nvrEMT  = SimNode<Real>::make("BUS6_vr",   PhaseType::ABC);

  // --- Load6 equivalent impedance ---
  // RXLoad models a constant-impedance load (G + jB derived from P, Q, Vbase).
  // P = 90 MW (3-phase), Q = 30 MVAR (3-phase), Vbase = 230 kV
  auto load6 = EMT::Ph3::RXLoad::make(ninebus.load6.Name, CPS::Logger::Level::off);
  load6->setParameters(
      Math::singlePhasePowerToThreePhase(ninebus.load6.RealPower),
      Math::singlePhasePowerToThreePhase(ninebus.load6.ReactivePower),
      ninebus.load6.BaseVoltage);

  // --- Controlled-voltage-source Thevenin chain ---
  // vs drives the voltage equal to what the follower reports for bus6.
  // rvs + ivs are a small series impedance to prevent algebraic loops and
  // smooth the interface (same values as the simple passive leader).
  auto vs  = EMT::Ph3::ControlledVoltageSource::make("vs");
  vs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0));
  auto rvs = EMT::Ph3::Resistor::make("rvs");
  rvs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0.1));
  auto ivs = EMT::Ph3::Inductor::make("ivs");
  ivs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0.01));

  // Connections:
  //   GND → vs → nvrEMT → rvs → nriEMT → ivs → n1EMT → load6 → GND
  vs->connect({SimNode<Real>::GND, nvrEMT});
  rvs->connect({nvrEMT, nriEMT});
  ivs->connect({nriEMT, n1EMT});
  load6->connect({n1EMT});  // RXLoad connects node-to-GND

  // Create system topology
  auto systemEMT = SystemTopology(
      ninebus.nomFreq,
      SystemNodeList{n1EMT, nvrEMT, nriEMT},
      SystemComponentList{vs, rvs, ivs, load6});

  // --- Interface ---
  auto inFromExternal_SeqExternalAttribute = CPS::AttributeStatic<Int>::make(0);
  auto inFromExternal_SeqDPsimAttribute    = CPS::AttributeStatic<Int>::make(0);
  auto outToExternal_SeqDPsimAttribute     = CPS::AttributeDynamic<Int>::make(0);

  auto updateFn = std::make_shared<CPS::AttributeUpdateTask<Int, Int>::Actor>(
      [](std::shared_ptr<Int> &dependent,
         typename CPS::Attribute<Int>::Ptr dependency) {
        *dependent = *dependent + 1;
      });
  outToExternal_SeqDPsimAttribute->addTask(
      CPS::UpdateTaskKind::UPDATE_ON_GET,
      CPS::AttributeUpdateTask<Int, Int>::make(
          CPS::UpdateTaskKind::UPDATE_ON_GET, *updateFn,
          inFromExternal_SeqDPsimAttribute));

  // Import: seq counters + bus6 voltages from follower → drive vs
  intf->addImport(inFromExternal_SeqExternalAttribute, true, true);
  intf->addImport(inFromExternal_SeqDPsimAttribute, true, true);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(0, 0), true, true);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(1, 0), true, true);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(2, 0), true, true);

  // Export: seq counters + load6 current (through rvs) to follower
  intf->addExport(outToExternal_SeqDPsimAttribute);
  intf->addExport(inFromExternal_SeqExternalAttribute);
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(0, 0));
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(1, 0));
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(2, 0));

  // --- Logger ---
  if (logger) {
    // Voltage at the coupling bus
    logger->logAttribute("a",   n1EMT->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b",   n1EMT->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c",   n1EMT->mVoltage->deriveCoeff<Real>(2, 0));
    // Voltage source reference (follower bus6 voltage)
    logger->logAttribute("ax",  nvrEMT->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("bx",  nvrEMT->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("cx",  nvrEMT->mVoltage->deriveCoeff<Real>(2, 0));
    // Interface current (exported to follower)
    logger->logAttribute("a_i", rvs->mIntfCurrent->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b_i", rvs->mIntfCurrent->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c_i", rvs->mIntfCurrent->deriveCoeff<Real>(2, 0));
  }

  systemEMT.renderToFile("logs/" + simNameEMT + ".svg");

  return systemEMT;
}

int main(int argc, char *argv[]) {
  CommandLineArgs args(argc, argv, "EMT_3Ph_9bus_load6_cosim_leader",
                       0.00005, 60.0, ninebus.nomFreq, -1,
                       CPS::Logger::Level::info, CPS::Logger::Level::off,
                       false, false, false, CPS::Domain::EMT);

  CPS::Logger::setLogDir("logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  // Data exchange via VILLAS shared memory
  auto villasIntf = std::make_shared<InterfaceVillasQueueless>(
      buildVillasConfig(args), "VillasInterface", spdlog::level::off);

  // Cosim sync: this side is the Leader — publishes start time & config
  std::string shmName = "/dpsim_sync_case";
  if (const char *env = std::getenv("DPSIM_SYNC_SHM")) {
    shmName = env;
  }
  if (args.options.find("shm") != args.options.end()) {
    shmName = args.getOptionString("shm");
  }
  auto syncIntf = std::make_shared<InterfaceCosimSyncShmem>(
      "CosimSync", shmName, InterfaceCosimSyncShmem::Role::Leader);
  syncIntf->open();

  std::filesystem::path logFilename =
      "logs/" + args.name + "/" + args.name + ".csv";
  std::shared_ptr<DataLoggerInterface> logger = nullptr;
  if (log) {
    logger = RealTimeDataLogger::make(logFilename, args.duration, args.timeStep);
  }

  auto sys = buildTopology(args, villasIntf, logger);

  RealTimeSimulation sim(args.name, args);
  sim.setSystem(sys);
  sim.addInterface(villasIntf);
  sim.setDomain(Domain::EMT);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);

  // Drop config (allow override via -o drop= and -o drop_threshold=)
  bool dropEnabled = true;
  double dropThreshold = 0.95;
  if (args.options.find("drop") != args.options.end()) {
    try { dropEnabled = args.getOptionBool("drop"); } catch (...) {}
  }
  if (args.options.find("drop_threshold") != args.options.end()) {
    try { dropThreshold = args.getOptionReal("drop_threshold"); } catch (...) {}
  }
  sim.setDropEnabled(dropEnabled);
  sim.setDropThreshold(dropThreshold);

  if (logger)
    sim.addLogger(logger);

  if (args.options.find("threads") != args.options.end()) {
    auto numThreads = args.getOptionInt("threads");
    sim.setScheduler(std::make_shared<OpenMPLevelScheduler>(numThreads));
  }

  // Start 5 s in the future and publish the config to the follower
  auto startAt = std::chrono::system_clock::now() + std::chrono::seconds(5);
  {
    using namespace std::chrono;
    auto now = system_clock::now();
    double secs = duration_cast<duration<double>>(startAt - now).count();
    if (secs < 0) {
      CPS::Logger::get(args.name)->error(
          "Start time is in the past by {} s. Aborting.", -secs);
      return 1;
    }
    if (secs > 3600.0) {
      CPS::Logger::get(args.name)->warn(
          "Start time is more than 1 hour away ({} s).", secs);
    }
  }
  uint64_t dt_ns  = static_cast<uint64_t>(args.timeStep * 1e9);
  uint64_t dur_ns = static_cast<uint64_t>(args.duration * 1e9);
  syncIntf->publishConfig(startAt, dt_ns, dur_ns);
  CPS::Logger::get(args.name)->info(
      "Published sync to {} (dt_ns={}, duration_ns={})", shmName, dt_ns, dur_ns);

  sim.run(startAt);

  sim.logStepTimes(args.name + "_step_times");
  sim.checkForOverruns(args.name + "_overruns");
  CPS::Logger::get(args.name)->info("Simulation finished.");
}
