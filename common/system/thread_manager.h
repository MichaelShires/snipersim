#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include "core.h"
#include "fixed_types.h"
#include "lock.h"
#include "sem.h"
#include "subsecond_time.h"
#include "simulation_context.h"
#include "tls.h"

#include <queue>
#include <vector>

#include "thread.h"
#include "stall_types.h"
#include <atomic>

class Thread;
class CoreManager;
class StatsManager;
class HooksManager;
class SyscallServer;
class ClockSkewMinimizationServer;
class Scheduler;

class ThreadManager
{
 public:
   typedef ::stall_type_t stall_type_t;
   typedef Thread::ThreadState ThreadState;

   static const char *stall_type_names[];

   struct ThreadSpawnRequest
   {
      thread_id_t creator_thread_id;
      thread_id_t thread_id;
      SubsecondTime time;
   };

   ThreadManager(SimulationContext *context);
   ~ThreadManager();

   Thread *getThreadFromID(thread_id_t thread_id);
   Thread *getCurrentThread(int threadIndex = 0);
   Thread *findThreadByTid(pid_t tid);

   Thread *createThread(app_id_t app_id, thread_id_t creator_thread_id);
   void onThreadStart(thread_id_t thread_id, SubsecondTime time);
   void onThreadExit(thread_id_t thread_id);

   thread_id_t spawnThread(thread_id_t thread_id, app_id_t app_id);
   thread_id_t getThreadToSpawn(SubsecondTime &time);
   void waitForThreadStart(thread_id_t thread_id, thread_id_t wait_thread_id);
   void joinThread(thread_id_t thread_id, thread_id_t join_thread_id);

   SubsecondTime stallThread(thread_id_t thread_id, stall_type_t reason, SubsecondTime time);
   void stallThread_async(thread_id_t thread_id, stall_type_t reason, SubsecondTime time);
   void resumeThread(thread_id_t thread_id, thread_id_t thread_by, SubsecondTime time, void *msg = NULL);
   void resumeThread_async(thread_id_t thread_id, thread_id_t thread_by, SubsecondTime time, void *msg = NULL);
   void moveThread(thread_id_t thread_id, core_id_t core_id, SubsecondTime time);

   UInt32 getNumThreads() { return m_threads.size(); }
   bool anyThreadRunning();
   bool areAllCoresRunning();

   Core::State getThreadState(thread_id_t thread_id) { return getThreadFromID(thread_id)->getStatus(); }
   stall_type_t getThreadStallReason(thread_id_t thread_id) { return getThreadFromID(thread_id)->getStalledReason(); }
   bool isThreadRunning(thread_id_t thread_id);
   bool isThreadInitializing(thread_id_t thread_id);

   Lock &getLock() { return m_thread_lock; }
   Scheduler *getScheduler() { return m_scheduler; }

 private:
   SimulationContext *m_context;
   TLS *m_thread_tls;
   Scheduler *m_scheduler;

   std::vector<Thread *> m_threads;
   std::queue<ThreadSpawnRequest> m_thread_spawn_list;
   Lock m_thread_lock;
   std::atomic<int> m_num_running_threads;

   CoreManager *m_core_manager;
   StatsManager *m_stats_manager;
   HooksManager *m_hooks_manager;
   SyscallServer *m_syscall_server;
   ClockSkewMinimizationServer *m_clock_skew_minimization_server;

   Thread *createThread_unlocked(app_id_t app_id, thread_id_t creator_thread_id);
   void wakeUpWaiter(thread_id_t thread_id, SubsecondTime time);

   void updateRunningCount(Core::State old_status, Core::State new_status);
};

#endif // THREAD_MANAGER_H
