// SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, EONERC, RWTH Aachen University
// SPDX-License-Identifier: MPL-2.0

// Shared building blocks for the basic co-simulation examples in this folder.
//
// Each basic example couples two one-node sub-networks through a VILLAS bridge
// using the interface-transmission (ITM) method. One side is the voltage-source
// side (it imports a voltage, imposes it with a controlled voltage source behind
// a small series impedance, and exports the resulting current); the other is the
// current-source side (it imports a current, injects it with a controlled
// current source into a load, and exports the resulting node voltage). The real
// excitation lives on the voltage-source side; the load lives on the
// current-source side.
//
// The matrix of examples varies two things independently:
//   - the ITM assignment: which side (leader or follower) is voltage / current,
//   - the mixed-simulation domains: EMT or DP on each side.
//
// The leader publishes the start-time synchronisation over InterfaceCosimSync;
// the follower waits for it. This header keeps the per-example main() files thin
// so the combination each one represents is easy to read.

#pragma once

#include <DPsim.h>
#include <dpsim-villas/InterfaceVillasQueueless.h>
#include <dpsim/InterfaceCosimSync.h>

namespace DPsimCosimBasics {

using namespace DPsim;
using namespace CPS;

enum class Role { Leader, Follower };

// Which controlled source this side presents at the interface. Current means a
// controlled current source injecting into a load; Voltage means a controlled
// voltage source behind a series impedance, driven by the excitation.
enum class SourceType { Current, Voltage };

// A comma-separated list of `count` phase signals named prefixA[/B/C].
inline std::string phaseSignalList(const char *prefix, const char *unit,
                                   const std::string &type, int count) {
  std::string s;
  const char *phases = "ABC";
  for (int i = 0; i < count; i++)
    s += fmt::format(
        "{}{{ \"name\": \"{}{}\", \"type\": \"{}\", \"unit\": \"{}\" }}",
        i ? ",\n        " : "", prefix, phases[i], type, unit);
  return s;
}

/// @brief Build the VILLAS shmem node configuration for one process.
///
/// Same-domain pairs connect their two shmem nodes directly, so the channel
/// names are the leader/follower pair and the signal counts match. Mixed EMT to
/// DP pairs go through the VILLAS bridge (configs/villas-emt-dp-basics.conf): the
/// channels are named per domain and the DP side exports three fanned phasors
/// (so the bridge can rebuild the three EMT phases) while importing one.
/// @param localDomain this side's domain (EMT or DP).
/// @param role        Leader or Follower, selects the direct channel direction.
/// @param st          the controlled source this side presents.
/// @param bridged     true when the peer is in the other domain.
inline std::string villasConfig(Domain localDomain, Role role, SourceType st,
                                bool bridged) {
  const bool complex = localDomain == Domain::DP;
  const std::string type = complex ? "complex" : "float";
  // The current-source side exports voltage and imports current; the
  // voltage-source side exports current and imports voltage.
  const bool exportsCurrent = st == SourceType::Voltage;

  std::string outName, inName;
  int outCount, inCount;
  if (!bridged) {
    outName = role == Role::Leader ? "/dpsim.leader-to-follower"
                                   : "/dpsim.follower-to-leader";
    inName = role == Role::Leader ? "/dpsim.follower-to-leader"
                                  : "/dpsim.leader-to-follower";
    outCount = inCount = complex ? 1 : 3;
  } else if (localDomain == Domain::EMT) {
    outName = "/dpsim.emt-to-bridge";
    inName = "/dpsim.bridge-to-emt";
    outCount = inCount = 3;
  } else {
    outName = "/dpsim.dp-to-bridge";
    inName = "/dpsim.bridge-to-dp";
    outCount = 3; // three fanned phasors so the bridge can rebuild three phases
    inCount = 1;  // one positive-sequence phasor back from the bridge
  }

  const std::string outSignals =
      exportsCurrent ? phaseSignalList("I", "A", type, outCount)
                     : phaseSignalList("V", "V", type, outCount);
  const std::string inSignals = exportsCurrent
                                    ? phaseSignalList("V", "V", type, inCount)
                                    : phaseSignalList("I", "A", type, inCount);

  const std::string out = fmt::format(
      "\"out\": {{ \"name\": \"{}\", \"signals\": [\n"
      "        {{ \"name\": \"seq_to_self\", \"type\": \"integer\" }},\n"
      "        {{ \"name\": \"seq_to_external\", \"type\": \"integer\" }},\n"
      "        {} ] }}",
      outName, outSignals);
  const std::string in = fmt::format(
      "\"in\": {{ \"name\": \"{}\", \"signals\": [\n"
      "        {{ \"name\": \"seq_from_self\", \"type\": \"integer\" }},\n"
      "        {{ \"name\": \"seq_from_external\", \"type\": \"integer\" }},\n"
      "        {} ] }}",
      inName, inSignals);
  return fmt::format("{{ \"type\": \"shmem\", {}, {} }}", out, in);
}

// Sequence counters exchanged alongside the physical signals so each side can
// detect a stale or dropped frame. Returns the three attributes and registers
// the running export counter's update task.
struct SeqCounters {
  Attribute<Int>::Ptr fromExternal;
  Attribute<Int>::Ptr fromDPsim;
  Attribute<Int>::Ptr toDPsim;
};

inline SeqCounters makeSeqCounters() {
  SeqCounters s;
  s.fromExternal = AttributeStatic<Int>::make(0);
  s.fromDPsim = AttributeStatic<Int>::make(0);
  auto toDPsim = AttributeDynamic<Int>::make(0);
  auto updateFn = std::make_shared<AttributeUpdateTask<Int, Int>::Actor>(
      [](std::shared_ptr<Int> &dependent, Attribute<Int>::Ptr) {
        *dependent = *dependent + 1;
      });
  toDPsim->addTask(UpdateTaskKind::UPDATE_ON_GET,
                   AttributeUpdateTask<Int, Int>::make(
                       UpdateTaskKind::UPDATE_ON_GET, *updateFn, s.fromDPsim));
  s.toDPsim = toDPsim;
  return s;
}

// The leader relies on InterfaceCosimSync for the start instant, so it does not
// block on a first import; the follower stays frame-aligned by blocking.
inline bool blockOnRead(Role role) { return role == Role::Follower; }

// Interface impedances. The ITM coupling is a fixed-point iteration across the
// interface: each step the voltage-source side computes its current from the
// imported voltage, the current-source side computes its voltage from the
// imported current, and so on. The per-step loop gain is roughly the load
// resistance over the series impedance of the voltage-source branch,
// kLoadResistance / (kSeriesResistance + omega * kSeriesInductance). It only
// converges when that ratio is below one, so the load must stay smaller than
// the series impedance. The values below give a gain near 0.1.
static constexpr Real kLoadResistance = 1.0;    // current-source side load
static constexpr Real kSeriesResistance = 10.0; // voltage-source side series R
static constexpr Real kSeriesInductance = 0.001;
static constexpr Real kSnubberResistance =
    1e8; // keeps the cs node well defined
static constexpr Real kSourceVoltage = 1000.0;

// ----- EMT (3-phase) source-side sub-networks -----

/// @brief Current-source side in EMT: a controlled current source injecting the
///        interface current into a resistive load.
inline SystemTopology
emtCurrentSource(CommandLineArgs &args, std::shared_ptr<Interface> intf,
                 std::shared_ptr<DataLoggerInterface> logger, Role role,
                 SeqCounters &seq) {
  auto n1 = SimNode<Real>::make("BUS1", PhaseType::ABC);
  auto rl = EMT::Ph3::Resistor::make("rl");
  rl->setParameters(Math::singlePhaseParameterToThreePhase(kLoadResistance));
  auto cs = EMT::Ph3::ControlledCurrentSource::make("cs");
  cs->setParameters(Math::singlePhaseParameterToThreePhase(0));
  auto rcs = EMT::Ph3::Resistor::make("rcs");
  rcs->setParameters(
      Math::singlePhaseParameterToThreePhase(kSnubberResistance));

  rl->connect({n1, SimNode<Real>::GND});
  cs->connect({n1, SimNode<Real>::GND});
  rcs->connect({n1, SimNode<Real>::GND});

  auto sys = SystemTopology(args.sysFreq, SystemNodeList{n1},
                            SystemComponentList{rl, cs, rcs});

  bool b = blockOnRead(role);
  intf->addImport(seq.fromExternal, b, b);
  intf->addImport(seq.fromDPsim, b, b);
  intf->addImport(cs->mCurrentRef->deriveCoeff<Real>(0, 0), b, b);
  intf->addImport(cs->mCurrentRef->deriveCoeff<Real>(1, 0), b, b);
  intf->addImport(cs->mCurrentRef->deriveCoeff<Real>(2, 0), b, b);
  intf->addExport(seq.toDPsim);
  intf->addExport(seq.fromExternal);
  intf->addExport(n1->mVoltage->deriveCoeff<Real>(0, 0));
  intf->addExport(n1->mVoltage->deriveCoeff<Real>(1, 0));
  intf->addExport(n1->mVoltage->deriveCoeff<Real>(2, 0));

  if (logger) {
    logger->logAttribute("a", n1->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b", n1->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c", n1->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("a_i", cs->mIntfCurrent->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b_i", cs->mIntfCurrent->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c_i", cs->mIntfCurrent->deriveCoeff<Real>(2, 0));
  }
  return sys;
}

/// @brief Voltage-source side in EMT: the excitation (a fixed source) plus a
///        controlled voltage source, driven by the interface voltage, behind a
///        small series impedance.
inline SystemTopology
emtVoltageSource(CommandLineArgs &args, std::shared_ptr<Interface> intf,
                 std::shared_ptr<DataLoggerInterface> logger, Role role,
                 SeqCounters &seq) {
  auto n1 = SimNode<Real>::make("BUS1", PhaseType::ABC);
  auto nri = SimNode<Real>::make("BUS1_ri", PhaseType::ABC);
  auto nvr = SimNode<Real>::make("BUS1_vr", PhaseType::ABC);

  auto vl = EMT::Ph3::VoltageSource::make("vl");
  vl->setParameters(
      Math::singlePhaseVariableToThreePhase(Math::polar(kSourceVoltage, 0)),
      args.sysFreq);
  auto vs = EMT::Ph3::ControlledVoltageSource::make("vs");
  vs->setParameters(Math::singlePhaseParameterToThreePhase(0));
  auto rvs = EMT::Ph3::Resistor::make("rvs");
  rvs->setParameters(Math::singlePhaseParameterToThreePhase(kSeriesResistance));
  auto ivs = EMT::Ph3::Inductor::make("ivs");
  ivs->setParameters(Math::singlePhaseParameterToThreePhase(kSeriesInductance));

  vl->connect({n1, SimNode<Real>::GND});
  vs->connect({SimNode<Real>::GND, nvr});
  rvs->connect({nvr, nri});
  ivs->connect({nri, n1});

  auto sys = SystemTopology(args.sysFreq, SystemNodeList{n1, nvr, nri},
                            SystemComponentList{vl, vs, rvs, ivs});

  bool b = blockOnRead(role);
  intf->addImport(seq.fromExternal, b, b);
  intf->addImport(seq.fromDPsim, b, b);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(0, 0), b, b);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(1, 0), b, b);
  intf->addImport(vs->mVoltageRef->deriveCoeff<Real>(2, 0), b, b);
  intf->addExport(seq.toDPsim);
  intf->addExport(seq.fromExternal);
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(0, 0));
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(1, 0));
  intf->addExport(rvs->mIntfCurrent->deriveCoeff<Real>(2, 0));

  if (logger) {
    logger->logAttribute("a", n1->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b", n1->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c", n1->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("ax", nvr->mVoltage->deriveCoeff<Real>(0, 0));
    logger->logAttribute("bx", nvr->mVoltage->deriveCoeff<Real>(1, 0));
    logger->logAttribute("cx", nvr->mVoltage->deriveCoeff<Real>(2, 0));
    logger->logAttribute("a_i", rvs->mIntfCurrent->deriveCoeff<Real>(0, 0));
    logger->logAttribute("b_i", rvs->mIntfCurrent->deriveCoeff<Real>(1, 0));
    logger->logAttribute("c_i", rvs->mIntfCurrent->deriveCoeff<Real>(2, 0));
  }
  return sys;
}

