//==--------- global_handler.hpp --- Global objects handler ----------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <sycl/detail/spinlock.hpp>
#include <sycl/detail/util.hpp>

#include <atomic>
#include <cassert>
#include <memory>
#include <unordered_map>

namespace sycl {
inline namespace _V1 {
namespace detail {
class platform_impl;
class context_impl;
class Scheduler;
class ProgramManager;
class Sync;
class adapter_impl;
class ods_target_list;
class XPTIRegistry;
class ThreadPool;

/// Wrapper class for global data structures with non-trivial destructors.
///
/// As user code can call SYCL Runtime functions from destructor of global
/// objects, it is not safe for the runtime library to have global objects with
/// non-trivial destructors. Such destructors can be called any time after
/// exiting main, which may result in user application crashes. Instead,
/// complex global objects must be wrapped into GlobalHandler. Its instance
/// is stored on heap, and deallocated when the runtime library is being
/// unloaded.
///
/// There's no need to store trivial globals here, as no code for their
/// construction or destruction is generated anyway.
class GlobalHandler {
public:
  static bool isInstanceAlive() {
    return RTGlobalObjHandler.load(std::memory_order_relaxed) != nullptr;
  }
  /// \return a reference to a GlobalHandler singleton instance. The reference
  /// is valid as long as runtime library is loaded (i.e. until `DllMain` or
  /// `__attribute__((destructor))` is called).
  static GlobalHandler &instance() {
    // Load once: re-loading could observe the shutdown store in between the
    // check and the dereference.
    GlobalHandler *Handler = RTGlobalObjHandler.load(std::memory_order_relaxed);
    assert(Handler && "GlobalHandler is used after runtime teardown");
    return *Handler;
  }

  GlobalHandler(const GlobalHandler &) = delete;
  GlobalHandler(GlobalHandler &&) = delete;
  GlobalHandler &operator=(const GlobalHandler &) = delete;

  void registerSchedulerUsage(bool ModifyCounter = true);
  Scheduler &getScheduler();
  bool isSchedulerAlive() const;
  ProgramManager &getProgramManager();
  Sync &getSync();
  std::vector<std::shared_ptr<platform_impl>> &getPlatformCache();

  std::unordered_map<platform_impl *, std::shared_ptr<context_impl>> &
  getPlatformToDefaultContextCache();

  std::mutex &getPlatformToDefaultContextCacheMutex();
  std::mutex &getPlatformMapMutex();
  std::mutex &getFilterMutex();
  std::vector<adapter_impl *> &getAdapters();
  ods_target_list &getOneapiDeviceSelectorTargets(const std::string &InitValue);
  XPTIRegistry &getXPTIRegistry();
  ThreadPool &getHostTaskThreadPool();
  static void registerStaticVarShutdownHandler();

  bool isOkToDefer() const;
  void endDeferredRelease();
  void unloadAdapters();
  void releaseDefaultContexts();
  void drainThreadPool();
  void prepareSchedulerToRelease(bool Blocking);

  void TraceEventXPTI(const char *Message);

  // For testing purposes only
  void attachScheduler(Scheduler *Scheduler);

  // Used in SYCL unit tests to reset the GlobalHandler instance.
  static void resetGlobalHandler() {
    RTGlobalObjHandler = new GlobalHandler();
  };

  // Used in SYCL unit tests to simulate runtime teardown; pair with
  // restoreGlobalHandler().
  static GlobalHandler *detachGlobalHandler() {
    GlobalHandler *Old = RTGlobalObjHandler;
    RTGlobalObjHandler = nullptr;
    return Old;
  }
  static void restoreGlobalHandler(GlobalHandler *Handler) {
    RTGlobalObjHandler = Handler;
  }

private:
  // Constructor and destructor are declared out-of-line to allow incomplete
  // types as template arguments to unique_ptr.
  GlobalHandler();
  ~GlobalHandler();

  // Cleared by endDeferredRelease() on the thread running static destruction,
  // and read by isOkToDefer() on whichever thread is releasing a memory object
  // - including a host task worker, which shutdown_early() only waits for
  // *after* clearing this. Atomic so those two do not race.
  std::atomic<bool> OkToDefer = true;

  friend void shutdown_early(bool);
  friend void shutdown_late();
  friend class ObjectUsageCounter;
  static SpinLock MSyclGlobalHandlerProtector;

  template <typename T> struct InstWithLock {
    std::unique_ptr<T> Inst;
    SpinLock Lock;
  };

  template <typename T, typename... Types>
  T &getOrCreate(InstWithLock<T> &IWL, Types &&...Args);

  InstWithLock<Scheduler> MScheduler;
  InstWithLock<ProgramManager> MProgramManager;
  InstWithLock<Sync> MSync;
  InstWithLock<std::vector<std::shared_ptr<platform_impl>>> MPlatformCache;
  InstWithLock<
      std::unordered_map<platform_impl *, std::shared_ptr<context_impl>>>
      MPlatformToDefaultContextCache;
  InstWithLock<std::mutex> MPlatformToDefaultContextCacheMutex;
  InstWithLock<std::mutex> MPlatformMapMutex;
  InstWithLock<std::mutex> MFilterMutex;
  InstWithLock<std::vector<adapter_impl *>> MAdapters;
  InstWithLock<ods_target_list> MOneapiDeviceSelectorTargets;
  InstWithLock<XPTIRegistry> MXPTIRegistry;
  // Thread pool for host task and event callbacks execution
  InstWithLock<ThreadPool> MHostTaskThreadPool;

  // Nulled by shutdown_late() on the thread running static destruction, while
  // threads that outlive it - a host task worker, or user code calling into the
  // runtime from another global's destructor - may still be loading it via
  // instance()/isInstanceAlive() without MSyclGlobalHandlerProtector. Atomic so
  // those loads are well defined; relaxed is enough, the object's own state is
  // guarded by its per-member locks.
  static std::atomic<GlobalHandler *> RTGlobalObjHandler;
};

} // namespace detail
} // namespace _V1
} // namespace sycl
