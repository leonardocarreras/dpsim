#include <DPsim.h>

#include <dpsim-villas/InterfaceVillasQueueless.h>

using namespace DPsim;
using namespace CPS;

SystemTopology buildTopology(CommandLineArgs &args,
                             std::shared_ptr<DataLoggerInterface> logger) {

  String simName = args.name;

  String simNameEMT = simName + "_EMT";
  CPS::Logger::setLogDir("logs/" + simNameEMT);

  // Nodes
  auto n1EMT = SimNode<Real>::make("BUS1", PhaseType::ABC);

  auto vl = EMT::Ph3::VoltageSource::make("vl");
  vl->setParameters(
      CPS::Math::singlePhaseVariableToThreePhase(CPS::Math::polar(1000, 0)),
      50);

  auto rl = EMT::Ph3::Resistor::make("rl");
  rl->setParameters(CPS::Math::singlePhaseParameterToThreePhase(10));

  vl->connect({SimNode<Real>::GND, n1EMT});
  rl->connect({n1EMT, SimNode<Real>::GND});

  // Create system topology
  auto systemEMT =
      SystemTopology(50, // System frequency in Hz
                     SystemNodeList{n1EMT}, SystemComponentList{vl, rl});

  // Logger
  if (logger) {
    logger->logAttribute("a", n1EMT->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b", n1EMT->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c", n1EMT->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("a_i", rl->mIntfCurrent->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b_i", rl->mIntfCurrent->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c_i", rl->mIntfCurrent->deriveCoeff<Real>(2, 0));
  }

  systemEMT.renderToFile("logs/" + simNameEMT + ".svg");

  return systemEMT;
}

int main(int argc, char *argv[]) {
  CommandLineArgs args(argc, argv, "EMT_3Ph_simple_cosim_baseline", 0.01,
                       1 * 60, 50, -1, CPS::Logger::Level::info,
                       CPS::Logger::Level::off, false, false, false,
                       CPS::Domain::EMT);

  CPS::Logger::setLogDir("logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  std::filesystem::path logFilename =
      "logs/" + args.name + "/" + args.name + ".csv";
  std::shared_ptr<DataLoggerInterface> logger = nullptr;
  if (log) {
    logger =
        RealTimeDataLogger::make(logFilename, args.duration, args.timeStep);
  }

  auto sys = buildTopology(args, logger);

  RealTimeSimulation sim(args.name, args);
  sim.setSystem(sys);
  //sim.addInterface(intf);
  sim.setDomain(Domain::EMT);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);
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
  if (log) {
    sim.addLogger(logger);
  }
  if (args.options.find("threads") != args.options.end()) {
    auto numThreads = args.getOptionInt("threads");
    sim.setScheduler(std::make_shared<OpenMPLevelScheduler>(numThreads));
  }

  auto startIn = std::chrono::seconds(5);
  sim.run(startIn);

  sim.logStepTimes(args.name + "_step_times");
  sim.checkForOverruns(args.name + "_overruns");
  CPS::Logger::get(args.name)->info("Simulation finished.");
}
