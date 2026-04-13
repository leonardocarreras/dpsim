#include "../../dpsim/examples/cxx/Examples.h"
#include "../../dpsim/examples/cxx/GeneratorFactory.h"

#include <DPsim.h>

#include <dpsim-villas/InterfaceVillasQueueless.h>
#include <dpsim/InterfaceCosimSyncShmem.h>

using namespace DPsim;
using namespace CPS;

CPS::CIM::Examples::Grids::NineBus::ScenarioConfig ninebus;

const std::string buildShmemFollowerConfig() {
  std::string signalOutConfig = fmt::format(
      R"STRING("out": {{
      "name": "/dpsim.dp-follower-to-bridge",
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
        "type": "complex",
        "unit": "V",
        "builtin": false
      }},
      {{
        "name": "VB",
        "type": "complex",
        "unit": "V",
        "builtin": false
      }},
      {{
        "name": "VC",
        "type": "complex",
        "unit": "V",
        "builtin": false
      }}]
    }})STRING");

  std::string signalInConfig = fmt::format(
      R"STRING("in": {{
      "name": "/dpsim.bridge-to-dp-follower",
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
        "type": "complex",
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
  DPsim::Logger::get("DP_Ph1_9bus_4order_cosim_follower")
      ->info("Config for Node:\n{}", config);
  return config;
}

