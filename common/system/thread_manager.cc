#include "thread_manager.h"
#include "sniper_exception.h"
#include "circular_log.h"
#include "clock_skew_minimization_object.h"
#include "config.h"
#include "core.h"
#include "core_manager.h"
#include "hooks_manager.h"
#include "instruction.h"
#include "log.h"
#include "performance_model.h"
#include "scheduler.h"
#include "simulator.h"
#include "stats.h"
#include "syscall_server.h"
#include "thread.h"
#include "transport.h"

#include "os_compat.h"
#include <sys/syscall.h>

const char *ThreadManager::stall_type_names[] = {"unscheduled", "broken", "join",  "mutex", "cond",
                                                 "barrier",     "futex",  "pause", "sleep", "syscall"};
static_assert(STALL_TYPES_MAX == sizeof(ThreadManager::stall_type_names) / sizeof(char *),
               "Not enough values in ThreadManager::stall_type_names");


ThreadManager::ThreadManager(SimulationContext *context)
    : m_context(context), m_thread_tls(TLS::create()), m_scheduler(Scheduler::create(this)),
      m_num_running_threads(0),
      m_core_manager(context->getCoreManager()), m_stats_manager(context->getStatsManager()),
      m_hooks_manager(context->getHooksManager()), m_syscall_server(context->getSyscallServer()),
      m_clock_skew_minimization_server(context->getClockSkewMinimizationServer())
{
}

ThreadManager::~ThreadManager()
{
   for (UInt32 i = 0; i < m_threads.size(); i++) {
      delete m_threads[i];
   }

   delete m_thread_tls;
   delete m_scheduler;
}

void ThreadManager::updateRunningCount(Core::State old_status, Core::State new_status)
{
   bool old_running = (old_status == Core::RUNNING || old_status == Core::INITIALIZING);
   bool new_running = (new_status == Core::RUNNING || new_status == Core::INITIALIZING);

   if (old_running && !new_running)
      m_num_running_threads--;
   else if (!old_running && new_running)
      m_num_running_threads++;
}

Thread *ThreadManager::getThreadFromID(thread_id_t thread_id)
{
   LOG_ASSERT_ERROR((size_t)thread_id < m_threads.size(), "Invalid thread_id %d", thread_id);
   return m_threads.at(thread_id);
}
Thread *ThreadManager::getCurrentThread(int threadIndex)
{
   return m_thread_tls->getPtr<Thread>(threadIndex);
}

Thread *ThreadManager::findThreadByTid(pid_t tid)
{
   for (UInt32 thread_id = 0; thread_id < m_threads.size(); ++thread_id) {
      if (m_threads.at(thread_id)->m_os_info.tid == tid)
         return m_threads.at(thread_id);
   }
   return NULL;
}

Thread *ThreadManager::createThread(app_id_t app_id, thread_id_t creator_thread_id)
{
   ScopedLock sl(m_thread_lock);
   return createThread_unlocked(app_id, creator_thread_id);
}

Thread *ThreadManager::createThread_unlocked(app_id_t app_id, thread_id_t creator_thread_id)
{
   thread_id_t thread_id = m_threads.size();
   Thread *thread = new Thread(thread_id, app_id);
   m_threads.push_back(thread);
   
   Core::State old_status = thread->getStatus();
   thread->setStatus(Core::INITIALIZING);
   updateRunningCount(old_status, Core::INITIALIZING);

   core_id_t core_id = m_scheduler->threadCreate(thread_id);
   if (core_id != INVALID_CORE_ID) {
      Core *core = m_core_manager->getCoreFromID(core_id);
      thread->setCore(core);
      core->setState(Core::INITIALIZING);
   }

   m_stats_manager->logEvent(StatsManager::EVENT_THREAD_CREATE, SubsecondTime::MaxTime(), core_id, thread_id,
                                      app_id, creator_thread_id, "");

   HooksManager::ThreadCreate args = {thread_id : thread_id, creator_thread_id : creator_thread_id};
   m_hooks_manager->callHooks(HookType::HOOK_THREAD_CREATE, (UInt64)&args);
   CLOG("thread", "Create %d", thread_id);

   return thread;
}

