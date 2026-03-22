#include "sync_server.h"
#include "sniper_exception.h"
#include "config.hpp"
#include "simulator.h"
#include "subsecond_time.h"
#include "sync_client.h"
#include "thread_manager.h"

// -- SimMutex -- //

SimMutex::SimMutex() : m_owner(NO_OWNER)
{
}

SimMutex::~SimMutex()
{
}

bool SimMutex::isLocked(thread_id_t thread_id)
{
   // Assumes m_lock is held by caller
   if (m_owner == NO_OWNER)
      return false;
   else if (m_owner == thread_id)
      return false;
   else
      return true;
}

SubsecondTime SimMutex::lock(thread_id_t thread_id, SubsecondTime time, ThreadManager *thread_manager)
{
   m_lock.acquire();
   if (m_owner == NO_OWNER) {
      m_owner = thread_id;
      m_lock.release();
      return time;
   }
   else {
      m_waiting.push(thread_id);
      m_lock.release();
      SubsecondTime res = thread_manager->stallThread(thread_id, STALL_MUTEX, time);
      return res;
   }
}

bool SimMutex::lock_async(thread_id_t thread_id, thread_id_t thread_by, SubsecondTime time, ThreadManager *thread_manager)
{
   m_lock.acquire();
   if (m_owner == NO_OWNER) {
      m_owner = thread_id;
      
      thread_manager->resumeThread(m_owner, thread_by, time);
      
      m_lock.release();
      return true;
   }
   else {
      m_waiting.push(thread_id);
      m_lock.release();
      return false;
   }
}

thread_id_t SimMutex::unlock(thread_id_t thread_id, SubsecondTime time, ThreadManager *thread_manager)
{
   m_lock.acquire();
   if (m_owner != thread_id) {
      m_lock.release();
      throw SimulationException("Attempted to unlock a mutex not owned by this thread.");
   }
   
   if (m_waiting.empty()) {
      m_owner = NO_OWNER;
      m_lock.release();
   }
   else {
      thread_id_t waiter = m_waiting.front();
      m_waiting.pop();
      m_owner = waiter;
      
      thread_manager->resumeThread(m_owner, thread_id, time);
      
      m_lock.release();
   }
   return m_owner;
}

// -- SimCond -- //
SimCond::SimCond()
{
}
SimCond::~SimCond()
{
}

SubsecondTime SimCond::wait(thread_id_t thread_id, SubsecondTime time, SimMutex *mux, ThreadManager *thread_manager)
{
   m_lock.acquire();
   m_waiting.push(CondWaiter(thread_id, mux));
   m_lock.release();
   
   // Release the mutex. It was held by caller.
   mux->unlock(thread_id, time, thread_manager);
   
   SubsecondTime res = thread_manager->stallThread(thread_id, STALL_COND, time);
   
   // Re-acquire the mutex
   mux->lock(thread_id, res, thread_manager);
   
   return res;
}

thread_id_t SimCond::signal(thread_id_t thread_id, SubsecondTime time, ThreadManager *thread_manager)
{
   m_lock.acquire();
   if (m_waiting.empty()) {
      m_lock.release();
      return INVALID_THREAD_ID;
   }
   else {
      CondWaiter waiter = m_waiting.front();
      m_waiting.pop();
      
      thread_manager->resumeThread(waiter.m_thread_id, thread_id, time);
      
      m_lock.release();
      return waiter.m_thread_id;
   }
}

void SimCond::broadcast(thread_id_t thread_id, SubsecondTime time, ThreadManager *thread_manager)
{
   m_lock.acquire();
   while (!m_waiting.empty()) {
      CondWaiter waiter = m_waiting.front();
      m_waiting.pop();
      
      thread_manager->resumeThread(waiter.m_thread_id, thread_id, time);
   }
   m_lock.release();
}

// -- SimBarrier -- //
SimBarrier::SimBarrier(UInt32 count) : m_count(count)
{
}
SimBarrier::~SimBarrier()
{
}

SubsecondTime SimBarrier::wait(thread_id_t thread_id, SubsecondTime time, ThreadManager *thread_manager)
{
   m_lock.acquire();
   // We are the last thread to reach the barrier
   if (m_waiting.size() == m_count - 1) {
      while (!m_waiting.empty()) {
         thread_id_t waiter = m_waiting.front();
         m_waiting.pop();
         thread_manager->resumeThread(waiter, thread_id, time);
      }
      m_lock.release();
      return time;
   }
   else {
      m_waiting.push(thread_id);
      m_lock.release();
      SubsecondTime res = thread_manager->stallThread(thread_id, STALL_BARRIER, time);
      return res;
   }
}