// ----- DP (single-phase phasor) source-side sub-networks -----

/// @brief Current-source side in DP: a controlled current source injecting the
///        interface current phasor into a resistive load.
inline SystemTopology
dpCurrentSource(CommandLineArgs &args, std::shared_ptr<Interface> intf,
                std::shared_ptr<DataLoggerInterface> logger, Role role,
                SeqCounters &seq, bool bridged) {
  auto n1 = SimNode<Complex>::make("BUS1", PhaseType::Single);
  auto rl = DP::Ph1::Resistor::make("rl");
  rl->setParameters(kLoadResistance);
  auto cs = DP::Ph1::ControlledCurrentSource::make("cs");
  cs->setParameters(Complex(0, 0));

  rl->connect({n1, SimNode<Complex>::GND});
  cs->connect({n1, SimNode<Complex>::GND});

  auto sys = SystemTopology(args.sysFreq, SystemNodeList{n1},
                            SystemComponentList{rl, cs});

  bool b = blockOnRead(role);
  intf->addImport(seq.fromExternal, b, b);
  intf->addImport(seq.fromDPsim, b, b);
  intf->addImport(cs->mCurrentRef, b, b);
  intf->addExport(seq.toDPsim);
  intf->addExport(seq.fromExternal);
  auto v = n1->mVoltage->deriveCoeff<Complex>(0, 0);
  intf->addExport(v);
  // Fan out to a balanced three-phase set so the bridge can rebuild the EMT
  // phases (positive-sequence coupling; only needed against an EMT peer).
  if (bridged) {
    intf->addExport(v->deriveScaled(SHIFT_TO_PHASE_B));
    intf->addExport(v->deriveScaled(SHIFT_TO_PHASE_C));
  }

  if (logger) {
    logger->logAttribute("v", n1->mVoltage->deriveCoeff<Complex>(0, 0));
    logger->logAttribute("i", cs->mIntfCurrent->deriveCoeff<Complex>(0, 0));
  }
  return sys;
}