void ThreadManager::onThreadStart(thread_id_t thread_id, SubsecondTime time)
{
   ScopedLock sl(m_thread_lock);
   LOG_PRINT("onThreadStart(%i)", thread_id);

   Thread *thread = getThreadFromID(thread_id);

   m_thread_tls->set(thread);
   thread->updateCoreTLS();

   // Set thread state to running for the duration of HOOK_THREAD_START, we'll move it to stalled later on if it didn't
   // have a core
   Core::State old_status = thread->getStatus();
   thread->setStatus(Core::RUNNING);
   updateRunningCount(old_status, Core::RUNNING);

   HooksManager::ThreadTime args = {thread_id : thread_id, time : time};
   m_hooks_manager->callHooks(HookType::HOOK_THREAD_START, (UInt64)&args);
   // Note: we may have been rescheduled during HOOK_THREAD_START
   // (Happens if core was occupied during our createThread() but became free since then)
   CLOG("thread", "Start %d", thread_id);

   Core *core = thread->getCore();
   if (core) {
      // Set the CoreState to 'RUNNING'
      core->setState(Core::RUNNING);

      PerformanceModel *pm = core->getPerformanceModel();
      // If the core already has a later time, we have to wait
      time = std::max(time, pm->getElapsedTime());
      pm->queuePseudoInstruction(new SpawnInstruction(time));

      LOG_PRINT("Setting status[%i] -> RUNNING", thread_id);
      // Status is already RUNNING
   }
   else {
      old_status = thread->getStatus();
      thread->setStatus(Core::STALLED);
      thread->setStalledReason(STALL_UNSCHEDULED);
      updateRunningCount(old_status, Core::STALLED);
   }

   if (thread->getWaiter() != INVALID_THREAD_ID) {
      getThreadFromID(thread->getWaiter())->signal(time);
      thread->setWaiter(INVALID_THREAD_ID);
   }
}

void ThreadManager::onThreadExit(thread_id_t thread_id)
{
   ScopedLock sl(m_thread_lock);

   Thread *thread = getThreadFromID(thread_id);
   Core *core = thread->getCore();
   LOG_ASSERT_ERROR(core != NULL, "Thread ended while not running on a core?");

#include "sniper_exception.h"

   SubsecondTime time = core->getPerformanceModel()->getElapsedTime();

   if (thread->getStatus() != Core::RUNNING) {
      throw SimulationException("Thread ended while not in RUNNING state.");
   }
   
   Core::State old_status = thread->getStatus();
   thread->setStatus(Core::IDLE);
   updateRunningCount(old_status, Core::IDLE);

   // Implement pthread_join
   wakeUpWaiter(thread_id, time);

   // Implement CLONE_CHILD_CLEARTID
   if (thread->m_os_info.clear_tid) {
      uint32_t zero = 0;
      core->accessMemory(Core::NONE, Core::WRITE, thread->m_os_info.tid_ptr, (char *)&zero, sizeof(zero));

      SubsecondTime end_time; // ignored
      m_syscall_server->futexWake(thread_id, (int *)thread->m_os_info.tid_ptr, 1, FUTEX_BITSET_MATCH_ANY, time,
                                           end_time);
   }

   // Set the CoreState to 'IDLE'
   core->setState(Core::IDLE);

   m_thread_tls->set(NULL);
   thread->setCore(NULL);
   thread->updateCoreTLS();

   m_stats_manager->logEvent(StatsManager::EVENT_THREAD_EXIT, SubsecondTime::MaxTime(), core->getId(),
                                      thread_id, 0, 0, "");

   HooksManager::ThreadTime args = {thread_id : thread_id, time : time};
   m_hooks_manager->callHooks(HookType::HOOK_THREAD_EXIT, (UInt64)&args);
   CLOG("thread", "Exit %d", thread_id);
}

