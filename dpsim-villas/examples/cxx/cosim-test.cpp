#include <DPsim.h>

#include <dpsim-villas/InterfaceVillasQueueless.h>

using namespace DPsim;
using namespace CPS;

const std::string buildFpgaConfig(CommandLineArgs &args) {
  std::filesystem::path fpgaIpPath =
      "/usr/local/etc/villas/node/etc/fpga/vc707-xbar-pcie/"
      "vc707-xbar-pcie.json";
  ;

  if (args.options.find("ips") != args.options.end()) {
    fpgaIpPath = std::filesystem::path(args.getOptionString("ips"));
  }
  std::string cardConfig = fmt::format(
      R"STRING("card": {{
      "interface": "pcie",
      "id": "10ee:7021",
      "slot": "0000:89:00.0",
      "do_reset": true,
      "ips": "{}",
      "polling": true
    }},
    "connect": [
      "0<->1",
      "2<->dma"
    ],
    "low_latency_mode": true,
    "timestep": {})STRING",
      fpgaIpPath.string(), args.timeStep);
  std::string signalOutConfig = fmt::format(
      R"STRING("out": {{
      "signals": [{{
        "name": "seq_to_dpsim",
        "type": "integer",
        "unit": "",
        "builtin": false
      }},
      {{
        "name": "seq_to_rtds",
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
      }},
      {{
        "name": "seqnum",
        "type": "integer",
        "unit": "",
        "builtin": false
      }}]
    }})STRING");
  std::string signalInConfig = fmt::format(
      R"STRING("in": {{
      "signals": [{{
        "name": "seq_from_dpsim",
        "type": "integer",
        "unit": "",
        "builtin": false
      }},
      {{
        "name": "seq_from_rtds",
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
      }},
      {{
        "name": "ramp_seq",
        "type": "integer",
        "unit": "",
        "builtin": false
      }}]
    }})STRING");
  const std::string config = fmt::format(
      R"STRING({{
    "type": "fpga",
    {},
    {},
    {}
  }})STRING",
      cardConfig, signalOutConfig, signalInConfig);
  DPsim::Logger::get("cosim-test")->debug("Config for Node:\n{}", config);
  return config;
}

