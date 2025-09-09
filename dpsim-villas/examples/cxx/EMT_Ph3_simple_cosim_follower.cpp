#include <DPsim.h>

#include <dpsim-villas/InterfaceVillasQueueless.h>

using namespace DPsim;
using namespace CPS;

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
  DPsim::Logger::get("simple_cosim_follower")
      ->info("Config for Node:\n{}", config);
  return config;
}

SystemTopology buildTopology(CommandLineArgs &args,
                             std::shared_ptr<Interface> intf,
                             std::shared_ptr<DataLoggerInterface> logger) {

  String simName = args.name;

  String simNameEMT = simName + "_EMT";
  CPS::Logger::setLogDir("logs/" + simNameEMT);

  // Nodes
  auto n1EMT = SimNode<Real>::make("BUS1", PhaseType::ABC);

  auto rl = EMT::Ph3::Resistor::make("rl");
  rl->setParameters(CPS::Math::singlePhaseParameterToThreePhase(10));

  auto vs = EMT::Ph3::ControlledVoltageSource::make("vs");
  vs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0));
  auto rvs = EMT::Ph3::Resistor::make("rvs");
  rvs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0.1));
  auto ivs = EMT::Ph3::Inductor::make("ivs");
  ivs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(0.01));

  rl->connect({n1EMT, SimNode<Real>::GND});

  vs->connect({n1EMT, SimNode<Real>::GND});
  rvs->connect({n1EMT, SimNode<Real>::GND});
  ivs->connect({n1EMT, SimNode<Real>::GND});
  // Create system topology
  auto systemEMT = SystemTopology(50, // System frequency in Hz
                                  SystemNodeList{n1EMT},
                                  SystemComponentList{rl, vs, rvs, ivs});

  // Interface
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

  intf->addImport(inFromExternal_SeqExternalAttribute, true, true);
  intf->addImport(inFromExternal_SeqDPsimAttribute, true, true);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(0, 0), true, true);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(1, 0), true, true);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(2, 0), true, true);

  intf->addExport(outToExternal_SeqDPsimAttribute);
  intf->addExport(inFromExternal_SeqExternalAttribute);
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(0, 0));
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(1, 0));
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(2, 0));

  // Logger
  if (logger) {
    logger->logAttribute("a", n1EMT->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b", n1EMT->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c", n1EMT->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("a_i", rvs->mIntfCurrent->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b_i", rvs->mIntfCurrent->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c_i", rvs->mIntfCurrent->deriveCoeff<Real>(2, 0));
  }

  systemEMT.renderToFile("logs/" + simNameEMT + ".svg");

  return systemEMT;
}

int main(int argc, char *argv[]) {
  CommandLineArgs args(argc, argv, "EMT_3Ph_simple_cosim_follower", 0.01,
                       1 * 60, 50, -1, CPS::Logger::Level::info,
                       CPS::Logger::Level::off, false, false, false,
                       CPS::Domain::EMT);

  CPS::Logger::setLogDir("logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  auto intf = std::make_shared<InterfaceVillasQueueless>(
      buildVillasConfig(args), "VillasInterface", spdlog::level::off);

  std::filesystem::path logFilename =
      "logs/" + args.name + "/" + args.name + ".csv";
  std::shared_ptr<DataLoggerInterface> logger = nullptr;
  if (log) {
    logger =
        RealTimeDataLogger::make(logFilename, args.duration, args.timeStep);
  }

  auto sys = buildTopology(args, intf, logger);

  RealTimeSimulation sim(args.name, args);
  //Simulation sim(args.name, args);
  sim.setSystem(sys);
  sim.addInterface(intf);
  sim.setDomain(Domain::EMT);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);
  if (log) {
    sim.addLogger(logger);
  }
  if (args.options.find("threads") != args.options.end()) {
    auto numThreads = args.getOptionInt("threads");
    sim.setScheduler(std::make_shared<OpenMPLevelScheduler>(numThreads));
  }
  auto startIn = std::chrono::seconds(5);
  sim.run(startIn);
  //sim.run();

  sim.logStepTimes(args.name + "_step_times");
  sim.checkForOverruns(args.name + "_overruns");
  CPS::Logger::get(args.name)->info("Simulation finished.");
}