thread_id_t ThreadManager::spawnThread(thread_id_t thread_id, app_id_t app_id)
{
   ScopedLock sl(getLock());

   SubsecondTime time_start = SubsecondTime::Zero();
   if (thread_id != INVALID_THREAD_ID) {
      Thread *thread = getThreadFromID(thread_id);
      Core *core = thread->getCore();
      time_start = core->getPerformanceModel()->getElapsedTime();
   }

   Thread *new_thread = createThread_unlocked(app_id, thread_id);

   // Insert the request in the thread request queue
   ThreadSpawnRequest req = {thread_id, new_thread->getId(), time_start};
   m_thread_spawn_list.push(req);

   LOG_PRINT("Done with (2)");

   return new_thread->getId();
}

thread_id_t ThreadManager::getThreadToSpawn(SubsecondTime &time)
{
   ScopedLock sl(getLock());

   LOG_ASSERT_ERROR(!m_thread_spawn_list.empty(), "Have no thread to spawn");

   ThreadSpawnRequest req = m_thread_spawn_list.front();
   m_thread_spawn_list.pop();

   time = req.time;
   return req.thread_id;
}

void ThreadManager::waitForThreadStart(thread_id_t thread_id, thread_id_t wait_thread_id)
{
   ScopedLock sl(getLock());
   Thread *self = getThreadFromID(thread_id);
   Thread *wait_thread = getThreadFromID(wait_thread_id);

   if (wait_thread->getStatus() == Core::INITIALIZING) {
      LOG_ASSERT_ERROR(wait_thread->getWaiter() == INVALID_THREAD_ID,
                       "Multiple threads waiting for thread: %d", wait_thread_id);

      wait_thread->setWaiter(thread_id);
      self->wait();
   }
}

void ThreadManager::moveThread(thread_id_t thread_id, core_id_t core_id, SubsecondTime time)
{
   Thread *thread = getThreadFromID(thread_id);
   CLOG("thread", "Move %d from %d to %d", thread_id, thread->getCore() ? thread->getCore()->getId() : -1, core_id);

   if (Core *core = thread->getCore())
      core->setState(Core::IDLE);

   if (core_id == INVALID_CORE_ID) {
      thread->setCore(NULL);
   }
   else {
      if (thread->getCore() == NULL) {
         // Unless thread was stalled for sync/futex/..., wake it up
         if (thread->getStatus() == Core::STALLED &&
             thread->getStalledReason() == STALL_UNSCHEDULED)
            resumeThread(thread_id, INVALID_THREAD_ID, time);
      }

      Core *core = m_core_manager->getCoreFromID(core_id);
      thread->setCore(core);
      if (thread->getStatus() != Core::STALLED)
         core->setState(Core::RUNNING);
   }

   HooksManager::ThreadMigrate args = {thread_id : thread_id, core_id : core_id, time : time};
   m_hooks_manager->callHooks(HookType::HOOK_THREAD_MIGRATE, (UInt64)&args);
}

bool ThreadManager::areAllCoresRunning()
{
   // Check if all the cores are running
   for (thread_id_t thread_id = 0; thread_id < (thread_id_t)m_threads.size(); thread_id++) {
      if (getThreadFromID(thread_id)->getStatus() == Core::IDLE) {
         return false;
      }
   }

   return true;
}

void ThreadManager::joinThread(thread_id_t thread_id, thread_id_t join_thread_id)
{
   Thread *thread = getThreadFromID(thread_id);
   Thread *join_thread = getThreadFromID(join_thread_id);
   Core *core = thread->getCore();
   SubsecondTime end_time;

   LOG_PRINT("Joining on thread: %d", join_thread_id);

   {
      ScopedLock sl(join_thread->getLock());

      if (join_thread->getStatus() == Core::IDLE) {
         LOG_PRINT("Not running.");
         return;
      }

      SubsecondTime start_time = core->getPerformanceModel()->getElapsedTime();

      LOG_ASSERT_ERROR(join_thread->getWaiter() == INVALID_THREAD_ID,
                       "Multiple threads joining on thread: %d", join_thread_id);

      join_thread->setWaiter(thread_id);
      end_time = stallThread(thread_id, STALL_JOIN, start_time);
   }

   if (thread->reschedule(end_time, core))
      core = thread->getCore();

   core->getPerformanceModel()->queuePseudoInstruction(new SyncInstruction(end_time, SyncInstruction::JOIN));

   LOG_PRINT("Exiting join thread.");
}