SystemTopology buildTopology(CommandLineArgs &args,
                             std::shared_ptr<Interface> intfFpga,
                             std::shared_ptr<DataLoggerInterface> logger) {

  String simName = "cosim-test";

  String simNameEMT = simName + "_EMT";
  CPS::Logger::setLogDir("logs/" + simNameEMT);

  // Nodes
  auto n1EMT = SimNode<Real>::make("BUS1", PhaseType::ABC);

  auto vs = EMT::Ph3::VoltageSource::make("vs");
  vs->setParameters(
      CPS::Math::singlePhaseVariableToThreePhase(CPS::Math::polar(1000, 0)),
      50);
  auto cs = EMT::Ph3::ControlledCurrentSource::make("cs");
  auto rcs = EMT::Ph3::Resistor::make("rcs");
  rcs->setParameters(CPS::Math::singlePhaseParameterToThreePhase(1e8));

  cs->connect({n1EMT, SimNode<Real>::GND});
  vs->connect({n1EMT, SimNode<Real>::GND});
  rcs->connect({n1EMT, SimNode<Real>::GND});
  // Create system topology
  auto systemEMT =
      SystemTopology(50, // System frequency in Hz
                     SystemNodeList{n1EMT}, SystemComponentList{cs, vs, rcs});

  // Interface
  auto inFromRTDS_SeqRTDSAttribute = CPS::AttributeStatic<Int>::make(0);
  auto inFromRTDS_SeqDPsimAttribute = CPS::AttributeStatic<Int>::make(0);
  auto inFromRTDS_rampSeqAttribute = CPS::AttributeStatic<Int>::make(0);

  auto outToRTDS_SeqDPsimAttribute = CPS::AttributeDynamic<Int>::make(0);

  auto updateFn = std::make_shared<CPS::AttributeUpdateTask<Int, Int>::Actor>(
      [](std::shared_ptr<Int> &dependent,
         typename CPS::Attribute<Int>::Ptr dependency) {
        *dependent = *dependent + 1;
      });
  outToRTDS_SeqDPsimAttribute->addTask(CPS::UpdateTaskKind::UPDATE_ON_GET,
                                       CPS::AttributeUpdateTask<Int, Int>::make(
                                           CPS::UpdateTaskKind::UPDATE_ON_GET,
                                           *updateFn,
                                           inFromRTDS_SeqDPsimAttribute));

  intfFpga->addImport(inFromRTDS_SeqRTDSAttribute, true, true);
  intfFpga->addImport(inFromRTDS_SeqDPsimAttribute, true, true);
  intfFpga->addImport(cs->mCurrentRef->deriveCoeff<Real>(0, 0), true, true);
  intfFpga->addImport(cs->mCurrentRef->deriveCoeff<Real>(1, 0), true, true);
  intfFpga->addImport(cs->mCurrentRef->deriveCoeff<Real>(2, 0), true, true);
  intfFpga->addImport(inFromRTDS_rampSeqAttribute, true, true);

  intfFpga->addExport(outToRTDS_SeqDPsimAttribute);
  intfFpga->addExport(inFromRTDS_SeqRTDSAttribute);
  intfFpga->addExport(n1EMT->mVoltage->deriveCoeff<Real>(0, 0));
  intfFpga->addExport(n1EMT->mVoltage->deriveCoeff<Real>(1, 0));
  intfFpga->addExport(n1EMT->mVoltage->deriveCoeff<Real>(2, 0));
  //intfFpga->addExport(seqNumForRTDS);
  //intfFpga->addExport(seqSimAttribute);
  // auto seqSimAttribute = CPS::AttributeDynamic<int>::make(0);

  // auto updateStairFn =
  //   std::make_shared<CPS::AttributeUpdateTask<int,int>::Actor>(
  //     [](std::shared_ptr<int>& dependent,
  //        CPS::Attribute<Int>::Ptr) {
  //       *dependent = (*dependent + 1) % 11;
  //     }
  //   );

  // seqSimAttribute->addTask(
  //   CPS::UpdateTaskKind::UPDATE_ON_SET,
  //   CPS::AttributeUpdateTask<int,int>::make(
  //     CPS::UpdateTaskKind::UPDATE_ON_SET,
  //     *updateStairFn,
  //     seqFromDPsimAttribute
  //   )
  // );

  // Logger
  if (logger) {
    logger->logAttribute("a", n1EMT->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b", n1EMT->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c", n1EMT->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("a_i", cs->mIntfCurrent->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b_i", cs->mIntfCurrent->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c_i", cs->mIntfCurrent->deriveCoeff<Real>(2, 0));
    logger->logAttribute("RTDS_Seq", inFromRTDS_rampSeqAttribute);
  }

  systemEMT.renderToFile("logs/" + simNameEMT + ".svg");

  return systemEMT;
}

int main(int argc, char *argv[]) {
  CommandLineArgs args(argc, argv, "cosim-test", 0.01, 10 * 60, 50, -1,
                       CPS::Logger::Level::info, CPS::Logger::Level::off, false,
                       false, false, CPS::Domain::EMT);

  CPS::Logger::setLogDir("logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  auto intfFpga = std::make_shared<InterfaceVillasQueueless>(
      buildFpgaConfig(args), "FpgaInterface", spdlog::level::off);

  std::filesystem::path logFilename =
      "logs/" + args.name + "/" + args.name + ".csv";
  std::shared_ptr<DataLoggerInterface> logger = nullptr;
  if (log) {
    logger =
        RealTimeDataLogger::make(logFilename, args.duration, args.timeStep);
  }

  auto sys = buildTopology(args, intfFpga, logger);

  Simulation sim(args.name, args);
  sim.setSystem(sys);
  sim.addInterface(intfFpga);
  sim.setDomain(Domain::EMT);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);
  sim.checkForOverruns(args.name + "_overruns");
  if (log) {
    sim.addLogger(logger);
  }
  if (args.options.find("threads") != args.options.end()) {
    auto numThreads = args.getOptionInt("threads");
    sim.setScheduler(std::make_shared<OpenMPLevelScheduler>(numThreads));
  }
  sim.run();

  sim.logStepTimes(args.name + "_step_times");
  CPS::Logger::get("cosim-test")->info("Simulation finished.");

  // std::ofstream of("task_dependencies.svg");
  // sim.dependencyGraph().render(of);
}
