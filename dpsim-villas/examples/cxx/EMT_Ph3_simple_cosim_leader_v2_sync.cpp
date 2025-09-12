#include <DPsim.h>

#include <dpsim-villas/InterfaceVillasQueueless.h>
#include <dpsim/InterfaceCosimSyncShmem.h>

using namespace DPsim;
using namespace CPS;

static const std::string buildVillasConfig(CommandLineArgs &args) {
  std::string signalOutConfig = fmt::format(
      R"STRING("out": {{
      "name": "/dpsim.leader-to-follower",
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
  std::string signalInConfig = fmt::format(
      R"STRING("in": {{
      "name": "/dpsim.follower-to-leader",
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
  const std::string config = fmt::format(
      R"STRING({{
    "type": "shmem",
    {},
    {}
  }})STRING",
      signalOutConfig, signalInConfig);
  DPsim::Logger::get("simple_cosim_leader_v2_sync")
      ->info("Config for Node:\n{}", config);
  return config;
}

static SystemTopology
buildTopology(CommandLineArgs &args, std::shared_ptr<Interface> intf,
              std::shared_ptr<DataLoggerInterface> logger) {

  String simName = args.name;

  String simNameEMT = simName + std::string("_EMT");
  CPS::Logger::setLogDir("logs/" + simNameEMT);

  auto n1EMT = SimNode<Real>::make("BUS1", PhaseType::ABC);

  auto rl = EMT::Ph3::Resistor::make("rl");
  rl->setParameters(CPS::Math::singlePhaseParameterToThreePhase(10));

  auto cs = EMT::Ph3::ControlledCurrentSource::make("cs");
  cs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0));
  auto rcs = EMT::Ph3::Resistor::make("rcs");
  rcs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(1e8));

  rl->connect({n1EMT, SimNode<Real>::GND});
  cs->connect({n1EMT, SimNode<Real>::GND});
  rcs->connect({n1EMT, SimNode<Real>::GND});

  auto systemEMT = SystemTopology(50, SystemNodeList{n1EMT},
                                  SystemComponentList{rl, cs, rcs});

  // Interface signal mapping
  auto inFromExternal_SeqExternalAttribute = CPS::AttributeStatic<Int>::make(0);
  auto inFromExternal_SeqDPsimAttribute = CPS::AttributeStatic<Int>::make(0);
  auto outToExternal_SeqDPsimAttribute = CPS::AttributeDynamic<Int>::make(0);

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

  intf->addImport(inFromExternal_SeqExternalAttribute, false, false);
  intf->addImport(inFromExternal_SeqDPsimAttribute, false, false);
  intf->addImport(cs->mCurrentRef->deriveCoeff<Real>(0, 0), false, false);
  intf->addImport(cs->mCurrentRef->deriveCoeff<Real>(1, 0), false, false);
  intf->addImport(cs->mCurrentRef->deriveCoeff<Real>(2, 0), false, false);

  intf->addExport(outToExternal_SeqDPsimAttribute);
  intf->addExport(inFromExternal_SeqExternalAttribute);
  intf->addExport(n1EMT->mVoltage->deriveCoeff<Real>(0, 0));
  intf->addExport(n1EMT->mVoltage->deriveCoeff<Real>(1, 0));
  intf->addExport(n1EMT->mVoltage->deriveCoeff<Real>(2, 0));

  if (logger) {
    logger->logAttribute("a", n1EMT->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b", n1EMT->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c", n1EMT->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("a_i", cs->mIntfCurrent->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b_i", cs->mIntfCurrent->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c_i", cs->mIntfCurrent->deriveCoeff<Real>(2, 0));
  }

  systemEMT.renderToFile("logs/" + simNameEMT + ".svg");
  return systemEMT;
}

int main(int argc, char *argv[]) {
  CommandLineArgs args(argc, argv, "EMT_3Ph_simple_cosim_leader_v2_sync", 0.01,
                       1 * 60, 50, -1, CPS::Logger::Level::info,
                       CPS::Logger::Level::off, false, false, false,
                       CPS::Domain::EMT);

  CPS::Logger::setLogDir("logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  // Data exchange via VILLAS shared memory
  auto villasIntf = std::make_shared<InterfaceVillasQueueless>(
      buildVillasConfig(args), "VillasInterface", spdlog::level::off);

  // New cosim sync interface (shared memory) for absolute start and config
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
    logger =
        RealTimeDataLogger::make(logFilename, args.duration, args.timeStep);
  }

  auto sys = buildTopology(args, villasIntf, logger);

  RealTimeSimulation sim(args.name, args);
  sim.setSystem(sys);
  sim.addInterface(villasIntf);
  sim.setDomain(Domain::EMT);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);
  // Default drop config; allow override via -o drop= and -o drop_threshold=
  bool dropEnabled = true;
  double dropThreshold = 0.95;
  if (args.options.find("drop") != args.options.end()) {
    try {
      dropEnabled = args.getOptionBool("drop");
    } catch (...) {
    }
  }
  if (args.options.find("drop_threshold") != args.options.end()) {
    try {
      dropThreshold = args.getOptionReal("drop_threshold");
    } catch (...) {
    }
  }
  sim.setDropEnabled(dropEnabled);
  sim.setDropThreshold(dropThreshold);
  if (logger)
    sim.addLogger(logger);
  if (args.options.find("threads") != args.options.end()) {
    auto numThreads = args.getOptionInt("threads");
    sim.setScheduler(std::make_shared<OpenMPLevelScheduler>(numThreads));
  }

  auto startAt = std::chrono::system_clock::now() + std::chrono::seconds(5);
  // Sanity check: start time must be in the future; warn if > 1h away
  {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto delta = startAt - now;
    double secs = duration_cast<duration<double>>(delta).count();
    if (secs < 0) {
      CPS::Logger::get(args.name)->error(
          "Start time is in the past by {} s. Aborting.", secs);
      return 1;
    }
    if (secs > 3600.0) {
      CPS::Logger::get(args.name)->warn(
          "Start time is more than 1 hour away ({} s).", secs);
    }
  }
  uint64_t dt_ns = static_cast<uint64_t>(args.timeStep * 1e9);
  uint64_t dur_ns = static_cast<uint64_t>(args.duration * 1e9);
  syncIntf->publishConfig(startAt, dt_ns, dur_ns);
  CPS::Logger::get(args.name)->info(
      "Published sync to {} (dt_ns={}, duration_ns={})", shmName, dt_ns,
      dur_ns);

  sim.run(startAt);

  sim.logStepTimes(args.name + "_step_times");
  sim.checkForOverruns(args.name + "_overruns");
  CPS::Logger::get(args.name)->info("Simulation finished.");
}