SystemTopology buildTopology(CommandLineArgs &args,
                             std::shared_ptr<Interface> intf,
                             std::shared_ptr<DataLoggerInterface> logger) {

  String simName = args.name;

  // ----- POWER FLOW FOR INITIALIZATION -----
  String simNamePF = simName + "_PF";
  CPS::Logger::setLogDir("logs/" + simNamePF);

  auto n1PF = SimNode<Complex>::make("BUS1", PhaseType::Single);
  auto n2PF = SimNode<Complex>::make("BUS2", PhaseType::Single);
  auto n3PF = SimNode<Complex>::make("BUS3", PhaseType::Single);
  auto n4PF = SimNode<Complex>::make("BUS4", PhaseType::Single);
  auto n5PF = SimNode<Complex>::make("BUS5", PhaseType::Single);
  auto n6PF = SimNode<Complex>::make("BUS6", PhaseType::Single);
  auto n7PF = SimNode<Complex>::make("BUS7", PhaseType::Single);
  auto n8PF = SimNode<Complex>::make("BUS8", PhaseType::Single);
  auto n9PF = SimNode<Complex>::make("BUS9", PhaseType::Single);

  auto gen1PF = SP::Ph1::SynchronGenerator::make(ninebus.gen1.Name,
                                                 CPS::Logger::Level::off);
  gen1PF->setParameters(ninebus.gen1.RatedPower, ninebus.gen1.RatedVoltage,
                        ninebus.gen1.InitialPower, ninebus.gen1.InitialVoltage,
                        ninebus.gen1.BusType);
  gen1PF->setBaseVoltage(ninebus.gen1.RatedVoltage);

  auto gen2PF = SP::Ph1::SynchronGenerator::make(ninebus.gen2.Name,
                                                 CPS::Logger::Level::off);
  gen2PF->setParameters(ninebus.gen2.RatedPower, ninebus.gen2.RatedVoltage,
                        ninebus.gen2.InitialPower, ninebus.gen2.InitialVoltage,
                        ninebus.gen2.BusType);
  gen2PF->setBaseVoltage(ninebus.gen2.RatedVoltage);

  auto gen3PF = SP::Ph1::SynchronGenerator::make(ninebus.gen3.Name,
                                                 CPS::Logger::Level::off);
  gen3PF->setParameters(ninebus.gen3.RatedPower, ninebus.gen3.RatedVoltage,
                        ninebus.gen3.InitialPower, ninebus.gen3.InitialVoltage,
                        ninebus.gen3.BusType);
  gen3PF->setBaseVoltage(ninebus.gen3.RatedVoltage);

  auto load5PF =
      SP::Ph1::Load::make(ninebus.load5.Name, CPS::Logger::Level::off);
  load5PF->setParameters(ninebus.load5.RealPower, ninebus.load5.ReactivePower,
                         ninebus.load5.BaseVoltage);
  load5PF->modifyPowerFlowBusType(PowerflowBusType::PQ);

  auto load6PF =
      SP::Ph1::Load::make(ninebus.load6.Name, CPS::Logger::Level::off);
  load6PF->setParameters(ninebus.load6.RealPower, ninebus.load6.ReactivePower,
                         ninebus.load6.BaseVoltage);
  load6PF->modifyPowerFlowBusType(PowerflowBusType::PQ);

  auto load8PF =
      SP::Ph1::Load::make(ninebus.load8.Name, CPS::Logger::Level::off);
  load8PF->setParameters(ninebus.load8.RealPower, ninebus.load8.ReactivePower,
                         ninebus.load8.BaseVoltage);
  load8PF->modifyPowerFlowBusType(PowerflowBusType::PQ);

  auto line54PF =
      SP::Ph1::PiLine::make(ninebus.line54.Name, CPS::Logger::Level::off);
  line54PF->setParameters(ninebus.line54.Resistance, ninebus.line54.Inductance,
                          ninebus.line54.Capacitance,
                          ninebus.line54.Conductance);
  line54PF->setBaseVoltage(ninebus.line54.BaseVoltage);

  auto line64PF =
      SP::Ph1::PiLine::make(ninebus.line64.Name, CPS::Logger::Level::off);
  line64PF->setParameters(ninebus.line64.Resistance, ninebus.line64.Inductance,
                          ninebus.line64.Capacitance,
                          ninebus.line64.Conductance);
  line64PF->setBaseVoltage(ninebus.line64.BaseVoltage);

  auto line75PF =
      SP::Ph1::PiLine::make(ninebus.line75.Name, CPS::Logger::Level::off);
  line75PF->setParameters(ninebus.line75.Resistance, ninebus.line75.Inductance,
                          ninebus.line75.Capacitance,
                          ninebus.line75.Conductance);
  line75PF->setBaseVoltage(ninebus.line75.BaseVoltage);

  auto line96PF =
      SP::Ph1::PiLine::make(ninebus.line96.Name, CPS::Logger::Level::off);
  line96PF->setParameters(ninebus.line96.Resistance, ninebus.line96.Inductance,
                          ninebus.line96.Capacitance,
                          ninebus.line96.Conductance);
  line96PF->setBaseVoltage(ninebus.line96.BaseVoltage);

  auto line78PF =
      SP::Ph1::PiLine::make(ninebus.line78.Name, CPS::Logger::Level::off);
  line78PF->setParameters(ninebus.line78.Resistance, ninebus.line78.Inductance,
                          ninebus.line78.Capacitance,
                          ninebus.line78.Conductance);
  line78PF->setBaseVoltage(ninebus.line78.BaseVoltage);

  auto line89PF =
      SP::Ph1::PiLine::make(ninebus.line89.Name, CPS::Logger::Level::off);
  line89PF->setParameters(ninebus.line89.Resistance, ninebus.line89.Inductance,
                          ninebus.line89.Capacitance,
                          ninebus.line89.Conductance);
  line89PF->setBaseVoltage(ninebus.line89.BaseVoltage);

  auto transf14PF = SP::Ph1::Transformer::make(ninebus.transf14.Name,
                                               CPS::Logger::Level::off);
  transf14PF->setParameters(
      ninebus.transf14.VoltageLVSide, ninebus.transf14.VoltageHVSide,
      ninebus.transf14.Ratio, 0.0, ninebus.transf14.Resistance,
      ninebus.transf14.Inductance);
  transf14PF->setBaseVoltage(ninebus.transf14.VoltageHVSide);

  auto transf27PF = SP::Ph1::Transformer::make(ninebus.transf27.Name,
                                               CPS::Logger::Level::off);
  transf27PF->setParameters(
      ninebus.transf27.VoltageLVSide, ninebus.transf27.VoltageHVSide,
      ninebus.transf27.Ratio, 0.0, ninebus.transf27.Resistance,
      ninebus.transf27.Inductance);
  transf27PF->setBaseVoltage(ninebus.transf27.VoltageHVSide);

  auto transf39PF = SP::Ph1::Transformer::make(ninebus.transf39.Name,
                                               CPS::Logger::Level::off);
  transf39PF->setParameters(
      ninebus.transf39.VoltageLVSide, ninebus.transf39.VoltageHVSide,
      ninebus.transf39.Ratio, 0.0, ninebus.transf39.Resistance,
      ninebus.transf39.Inductance);
  transf39PF->setBaseVoltage(ninebus.transf39.VoltageHVSide);

  gen1PF->connect({n1PF});
  gen2PF->connect({n2PF});
  gen3PF->connect({n3PF});

  load5PF->connect({n5PF});
  load6PF->connect({n6PF});
  load8PF->connect({n8PF});

  line54PF->connect({n5PF, n4PF});
  line64PF->connect({n6PF, n4PF});
  line75PF->connect({n7PF, n5PF});
  line96PF->connect({n9PF, n6PF});
  line78PF->connect({n7PF, n8PF});
  line89PF->connect({n8PF, n9PF});

  transf14PF->connect({n1PF, n4PF});
  transf27PF->connect({n2PF, n7PF});
  transf39PF->connect({n3PF, n9PF});

  auto systemPF = SystemTopology(
      ninebus.nomFreq,
      SystemNodeList{n1PF, n2PF, n3PF, n4PF, n5PF, n6PF, n7PF, n8PF, n9PF},
      SystemComponentList{gen1PF, gen2PF, gen3PF, load5PF, load6PF, load8PF,
                          line54PF, line64PF, line75PF, line96PF, line78PF,
                          line89PF, transf14PF, transf27PF, transf39PF});

  systemPF.renderToFile("logs/" + simNamePF + ".svg");

  auto loggerPF = DataLogger::make(simNamePF, CPS::Logger::Level::off);
  loggerPF->logAttribute("v_bus1", n1PF->attribute("v"));
  loggerPF->logAttribute("v_bus2", n2PF->attribute("v"));
  loggerPF->logAttribute("v_bus3", n3PF->attribute("v"));
  loggerPF->logAttribute("v_bus4", n4PF->attribute("v"));
  loggerPF->logAttribute("v_bus5", n5PF->attribute("v"));
  loggerPF->logAttribute("v_bus6", n6PF->attribute("v"));
  loggerPF->logAttribute("v_bus7", n7PF->attribute("v"));
  loggerPF->logAttribute("v_bus8", n8PF->attribute("v"));
  loggerPF->logAttribute("v_bus9", n9PF->attribute("v"));
  loggerPF->logAttribute("s_bus1", n1PF->attribute("s"));
  loggerPF->logAttribute("s_bus2", n2PF->attribute("s"));
  loggerPF->logAttribute("s_bus3", n3PF->attribute("s"));
  loggerPF->logAttribute("s_bus4", n4PF->attribute("s"));
  loggerPF->logAttribute("s_bus5", n5PF->attribute("s"));
  loggerPF->logAttribute("s_bus6", n6PF->attribute("s"));
  loggerPF->logAttribute("s_bus7", n7PF->attribute("s"));
  loggerPF->logAttribute("s_bus8", n8PF->attribute("s"));
  loggerPF->logAttribute("s_bus9", n9PF->attribute("s"));

  Simulation simPF(simNamePF, CPS::Logger::Level::off);
  simPF.setSystem(systemPF);
  simPF.setTimeStep(args.timeStep);
  simPF.setFinalTime(1 * args.timeStep);
  simPF.setDomain(Domain::SP);
  simPF.setSolverType(Solver::Type::NRP);
  simPF.setSolverAndComponentBehaviour(Solver::Behaviour::Simulation);
  simPF.addLogger(loggerPF);
  simPF.run();

  // ----- DYNAMIC SIMULATION - DP -----
  String simNameDP = simName + "_DP";
  CPS::Logger::setLogDir("logs/" + simNameDP);

  auto n1DP = SimNode<Complex>::make("BUS1", PhaseType::Single);
  auto n2DP = SimNode<Complex>::make("BUS2", PhaseType::Single);
  auto n3DP = SimNode<Complex>::make("BUS3", PhaseType::Single);
  auto n4DP = SimNode<Complex>::make("BUS4", PhaseType::Single);
  auto n5DP = SimNode<Complex>::make("BUS5", PhaseType::Single);
  auto n6DP = SimNode<Complex>::make("BUS6", PhaseType::Single);
  auto n7DP = SimNode<Complex>::make("BUS7", PhaseType::Single);
  auto n8DP = SimNode<Complex>::make("BUS8", PhaseType::Single);
  auto n9DP = SimNode<Complex>::make("BUS9", PhaseType::Single);

  auto gen1DP = DP::Ph1::SynchronGenerator4OrderVBR::make(
      ninebus.gen1.Name, CPS::Logger::Level::off);
  gen1DP->setOperationalParametersPerUnit(
      ninebus.gen1.RatedPower, ninebus.gen1.RatedVoltage, ninebus.nomFreq,
      ninebus.gen1.H, ninebus.gen1.Xd, ninebus.gen1.Xq, ninebus.gen1.Xa,
      ninebus.gen1.XdPrime, ninebus.gen1.XqPrime, ninebus.gen1.TdoPrime,
      ninebus.gen1.TqoPrime);

  auto exciter1 =
      Signal::Exciter::make("Gen1_Exciter", CPS::Logger::Level::off);
  exciter1->setParameters(ninebus.exc1.TA, ninebus.exc1.KA, ninebus.exc1.TE,
                          ninebus.exc1.KE, ninebus.exc1.TF, ninebus.exc1.KF,
                          0.01, ninebus.exc1.VRmax, ninebus.exc1.VRmin);
  gen1DP->addExciter(exciter1);

  CPS::Real T4 = 1.0;
  CPS::Real T5 = 1.0;
  auto turbineGovernor1 = Signal::TurbineGovernorType1::make(
      "Gen1_TurbineGovernor", CPS::Logger::Level::off);
  turbineGovernor1->setParameters(ninebus.gov1.T2, T4, T5, ninebus.gov1.T3,
                                  ninebus.gov1.T1, ninebus.gov1.R,
                                  ninebus.gov1.Vmin, ninebus.gov1.Vmax, 1.0);
  gen1DP->addGovernor(turbineGovernor1);

  auto gen2DP = DP::Ph1::SynchronGenerator4OrderVBR::make(
      ninebus.gen2.Name, CPS::Logger::Level::off);
  gen2DP->setOperationalParametersPerUnit(
      ninebus.gen2.RatedPower, ninebus.gen2.RatedVoltage, ninebus.nomFreq,
      ninebus.gen2.H, ninebus.gen2.Xd, ninebus.gen2.Xq, ninebus.gen2.Xa,
      ninebus.gen2.XdPrime, ninebus.gen2.XqPrime, ninebus.gen2.TdoPrime,
      ninebus.gen2.TqoPrime);

  auto exciter2 =
      Signal::Exciter::make("Gen2_Exciter", CPS::Logger::Level::off);
  exciter2->setParameters(ninebus.exc2.TA, ninebus.exc2.KA, ninebus.exc2.TE,
                          ninebus.exc2.KE, ninebus.exc2.TF, ninebus.exc2.KF,
                          0.01, ninebus.exc2.VRmax, ninebus.exc2.VRmin);
  gen2DP->addExciter(exciter2);

  auto turbineGovernor2 = Signal::TurbineGovernorType1::make(
      "Gen2_TurbineGovernor", CPS::Logger::Level::off);
  turbineGovernor2->setParameters(ninebus.gov2.T2, T4, T5, ninebus.gov2.T3,
                                  ninebus.gov2.T1, ninebus.gov2.R,
                                  ninebus.gov2.Vmin, ninebus.gov2.Vmax, 1.0);
  gen2DP->addGovernor(turbineGovernor2);

  auto gen3DP = DP::Ph1::SynchronGenerator4OrderVBR::make(
      ninebus.gen3.Name, CPS::Logger::Level::off);
  gen3DP->setOperationalParametersPerUnit(
      ninebus.gen3.RatedPower, ninebus.gen3.RatedVoltage, ninebus.nomFreq,
      ninebus.gen3.H, ninebus.gen3.Xd, ninebus.gen3.Xq, ninebus.gen3.Xa,
      ninebus.gen3.XdPrime, ninebus.gen3.XqPrime, ninebus.gen3.TdoPrime,
      ninebus.gen3.TqoPrime);

  auto exciter3 =
      Signal::Exciter::make("Gen3_Exciter", CPS::Logger::Level::off);
  exciter3->setParameters(ninebus.exc3.TA, ninebus.exc3.KA, ninebus.exc3.TE,
                          ninebus.exc3.KE, ninebus.exc3.TF, ninebus.exc3.KF,
                          0.01, ninebus.exc3.VRmax, ninebus.exc3.VRmin);
  gen3DP->addExciter(exciter3);

  auto turbineGovernor3 = Signal::TurbineGovernorType1::make(
      "Gen3_TurbineGovernor", CPS::Logger::Level::off);
  turbineGovernor3->setParameters(ninebus.gov3.T2, T4, T5, ninebus.gov3.T3,
                                  ninebus.gov3.T1, ninebus.gov3.R,
                                  ninebus.gov3.Vmin, ninebus.gov3.Vmax, 1.0);
  gen3DP->addGovernor(turbineGovernor3);

  auto load5DP =
      DP::Ph1::RXLoad::make(ninebus.load5.Name, CPS::Logger::Level::off);
  load5DP->setParameters(ninebus.load5.RealPower, ninebus.load5.ReactivePower,
                         ninebus.load5.BaseVoltage);

  auto load6DP =
      DP::Ph1::RXLoad::make(ninebus.load6.Name, CPS::Logger::Level::off);
  load6DP->setParameters(ninebus.load6.RealPower, ninebus.load6.ReactivePower,
                         ninebus.load6.BaseVoltage);

  auto load8DP =
      DP::Ph1::RXLoad::make(ninebus.load8.Name, CPS::Logger::Level::off);
  load8DP->setParameters(ninebus.load8.RealPower, ninebus.load8.ReactivePower,
                         ninebus.load8.BaseVoltage);

  auto line54DP =
      DP::Ph1::PiLine::make(ninebus.line54.Name, CPS::Logger::Level::off);
  line54DP->setParameters(ninebus.line54.Resistance, ninebus.line54.Inductance,
                          ninebus.line54.Capacitance,
                          ninebus.line54.Conductance);

  auto line64DP =
      DP::Ph1::PiLine::make(ninebus.line64.Name, CPS::Logger::Level::off);
  line64DP->setParameters(ninebus.line64.Resistance, ninebus.line64.Inductance,
                          ninebus.line64.Capacitance,
                          ninebus.line64.Conductance);

  auto line75DP =
      DP::Ph1::PiLine::make(ninebus.line75.Name, CPS::Logger::Level::off);
  line75DP->setParameters(ninebus.line75.Resistance, ninebus.line75.Inductance,
                          ninebus.line75.Capacitance,
                          ninebus.line75.Conductance);

  auto line96DP =
      DP::Ph1::PiLine::make(ninebus.line96.Name, CPS::Logger::Level::off);
  line96DP->setParameters(ninebus.line96.Resistance, ninebus.line96.Inductance,
                          ninebus.line96.Capacitance,
                          ninebus.line96.Conductance);

  auto line78DP =
      DP::Ph1::PiLine::make(ninebus.line78.Name, CPS::Logger::Level::off);
  line78DP->setParameters(ninebus.line78.Resistance, ninebus.line78.Inductance,
                          ninebus.line78.Capacitance,
                          ninebus.line78.Conductance);

  auto line89DP =
      DP::Ph1::PiLine::make(ninebus.line89.Name, CPS::Logger::Level::off);
  line89DP->setParameters(ninebus.line89.Resistance, ninebus.line89.Inductance,
                          ninebus.line89.Capacitance,
                          ninebus.line89.Conductance);

  auto transf14DP = DP::Ph1::Transformer::make(ninebus.transf14.Name,
                                               CPS::Logger::Level::off);
  transf14DP->setParameters(
      ninebus.transf14.VoltageLVSide, ninebus.transf14.VoltageHVSide,
      ninebus.transf14.RatedPower, ninebus.transf14.Ratio, 0.0,
      ninebus.transf14.Resistance, ninebus.transf14.Inductance);

  auto transf27DP = DP::Ph1::Transformer::make(ninebus.transf27.Name,
                                               CPS::Logger::Level::off);
  transf27DP->setParameters(
      ninebus.transf27.VoltageLVSide, ninebus.transf27.VoltageHVSide,
      ninebus.transf14.RatedPower, ninebus.transf27.Ratio, 0.0,
      ninebus.transf27.Resistance, ninebus.transf27.Inductance);

  auto transf39DP = DP::Ph1::Transformer::make(ninebus.transf39.Name,
                                               CPS::Logger::Level::off);
  transf39DP->setParameters(
      ninebus.transf39.VoltageLVSide, ninebus.transf39.VoltageHVSide,
      ninebus.transf14.RatedPower, ninebus.transf39.Ratio, 0.0,
      ninebus.transf39.Resistance, ninebus.transf39.Inductance);

  gen1DP->connect({n1DP});
  gen2DP->connect({n2DP});
  gen3DP->connect({n3DP});

  load5DP->connect({n5DP});
  load6DP->connect({n6DP});
  load8DP->connect({n8DP});

  line54DP->connect({n5DP, n4DP});
  line64DP->connect({n6DP, n4DP});
  line75DP->connect({n7DP, n5DP});
  line96DP->connect({n9DP, n6DP});
  line78DP->connect({n7DP, n8DP});
  line89DP->connect({n8DP, n9DP});

  transf14DP->connect({n1DP, n4DP});
  transf27DP->connect({n2DP, n7DP});
  transf39DP->connect({n3DP, n9DP});

  auto systemDP = SystemTopology(
      ninebus.nomFreq,
      SystemNodeList{n1DP, n2DP, n3DP, n4DP, n5DP, n6DP, n7DP, n8DP, n9DP},
      SystemComponentList{gen1DP, gen2DP, gen3DP, load5DP, load6DP, load8DP,
                          line54DP, line64DP, line75DP, line96DP, line78DP,
                          line89DP, transf14DP, transf27DP, transf39DP});

  systemDP.initWithPowerflow(systemPF, Domain::DP);

  auto cs = DP::Ph1::CurrentSource::make("cs");
  cs->setParameters(Complex(0, 0));
  cs->connect({n6DP, DP::SimNode::GND});
  cs->initializeFromNodesAndTerminals(ninebus.nomFreq);
  systemDP.addComponent(cs);

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
  intf->addImport(cs->mCurrentRef, true, true);

  intf->addExport(outToExternal_SeqDPsimAttribute);
  intf->addExport(inFromExternal_SeqExternalAttribute);
  intf->addExport(n6DP->mVoltage->deriveCoeff<Complex>(0, 0));
  intf->addExport(
      n6DP->mVoltage->deriveCoeff<Complex>(0, 0)->deriveScaled(SHIFT_TO_PHASE_B));
  intf->addExport(
      n6DP->mVoltage->deriveCoeff<Complex>(0, 0)->deriveScaled(SHIFT_TO_PHASE_C));

  if (logger) {
    logger->logAttribute("BUS6", n6DP->attribute("v"));
    logger->logAttribute("BUS6_A", n6DP->mVoltage->deriveCoeff<Complex>(0, 0));
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

  systemDP.removeComponent(ninebus.load6.Name);
  systemDP.renderToFile("logs/" + simNameDP + ".svg");

  return systemDP;
}

int main(int argc, char *argv[]) {
  CommandLineArgs args(argc, argv, "DP_Ph1_9bus_4order_cosim_follower",
                       0.00005, 1 * 60, ninebus.nomFreq, -1,
                       CPS::Logger::Level::info, CPS::Logger::Level::off,
                       false, false, false, CPS::Domain::DP);

  std::error_code ec;
  std::filesystem::create_directories("./logs", ec);

  CPS::Logger::setLogDir("./logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  auto villasIntf = std::make_shared<InterfaceVillasQueueless>(
      buildShmemFollowerConfig(), "ShmemInterface", spdlog::level::off);

  std::string shmName = "/dpsim_sync_case";
  if (const char *env = std::getenv("DPSIM_SYNC_SHM")) {
    shmName = env;
  }
  if (args.options.find("shm") != args.options.end()) {
    shmName = args.getOptionString("shm");
  }

  auto syncIntf = std::make_shared<InterfaceCosimSyncShmem>(
      "CosimSync", shmName, InterfaceCosimSyncShmem::Role::Follower);
  syncIntf->open();

  InterfaceCosimSyncShmem::ConfigNs cfg;
  bool ok = syncIntf->waitForConfig(cfg, 10000);
  if (!ok) {
    CPS::Logger::get(args.name)->error("Timed out waiting for leader config");
    return 1;
  }

  args.timeStep = static_cast<double>(cfg.time_step_ns) / 1e9;
  args.duration = static_cast<double>(cfg.duration_ns) / 1e9;
  auto startAt = InterfaceCosimSyncShmem::toTimePoint(cfg.start_time_ns);
  CPS::Logger::get(args.name)->info(
      "Received sync from {} (dt_ns={}, duration_ns={})", shmName,
      cfg.time_step_ns, cfg.duration_ns);

  std::filesystem::path logFilename = "./logs/" + args.name + ".csv";
  std::shared_ptr<DataLoggerInterface> logger = nullptr;
  if (log) {
    logger =
        RealTimeDataLogger::make(logFilename, args.duration, args.timeStep);
  }

  auto sys = buildTopology(args, villasIntf, logger);

  RealTimeSimulation sim(args.name, args);
  sim.setSystem(sys);
  sim.addInterface(villasIntf);
  sim.setDomain(Domain::DP);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);
  if (log) {
    sim.addLogger(logger);
  }

  sim.run(startAt);

  sim.logStepTimes(args.name + "_step_times");
  CPS::Logger::get("DP_Ph1_9bus_4order_cosim_follower")
      ->info("Simulation finished.");
  sim.checkForOverruns(args.name + "_overruns");
}
