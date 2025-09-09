/* Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023-2024 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: MPL-2.0
 */

#include <dpsim-villas/InterfaceVillasQueueless.h>
#include <dpsim-villas/InterfaceWorkerVillas.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <typeinfo>
#include <villas/node.h>
#include <villas/node/memory.hpp>
#include <villas/node/memory_type.hpp>
#include <villas/path.hpp>
#include <villas/signal_type.hpp>

using namespace CPS;
using namespace villas;

namespace DPsim {

InterfaceVillasQueueless::InterfaceVillasQueueless(
    const String &nodeConfig, const String &name,
    spdlog::level::level_enum logLevel)
    : Interface(name, logLevel), mNodeConfig(nodeConfig), mNode(nullptr),
      mSamplePool(), mSequenceToDpsim(0), mSequenceFromDpsim(0) {}

void InterfaceVillasQueueless::createNode() {
  if (villas::node::memory::init(100) != 0) {
    SPDLOG_LOGGER_ERROR(mLog, "Error: Failed to initialize memory subsystem!");
    std::exit(1);
  }
  json_error_t error;
  json_t *config = json_loads(mNodeConfig.c_str(), 0, &error);
  if (config == nullptr) {
    SPDLOG_LOGGER_ERROR(mLog, "Error: Failed to parse node config! Error: {}",
                        error.text);
    throw JsonError(config, error);
  }

  const json_t *nodeType = json_object_get(config, "type");
  if (nodeType == nullptr) {
    SPDLOG_LOGGER_ERROR(mLog, "Error: Node config does not contain type-key!");
    std::exit(1);
  }
  String nodeTypeString = json_string_value(nodeType);

  mNode = node::NodeFactory::make(nodeTypeString);

  int ret = 0;
  ret = mNode->parse(config);
  if (ret < 0) {
    SPDLOG_LOGGER_ERROR(mLog,
                        "Error: Node in InterfaceVillas failed to parse "
                        "config. Parse returned code {}",
                        ret);
    std::exit(1);
  }
  ret = mNode->check();
  if (ret < 0) {
    SPDLOG_LOGGER_ERROR(
        mLog,
        "Error: Node in InterfaceVillas failed check. Check returned code {}",
        ret);
    std::exit(1);
  }
  struct villas::node::memory::Type *pool_mt = &villas::node::memory::heap;
  ret = node::pool_init(&mSamplePool, 16,
                        sizeof(node::Sample) + SAMPLE_DATA_LENGTH(64), pool_mt);
  if (ret < 0) {
    SPDLOG_LOGGER_ERROR(mLog,
                        "Error: InterfaceVillas failed to init sample pool. "
                        "pool_init returned code {}",
                        ret);
    std::exit(1);
  }
}

static node::SignalType stdTypeToNodeType(const std::type_info &type) {
  if (type == typeid(Real)) {
    return node::SignalType::FLOAT;
  } else if (type == typeid(Int)) {
    return node::SignalType::INTEGER;
  } else if (type == typeid(Bool)) {
    return node::SignalType::BOOLEAN;
  } else if (type == typeid(Complex)) {
    return node::SignalType::COMPLEX;
  } else {
    return node::SignalType::INVALID;
  }
}

void InterfaceVillasQueueless::createSignals() {
  if (!mNode->out.path)
    mNode->out.path = new node::Path();
  if (!mNode->in.path)
    mNode->in.path = new node::Path();

  mNode->out.path->signals = std::make_shared<node::SignalList>();
  auto nodeOutputSignals = mNode->out.path->signals;
  nodeOutputSignals->clear();

  unsigned int idx = 0;
  for (const auto &[attr, id] : mExportAttrsDpsim) {
    while (id > idx) {
      nodeOutputSignals->push_back(
          std::make_shared<node::Signal>("", "", node::SignalType::INVALID));
      idx++;
    }
    nodeOutputSignals->push_back(std::make_shared<node::Signal>(
        "", "", stdTypeToNodeType(attr->getType())));
    idx++;
  }

  mNode->in.path->signals = std::make_shared<node::SignalList>();
  auto nodeInputSignals = mNode->in.path->signals;
  nodeInputSignals->clear();

  idx = 0;
  for (const auto &[attr, id, blockOnRead, syncOnSimulationStart] :
       mImportAttrsDpsim) {
    while (id > idx) {
      nodeInputSignals->push_back(
          std::make_shared<node::Signal>("", "", node::SignalType::INVALID));
      idx++;
    }
    nodeInputSignals->push_back(std::make_shared<node::Signal>(
        "", "", stdTypeToNodeType(attr->getType())));
    idx++;
  }
}

void InterfaceVillasQueueless::open() {
  createNode();
  createSignals();

  int ret = mNode->prepare();
  if (ret < 0) {
    SPDLOG_LOGGER_ERROR(mLog,
                        "Error: Node in InterfaceVillas failed to prepare. "
                        "Prepare returned code {}",
                        ret);
    std::exit(1);
  }
  SPDLOG_LOGGER_INFO(mLog, "Node: {}", mNode->getNameFull());

  mNode->getFactory()->start(nullptr);

  ret = mNode->start();
  if (ret < 0) {
    SPDLOG_LOGGER_ERROR(mLog,
                        "Fatal error: failed to start node in InterfaceVillas. "
                        "Start returned code {}",
                        ret);
    close();
    std::exit(1);
  }
  mOpened = true;
  mSequenceFromDpsim = 0;
  mSequenceToDpsim = 0;
}

void InterfaceVillasQueueless::close() {
  SPDLOG_LOGGER_INFO(mLog, "Closing InterfaceVillas...");
  int ret = mNode->stop();
  if (ret < 0) {
    SPDLOG_LOGGER_ERROR(
        mLog,
        "Error: failed to stop node in InterfaceVillas. Stop returned code {}",
        ret);
    std::exit(1);
  }
  mOpened = false;
  ret = node::pool_destroy(&mSamplePool);
  if (ret < 0) {
    SPDLOG_LOGGER_ERROR(mLog,
                        "Error: failed to destroy SamplePool in "
                        "InterfaceVillas. pool_destroy returned code {}",
                        ret);
    std::exit(1);
  }

  mNode->getFactory()->stop();

  delete mNode;
  mOpened = false;
}

CPS::Task::List InterfaceVillasQueueless::getTasks() {
  auto tasks = CPS::Task::List();
  if (!mImportAttrsDpsim.empty()) {
    tasks.push_back(std::make_shared<InterfaceVillasQueueless::PreStep>(*this));
  }
  if (!mExportAttrsDpsim.empty()) {
    tasks.push_back(
        std::make_shared<InterfaceVillasQueueless::PostStep>(*this));
  }
  return tasks;
}

void InterfaceVillasQueueless::PreStep::execute(Real time, Int timeStepCount) {
  auto seqnum = mIntf.readFromVillas();
  static size_t overrunCounter = 0;
  if (seqnum != mIntf.mSequenceToDpsim + 1) {
    overrunCounter++;
  }
  if (overrunCounter > 10000) {
    SPDLOG_LOGGER_WARN(mIntf.mLog, "{} Overrun(s) detected!", overrunCounter);
    overrunCounter = 0;
  }
  mIntf.mSequenceToDpsim = seqnum;
}

Int InterfaceVillasQueueless::readFromVillas() {
  node::Sample *sample = nullptr;
  Int seqnum = 0;
  int ret = 0;

  if (mImportAttrsDpsim.empty())
    return 0;

  try {
    sample = node::sample_alloc(&mSamplePool);
    if (!sample)
      throw RuntimeError("sample_alloc returned nullptr");

    ret = mNode->read(&sample, 1);
    if (ret <= 0) {
      if (ret == 0) {
        SPDLOG_LOGGER_WARN(mLog, "InterfaceVillas read returned 0.");
      } else {
        SPDLOG_LOGGER_ERROR(mLog, "InterfaceVillas read failed: {}", ret);
      }
      sample_decref(sample);
      return mSequenceToDpsim;
    }

    if ((UInt)sample->length != mImportAttrsDpsim.size()) {
      SPDLOG_LOGGER_ERROR(
          mLog, "Received Sample length ({}) != configured attributes ({})",
          sample->length, mImportAttrsDpsim.size());
      sample_decref(sample);
      throw RuntimeError("Sample length mismatch");
    }

    for (size_t i = 0; i < mImportAttrsDpsim.size(); i++) {
      auto attr = std::get<0>(mImportAttrsDpsim[i]);
      if (attr->getType() == typeid(Real)) {
        auto a = std::dynamic_pointer_cast<Attribute<Real>>(attr.getPtr());
        a->set(sample->data[i].f);
      } else if (attr->getType() == typeid(Int)) {
        auto a = std::dynamic_pointer_cast<Attribute<Int>>(attr.getPtr());
        a->set(sample->data[i].i);
      } else if (attr->getType() == typeid(Bool)) {
        auto a = std::dynamic_pointer_cast<Attribute<Bool>>(attr.getPtr());
        a->set(sample->data[i].b);
      } else if (attr->getType() == typeid(Complex)) {
        auto a = std::dynamic_pointer_cast<Attribute<Complex>>(attr.getPtr());
        a->set(Complex(sample->data[i].z.real(), sample->data[i].z.imag()));
      } else {
        sample_decref(sample);
        throw RuntimeError("Unsupported attribute type");
      }
    }

    if (sample->flags & (int)villas::node::SampleFlags::HAS_SEQUENCE) {
      seqnum = sample->sequence;
    } else if (!mImportAttrsDpsim.empty() &&
               std::get<0>(mImportAttrsDpsim[0])->getType() == typeid(Int)) {
      seqnum = sample->data[0].i;
    }

    sample_decref(sample);
  } catch (...) {
    if (sample)
      sample_decref(sample);
    throw;
  }

  return seqnum;
}

void InterfaceVillasQueueless::PostStep::execute(Real time, Int timeStepCount) {
  mIntf.writeToVillas();
}

void InterfaceVillasQueueless::writeToVillas() {
  if (mExportAttrsDpsim.empty())
    return;

  node::Sample *sample = nullptr;
  Int ret = 0;

  try {
    sample = node::sample_alloc(&mSamplePool);
    if (!sample) {
      SPDLOG_LOGGER_ERROR(mLog, "InterfaceVillas could not allocate a new "
                                "sample! Not sending any data!");
      return;
    }

    for (size_t i = 0; i < mExportAttrsDpsim.size(); i++) {
      auto attr = std::get<0>(mExportAttrsDpsim[i]);
      if (attr->getType() == typeid(Real)) {
        auto a = std::dynamic_pointer_cast<Attribute<Real>>(attr.getPtr());
        sample->data[i].f = a->get();
      } else if (attr->getType() == typeid(Int)) {
        auto a = std::dynamic_pointer_cast<Attribute<Int>>(attr.getPtr());
        sample->data[i].i = a->get();
      } else if (attr->getType() == typeid(Bool)) {
        auto a = std::dynamic_pointer_cast<Attribute<Bool>>(attr.getPtr());
        sample->data[i].b = a->get();
      } else if (attr->getType() == typeid(Complex)) {
        auto a = std::dynamic_pointer_cast<Attribute<Complex>>(attr.getPtr());
        sample->data[i].z =
            std::complex<float>(a->get().real(), a->get().imag());
      } else {
        sample_decref(sample);
        throw RuntimeError("Unsupported attribute type");
      }
    }

    sample->length = mExportAttrsDpsim.size();
    sample->sequence = mSequenceFromDpsim++;
    sample->flags |= (int)villas::node::SampleFlags::HAS_SEQUENCE;
    sample->flags |= (int)villas::node::SampleFlags::HAS_DATA;
    clock_gettime(CLOCK_REALTIME, &sample->ts.origin);
    sample->flags |= (int)villas::node::SampleFlags::HAS_TS_ORIGIN;

    do {
      ret = mNode->write(&sample, 1);
    } while (ret == 0);
    if (ret < 0) {
      SPDLOG_LOGGER_ERROR(mLog,
                          "Failed to write samples to InterfaceVillas. Write "
                          "returned code {}",
                          ret);
    }

    sample_decref(sample);
  } catch (...) {
    sample_decref(sample);
    if (ret < 0)
      SPDLOG_LOGGER_ERROR(
          mLog,
          "Failed to write samples to InterfaceVillas. Write returned code {}",
          ret);
  }
}

void InterfaceVillasQueueless::syncImports() {
  bool needSync = false;
  for (const auto &t : mImportAttrsDpsim) {
    if (std::get<3>(t)) {
      needSync = true;
      break;
    }
  }
  if (!needSync)
    return;

  mSequenceToDpsim = this->readFromVillas();
}

void InterfaceVillasQueueless::syncExports() { this->writeToVillas(); }

void InterfaceVillasQueueless::printVillasSignals() const {
  SPDLOG_LOGGER_INFO(mLog, "Export signals:");
  for (const auto &signal : *mNode->getOutputSignals(true)) {
    SPDLOG_LOGGER_INFO(mLog, "Name: {}, Unit: {}, Type: {}", signal->name,
                       signal->unit, node::signalTypeToString(signal->type));
  }

  SPDLOG_LOGGER_INFO(mLog, "Import signals:");
  for (const auto &signal : *mNode->getInputSignals(true)) {
    SPDLOG_LOGGER_INFO(mLog, "Name: {}, Unit: {}, Type: {}", signal->name,
                       signal->unit, node::signalTypeToString(signal->type));
  }
}

} // namespace DPsim