/// @brief Voltage-source side in DP: the excitation phasor plus a controlled
///        voltage source, driven by the interface voltage, behind a small
///        series impedance.
inline SystemTopology
dpVoltageSource(CommandLineArgs &args, std::shared_ptr<Interface> intf,
                std::shared_ptr<DataLoggerInterface> logger, Role role,
                SeqCounters &seq, bool bridged) {
  auto n1 = SimNode<Complex>::make("BUS1", PhaseType::Single);
  auto nri = SimNode<Complex>::make("BUS1_ri", PhaseType::Single);
  auto nvr = SimNode<Complex>::make("BUS1_vr", PhaseType::Single);

  auto vl = DP::Ph1::VoltageSource::make("vl");
  vl->setParameters(Math::polar(kSourceVoltage, 0), 0);
  auto vs = DP::Ph1::ControlledVoltageSource::make("vs");
  vs->setParameters(Complex(0, 0));
  auto rvs = DP::Ph1::Resistor::make("rvs");
  rvs->setParameters(kSeriesResistance);
  auto ivs = DP::Ph1::Inductor::make("ivs");
  ivs->setParameters(kSeriesInductance);

  vl->connect({n1, SimNode<Complex>::GND});
  vs->connect({SimNode<Complex>::GND, nvr});
  rvs->connect({nvr, nri});
  ivs->connect({nri, n1});

  auto sys = SystemTopology(args.sysFreq, SystemNodeList{n1, nvr, nri},
                            SystemComponentList{vl, vs, rvs, ivs});

  bool b = blockOnRead(role);
  intf->addImport(seq.fromExternal, b, b);
  intf->addImport(seq.fromDPsim, b, b);
  intf->addImport(vs->mVoltageRef, b, b);
  intf->addExport(seq.toDPsim);
  intf->addExport(seq.fromExternal);
  auto i = rvs->mIntfCurrent->deriveCoeff<Complex>(0, 0);
  intf->addExport(i);
  if (bridged) {
    intf->addExport(i->deriveScaled(SHIFT_TO_PHASE_B));
    intf->addExport(i->deriveScaled(SHIFT_TO_PHASE_C));
  }

  if (logger) {
    logger->logAttribute("v", n1->mVoltage->deriveCoeff<Complex>(0, 0));
    logger->logAttribute("vx", nvr->mVoltage->deriveCoeff<Complex>(0, 0));
    logger->logAttribute("i", rvs->mIntfCurrent->deriveCoeff<Complex>(0, 0));
  }
  return sys;
}

