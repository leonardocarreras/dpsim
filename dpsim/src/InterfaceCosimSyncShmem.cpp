// SPDX-License-Identifier: Apache-2.0

#include <cerrno>
#include <cstring>
#include <thread>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dpsim/InterfaceCosimSyncShmem.h>

using namespace DPsim;
using namespace CPS;

void InterfaceCosimSyncShmem::open() {
  if (mOpened)
    return;

  bool create = (mRole == Role::Leader);
  if (create) {
    mapSharedMemory(true);
    auto *h = hdr();
    h->magic = MAGIC;
    h->version = VERSION;
    h->flags = 0;
    h->reserved = 0;
    h->start_time_ns = 0;
    h->time_step_ns = 0;
    h->duration_ns = 0;
    h->sequence = 0;
  } else {
    // Follower: poll until the shared memory segment is created and sized.
    for (;;) {
      int fd = ::shm_open(mShmName.c_str(), O_RDWR, 0666);
      if (fd < 0) {
        if (errno == ENOENT) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          continue;
        }
        SPDLOG_LOGGER_WARN(mLog, "CosimSyncShmem: shm_open error: {}",
                           std::strerror(errno));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      struct stat st;
      if (fstat(fd, &st) < 0) {
        SPDLOG_LOGGER_WARN(mLog, "CosimSyncShmem: fstat error: {}",
                           std::strerror(errno));
        ::close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      if ((size_t)st.st_size < sizeof(ShmHeader)) {
        // Not sized yet by leader.
        ::close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      void *map = ::mmap(nullptr, sizeof(ShmHeader), PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0);
      if (map == MAP_FAILED) {
        SPDLOG_LOGGER_WARN(mLog, "CosimSyncShmem: mmap error: {}",
                           std::strerror(errno));
        ::close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      // Success
      mShmFd = fd;
      mMap = map;
      mMapSize = sizeof(ShmHeader);
      break;
    }
  }

  mOpened = true;
}

void InterfaceCosimSyncShmem::close() {
  if (!mOpened)
    return;

  unmapSharedMemory();
  if (mRole == Role::Leader) {
    // Best-effort cleanup; ignore errors if another process still has it open.
    ::shm_unlink(mShmName.c_str());
  }
  mOpened = false;
}

void InterfaceCosimSyncShmem::publishConfig(
    const std::chrono::system_clock::time_point &startAt, uint64_t timeStepNs,
    uint64_t durationNs) {
  if (!mOpened || mRole != Role::Leader)
    throw SystemError("CosimSyncShmem: publishConfig called in wrong state");

  auto *h = hdr();
  // Sanity-logging: show deltas to help diagnose wrong epochs/units
  auto now = std::chrono::system_clock::now();
  auto delta = startAt - now;
  if (delta > std::chrono::hours(1) || delta < -std::chrono::hours(1)) {
    double secs =
        std::chrono::duration_cast<std::chrono::duration<double>>(delta)
            .count();
    SPDLOG_LOGGER_WARN(
        mLog, "CosimSyncShmem: start time is more than 1 hour away ({} s).",
        secs);
  }
  h->time_step_ns = timeStepNs;
  h->duration_ns = durationNs;
  h->start_time_ns = toEpochNs(startAt);
  std::atomic_thread_fence(std::memory_order_release);
  h->sequence++;
  h->flags |= FLAG_CONFIG_READY;
}

bool InterfaceCosimSyncShmem::waitForConfig(ConfigNs &outCfg,
                                            uint64_t timeoutMs) {
  if (!mOpened || mRole != Role::Follower)
    throw SystemError("CosimSyncShmem: waitForConfig called in wrong state");

  auto *h = hdr();
  uint64_t startMs = 0;
  if (timeoutMs)
    startMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now().time_since_epoch())
                  .count();

  uint64_t seenSeq = h->sequence;
  while (!(h->flags & FLAG_CONFIG_READY)) {
    if (timeoutMs) {
      uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
      if (nowMs - startMs > timeoutMs)
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // Basic header validation
  if (h->magic != MAGIC || h->version != VERSION) {
    SPDLOG_LOGGER_ERROR(
        mLog, "CosimSyncShmem: invalid header (magic=0x{:x}, version={}).",
        h->magic, h->version);
    return false;
  }
  // Optional: ensure we saw an update
  if (h->sequence == seenSeq) {
    // still okay, continue
  }
  std::atomic_thread_fence(std::memory_order_acquire);
  outCfg.start_time_ns = h->start_time_ns;
  outCfg.time_step_ns = h->time_step_ns;
  outCfg.duration_ns = h->duration_ns;
  // Sanity-logging: show delta to current time
  auto now = std::chrono::system_clock::now();
  auto startAt = toTimePoint(outCfg.start_time_ns);
  auto delta = startAt - now;
  if (delta > std::chrono::hours(1) || delta < -std::chrono::hours(1)) {
    double secs =
        std::chrono::duration_cast<std::chrono::duration<double>>(delta)
            .count();
    SPDLOG_LOGGER_WARN(
        mLog,
        "CosimSyncShmem: received start time is more than 1 hour away ({} s).",
        secs);
  }
  return true;
}

std::chrono::system_clock::time_point
InterfaceCosimSyncShmem::toTimePoint(uint64_t nsSinceEpoch) {
  using TP = std::chrono::time_point<std::chrono::system_clock,
                                     std::chrono::nanoseconds>;
  return TP(std::chrono::nanoseconds(nsSinceEpoch));
}

uint64_t InterfaceCosimSyncShmem::toEpochNs(
    const std::chrono::system_clock::time_point &tp) {
  auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp)
                .time_since_epoch()
                .count();
  return static_cast<uint64_t>(ns);
}

InterfaceCosimSyncShmem::ShmHeader *InterfaceCosimSyncShmem::hdr() {
  return reinterpret_cast<ShmHeader *>(mMap);
}

void InterfaceCosimSyncShmem::mapSharedMemory(bool create) {
  int flags = O_RDWR;
  mode_t mode = 0666;
  if (create)
    flags |= O_CREAT;

  mShmFd = ::shm_open(mShmName.c_str(), flags, mode);
  if (mShmFd < 0) {
    SPDLOG_LOGGER_ERROR(mLog, "CosimSyncShmem: shm_open failed: {}",
                        std::strerror(errno));
    throw SystemError("CosimSyncShmem: shm_open failed");
  }

  mMapSize = sizeof(ShmHeader);
  if (create) {
    if (ftruncate(mShmFd, mMapSize) < 0) {
      SPDLOG_LOGGER_ERROR(mLog, "CosimSyncShmem: ftruncate failed: {}",
                          std::strerror(errno));
      ::close(mShmFd);
      mShmFd = -1;
      throw SystemError("CosimSyncShmem: ftruncate failed");
    }
  }

  mMap =
      ::mmap(nullptr, mMapSize, PROT_READ | PROT_WRITE, MAP_SHARED, mShmFd, 0);
  if (mMap == MAP_FAILED) {
    SPDLOG_LOGGER_ERROR(mLog, "CosimSyncShmem: mmap failed: {}",
                        std::strerror(errno));
    ::close(mShmFd);
    mShmFd = -1;
    throw SystemError("CosimSyncShmem: mmap failed");
  }
}

void InterfaceCosimSyncShmem::unmapSharedMemory() {
  if (mMap && mMap != MAP_FAILED) {
    ::munmap(mMap, mMapSize);
    mMap = nullptr;
  }
  if (mShmFd >= 0) {
    ::close(mShmFd);
    mShmFd = -1;
  }
}