void ThreadManager::wakeUpWaiter(thread_id_t thread_id, SubsecondTime time)
{
   Thread *thread = getThreadFromID(thread_id);
   if (thread->getWaiter() != INVALID_THREAD_ID) {
      LOG_PRINT("Waking up core: %d at time: %s", thread->getWaiter(), itostr(time).c_str());

      // Resume the 'pthread_join' caller
      resumeThread(thread->getWaiter(), thread_id, time);

      thread->setWaiter(INVALID_THREAD_ID);
   }
   LOG_PRINT("Exiting wakeUpWaiter");
}

void ThreadManager::stallThread_async(thread_id_t thread_id, stall_type_t reason, SubsecondTime time)
{
   Thread *thread = getThreadFromID(thread_id);
   ScopedLock sl(thread->getLock());

   LOG_PRINT("Core(%i) -> STALLED", thread_id);
   Core::State old_status = thread->getStatus();
   thread->setStatus(Core::STALLED);
   thread->setStalledReason(reason);
   updateRunningCount(old_status, Core::STALLED);

   HooksManager::ThreadStall args = {thread_id : thread_id, reason : reason, time : time};
   m_hooks_manager->callHooks(HookType::HOOK_THREAD_STALL, (UInt64)&args);
   CLOG("thread", "Stall %d (%s)", thread_id, ThreadManager::stall_type_names[reason]);
}

SubsecondTime ThreadManager::stallThread(thread_id_t thread_id, stall_type_t reason, SubsecondTime time)
{
   stallThread_async(thread_id, reason, time);
   
   // When all threads are stalled, we have a deadlock -- unless we let the barrier advance time
   while (m_num_running_threads.load() == 0) {
      m_clock_skew_minimization_server->advance();
   }
   
   Thread *thread = getThreadFromID(thread_id);
   if (thread->getStatus() == Core::RUNNING)
      return thread->getWakeupTime();
   else
      return thread->wait();
}

void ThreadManager::resumeThread_async(thread_id_t thread_id, thread_id_t thread_by, SubsecondTime time, void *msg)
{
   Thread *thread = getThreadFromID(thread_id);
   ScopedLock sl(thread->getLock());

   LOG_PRINT("Core(%i) -> RUNNING", thread_id);
   Core::State old_status = thread->getStatus();
   thread->setStatus(Core::RUNNING);
   updateRunningCount(old_status, Core::RUNNING);

   HooksManager::ThreadResume args = {thread_id : thread_id, thread_by : thread_by, time : time};
   m_hooks_manager->callHooks(HookType::HOOK_THREAD_RESUME, (UInt64)&args);
   CLOG("thread", "Resume %d (by %d)", thread_id, thread_by);
}

void ThreadManager::resumeThread(thread_id_t thread_id, thread_id_t thread_by, SubsecondTime time, void *msg)
{
   getThreadFromID(thread_id)->signal(time, msg);

   resumeThread_async(thread_id, thread_by, time, msg);
}

bool ThreadManager::isThreadRunning(thread_id_t thread_id)
{
   return (getThreadFromID(thread_id)->getStatus() == Core::RUNNING);
}

bool ThreadManager::isThreadInitializing(thread_id_t thread_id)
{
   return (getThreadFromID(thread_id)->getStatus() == Core::INITIALIZING);
}

bool ThreadManager::anyThreadRunning()
{
   return m_num_running_threads.load() > 0;
}
