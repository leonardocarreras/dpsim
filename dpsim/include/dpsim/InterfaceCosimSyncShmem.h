// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <dpsim/Interface.h>

namespace DPsim {

// A lightweight shared-memory interface for co-simulation clock sync.
// Leader publishes absolute start time (system_clock), timestep and duration.
// Followers wait for the config and then start their RealTimeSimulation
// using run(startAt).
class InterfaceCosimSyncShmem : public Interface,
                                public SharedFactory<InterfaceCosimSyncShmem> {
public:
  enum class Role { Leader, Follower };

  struct ConfigNs {
    uint64_t start_time_ns; // epoch: system_clock
    uint64_t time_step_ns;
    uint64_t duration_ns;
  };

  InterfaceCosimSyncShmem(const String &name, const String &shmName, Role role)
      : Interface(name), mShmName(shmName), mRole(role) {}

  ~InterfaceCosimSyncShmem() override {
    if (mOpened)
      close();
  }

  // Interface overrides (no per-step tasks; this is control-path only)
  void open() override;
  void close() override;
  void syncExports() override {}
  void syncImports() override {}
  CPS::Task::List getTasks() override { return CPS::Task::List(); }

  // Leader publishes configuration once.
  void publishConfig(const std::chrono::system_clock::time_point &startAt,
                     uint64_t timeStepNs, uint64_t durationNs);

  // Follower waits for configuration. Returns true on success.
  bool waitForConfig(ConfigNs &outCfg, uint64_t timeoutMs = 0);

  static std::chrono::system_clock::time_point
  toTimePoint(uint64_t nsSinceEpoch);
  static uint64_t toEpochNs(const std::chrono::system_clock::time_point &tp);

private:
  String mShmName;
  Role mRole;

  int mShmFd = -1;
  void *mMap = nullptr;
  size_t mMapSize = 0;

  struct ShmHeader {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t flags;
    uint32_t reserved;
    uint64_t start_time_ns;
    uint64_t time_step_ns;
    uint64_t duration_ns;
    uint64_t sequence;
  };

  static constexpr uint32_t MAGIC = 0x44505353; // 'DPSS'
  static constexpr uint32_t VERSION = 1;

  // flags bits
  static constexpr uint32_t FLAG_CONFIG_READY = 0x1;

  ShmHeader *hdr();
  void mapSharedMemory(bool create);
  void unmapSharedMemory();
};

} // namespace DPsim