/// @brief Run one basic co-simulation process end to end.
/// @param localDomain the domain of this side (EMT or DP).
/// @param peerDomain  the domain of the other side; if it differs the pair goes
///                    through the VILLAS bridge instead of pairing directly.
/// @param role        Leader (publishes sync) or Follower (waits for it).
/// @param st          the controlled source this side presents.
inline int runBasicCosim(int argc, char *argv[], Domain localDomain,
                         Domain peerDomain, Role role, SourceType st,
                         const std::string &name) {
  // dt = 1e-4 gives 200 samples per 50 Hz period: enough to resolve the EMT
  // carrier and an integer demodulation window for the mixed EMT to DP bridge.
  CommandLineArgs args(argc, argv, name, 1e-4, 60.0, 50.0, -1,
                       CPS::Logger::Level::info, CPS::Logger::Level::off, false,
                       false, false, localDomain);
  CPS::Logger::setLogDir("logs/" + args.name);
  bool log = args.options.find("log") != args.options.end() &&
             args.getOptionBool("log");

  bool bridged = localDomain != peerDomain;
  auto villasIntf = std::make_shared<InterfaceVillasQueueless>(
      villasConfig(localDomain, role, st, bridged), "VillasInterface",
      spdlog::level::off);

  uint16_t port = 8890;
  if (const char *env = std::getenv("DPSIM_SYNC_PORT"))
    port = static_cast<uint16_t>(std::stoi(env));
  std::string host = "127.0.0.1";
  if (const char *env = std::getenv("DPSIM_SYNC_HOST"))
    host = env;
  if (args.options.find("port") != args.options.end())
    port = static_cast<uint16_t>(args.getOptionInt("port"));
  if (args.options.find("host") != args.options.end())
    host = args.getOptionString("host");

  auto syncIntf = std::make_shared<InterfaceCosimSync>(
      "CosimSync", role == Role::Leader ? "" : host, port,
      role == Role::Leader ? InterfaceCosimSync::Role::Leader
                           : InterfaceCosimSync::Role::Follower);
  syncIntf->open();

  std::chrono::system_clock::time_point startAt;
  if (role == Role::Follower) {
    InterfaceCosimSync::ConfigNs cfg;
    if (!syncIntf->waitForConfig(cfg, 30000)) {
      CPS::Logger::get(args.name)->error(
          "Timed out waiting for leader sync config");
      return 1;
    }
    args.timeStep = static_cast<double>(cfg.time_step_ns) / 1e9;
    args.duration = static_cast<double>(cfg.duration_ns) / 1e9;
    startAt = InterfaceCosimSync::toTimePoint(cfg.start_time_ns);
  }

  std::shared_ptr<DataLoggerInterface> logger = nullptr;
  std::filesystem::path logFilename =
      "logs/" + args.name + "/" + args.name + ".csv";
  if (log) {
    size_t rows = static_cast<size_t>(args.duration / args.timeStep + 0.5) + 2;
    logger = RealTimeDataLogger::make(logFilename, rows);
  }

  auto seq = makeSeqCounters();
  SystemTopology sys = [&] {
    if (localDomain == Domain::EMT)
      return st == SourceType::Current
                 ? emtCurrentSource(args, villasIntf, logger, role, seq)
                 : emtVoltageSource(args, villasIntf, logger, role, seq);
    return st == SourceType::Current
               ? dpCurrentSource(args, villasIntf, logger, role, seq, bridged)
               : dpVoltageSource(args, villasIntf, logger, role, seq, bridged);
  }();

  RealTimeSimulation sim(args.name, args);
  sim.setSystem(sys);
  sim.addInterface(villasIntf);
  sim.setDomain(localDomain);
  sim.doSystemMatrixRecomputation(true);
  sim.setLogStepTimes(true);
  if (logger)
    sim.addLogger(logger);

  if (role == Role::Leader) {
    startAt = std::chrono::system_clock::now() + std::chrono::seconds(5);
    uint64_t dtNs = static_cast<uint64_t>(args.timeStep * 1e9);
    uint64_t durNs = static_cast<uint64_t>(args.duration * 1e9);
    if (!syncIntf->publishConfig(startAt, dtNs, durNs, 1, 30000)) {
      CPS::Logger::get(args.name)->error(
          "Follower did not acknowledge sync config");
      return 1;
    }
  }

  sim.run(startAt);
  sim.logStepTimes(args.name + "_step_times");
  sim.checkForOverruns(args.name + "_overruns");
  CPS::Logger::get(args.name)->info("Simulation finished.");
  return 0;
}

/// @brief Same-domain convenience overload (peer domain equals the local one).
inline int runBasicCosim(int argc, char *argv[], Domain localDomain, Role role,
                         SourceType st, const std::string &name) {
  return runBasicCosim(argc, argv, localDomain, localDomain, role, st, name);
}

} // namespace DPsimCosimBasics