// -- SyncServer -- //

SyncServer::SyncServer(SimulationContext *context)
    : m_context(context), m_cfg(context->getConfigFile()), m_config(context->getConfig()), m_thread_manager(NULL)
{
   m_reschedule_cost = SubsecondTime::NS() * m_cfg->getInt("perf_model/sync/reschedule_cost");
}

SyncServer::~SyncServer()
{
}

SimMutex *SyncServer::getMutex(carbon_mutex_t *mux, bool canCreate)
{
   ScopedLock sl(m_mutexes_lock);
   if (m_mutexes.count(mux))
      return &m_mutexes[mux];
   else if (canCreate) {
      m_mutexes[mux] = SimMutex();
      return &m_mutexes[mux];
   }
   else {
      LOG_ASSERT_ERROR(false, "Invalid mutex id passed");
      return NULL;
   }
}

void SyncServer::mutexInit(thread_id_t thread_id, carbon_mutex_t *mux)
{
   getMutex(mux);
}

std::pair<SubsecondTime, bool> SyncServer::mutexLock(thread_id_t thread_id, carbon_mutex_t *mux, bool tryLock,
                                                     SubsecondTime time)
{
   SimMutex *psimmux = getMutex(mux);

   if (tryLock) {
      psimmux->acquire();
      if (psimmux->isLocked(thread_id)) {
         psimmux->release();
         return std::make_pair(time, false);
      }
      psimmux->setOwner(thread_id);
      psimmux->release();
      return std::make_pair(time + m_reschedule_cost, true);
   }
   else {
      SubsecondTime time_end = psimmux->lock(thread_id, time, m_thread_manager);
      return std::make_pair(time_end + m_reschedule_cost, true);
   }
}

SubsecondTime SyncServer::mutexUnlock(thread_id_t thread_id, carbon_mutex_t *mux, SubsecondTime time)
{
   SimMutex *psimmux = getMutex(mux, false);

   thread_id_t new_owner = psimmux->unlock(thread_id, time + m_reschedule_cost, m_thread_manager);

   SubsecondTime new_time =
       time +
       (new_owner == SimMutex::NO_OWNER ? SubsecondTime::Zero() : m_reschedule_cost /* we had to call futex_wake */);
   return new_time;
}

// -- Condition Variable Stuffs -- //
SimCond *SyncServer::getCond(carbon_cond_t *cond, bool canCreate)
{
   ScopedLock sl(m_conds_lock);
   if (m_conds.count(cond))
      return &m_conds[cond];
   else if (canCreate) {
      m_conds[cond] = SimCond();
      return &m_conds[cond];
   }
   else {
      LOG_ASSERT_ERROR(false, "Invalid cond id passed");
      return NULL;
   }
}

void SyncServer::condInit(thread_id_t thread_id, carbon_cond_t *cond)
{
   getCond(cond);
}

SubsecondTime SyncServer::condWait(thread_id_t thread_id, carbon_cond_t *cond, carbon_mutex_t *mux, SubsecondTime time)
{
   SimCond *psimcond = getCond(cond);
   SimMutex *psimmux = getMutex(mux, false);

   return psimcond->wait(thread_id, time, psimmux, m_thread_manager);
}

SubsecondTime SyncServer::condSignal(thread_id_t thread_id, carbon_cond_t *cond, SubsecondTime time)
{
   SimCond *psimcond = getCond(cond);
   psimcond->signal(thread_id, time, m_thread_manager);
   return time;
}

SubsecondTime SyncServer::condBroadcast(thread_id_t thread_id, carbon_cond_t *cond, SubsecondTime time)
{
   SimCond *psimcond = getCond(cond);
   psimcond->broadcast(thread_id, time, m_thread_manager);
   return time;
}

// -- Barrier Stuffs -- //
void SyncServer::barrierInit(thread_id_t thread_id, carbon_barrier_t *barrier, UInt32 count)
{
   ScopedLock sl(m_barriers_lock);
   m_barriers.push_back(SimBarrier(count));
   // Use the address of the newly created barrier as the id
   *barrier = (carbon_barrier_t)(m_barriers.size() - 1);
}

SubsecondTime SyncServer::barrierWait(thread_id_t thread_id, carbon_barrier_t *barrier, SubsecondTime time)
{
   m_barriers_lock.acquire();
   UInt32 id = (UInt32)*barrier;
   LOG_ASSERT_ERROR(id < m_barriers.size(), "Invalid barrier id: %d", id);
   SimBarrier *psimbarrier = &m_barriers[id];
   m_barriers_lock.release();

   return psimbarrier->wait(thread_id, time, m_thread_manager);
}
