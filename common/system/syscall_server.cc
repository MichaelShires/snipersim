#include "syscall_server.h"
#include "circular_log.h"
#include "config.h"
#include "config.hpp"
#include "core.h"
#include "core_manager.h"
#include "hooks_manager.h"
#include "log.h"
#include "simulator.h"
#include "thread.h"
#include "thread_manager.h"

#include "os_compat.h"
#include <sys/syscall.h>

SyscallServer::SyscallServer(SimulationContext *context)
    : m_context(context), m_cfg(context->getConfigFile()), m_config(context->getConfig()),
      m_hooks_manager(context->getHooksManager()), m_thread_manager(NULL)
{
   m_reschedule_cost = SubsecondTime::NS() * m_cfg->getInt("perf_model/sync/reschedule_cost");

   m_hooks_manager->registerHook(HookType::HOOK_PERIODIC, hook_periodic, (UInt64)this);
}

SyscallServer::~SyscallServer()
{
}

void SyscallServer::handleSleepCall(thread_id_t thread_id, SubsecondTime wake_time, SubsecondTime curr_time,
                                    SubsecondTime &end_time)
{
   m_sleeping_lock.acquire();
   m_sleeping.push_back(SimFutex::Waiter(thread_id, 0, wake_time));
   m_sleeping_lock.release();

   end_time = m_thread_manager->stallThread(thread_id, STALL_SLEEP, curr_time);
}

IntPtr SyscallServer::handleFutexCall(thread_id_t thread_id, futex_args_t &args, SubsecondTime curr_time,
                                      SubsecondTime &end_time)
{
   CLOG("futex", "Futex enter thread %d", thread_id);

   int cmd = (args.op & FUTEX_CMD_MASK) & ~FUTEX_PRIVATE_FLAG;
   SubsecondTime timeout_time = SubsecondTime::MaxTime();
   LOG_PRINT("Futex syscall: uaddr(0x%x), op(%u), val(%u)", args.uaddr, args.op, args.val);

   Core *core = m_thread_manager->getThreadFromID(thread_id)->getCore();
   LOG_ASSERT_ERROR(core != NULL, "Core should not be NULL");

   if (cmd == FUTEX_WAIT || cmd == FUTEX_WAIT_BITSET) {
      if (args.timeout != NULL) {
         struct timespec timeout_buf;
         core->accessMemory(Core::NONE, Core::READ, (IntPtr)args.timeout, (char *)&timeout_buf, sizeof(timeout_buf));
         SubsecondTime timeout_val = SubsecondTime::SEC(timeout_buf.tv_sec - m_config->getOSEmuTimeStart()) +
                                     SubsecondTime::NS(timeout_buf.tv_nsec);
         if (cmd == FUTEX_WAIT_BITSET) {
            timeout_time = timeout_val;
         }
         else {
            timeout_time = curr_time + timeout_val;
         }
      }
   }

   int act_val;
   IntPtr res;

   core->accessMemory(Core::NONE, Core::READ, (IntPtr)args.uaddr, (char *)&act_val, sizeof(act_val));

   if (cmd == FUTEX_WAIT || cmd == FUTEX_WAIT_BITSET) {
      res = futexWait(thread_id, args.uaddr, args.val, act_val,
                      cmd == FUTEX_WAIT_BITSET ? args.val3 : FUTEX_BITSET_MATCH_ANY, curr_time, timeout_time, end_time);
   }
   else if (cmd == FUTEX_WAKE || cmd == FUTEX_WAKE_BITSET) {
      res = futexWake(thread_id, args.uaddr, args.val, cmd == FUTEX_WAIT_BITSET ? args.val3 : FUTEX_BITSET_MATCH_ANY,
                      curr_time, end_time);
   }
   else if (cmd == FUTEX_WAKE_OP) {
      res = futexWakeOp(thread_id, args.uaddr, args.uaddr2, args.val, args.val2, args.val3, curr_time, end_time);
   }
   else if (cmd == FUTEX_CMP_REQUEUE) {
      res = futexCmpRequeue(thread_id, args.uaddr, args.val, args.uaddr2, args.val3, act_val, curr_time, end_time);
   }
   else {
      LOG_PRINT_ERROR("Unknown SYS_futex cmd %d", cmd);
   }

   CLOG("futex", "Futex leave thread %d", thread_id);
   return res;
}

IntPtr SyscallServer::futexWait(thread_id_t thread_id, int *uaddr, int val, int act_val, int mask,
                                SubsecondTime curr_time, SubsecondTime timeout_time, SubsecondTime &end_time)
{
   SimFutex *sim_futex = findFutexByUaddr(uaddr, thread_id);

   if (val != act_val) {
      end_time = curr_time;
      return -EAGAIN;
   }

   bool res = sim_futex->enqueueWaiter(thread_id, mask, curr_time, timeout_time, end_time, m_thread_manager);
   return res ? 0 : -ETIMEDOUT;
}

IntPtr SyscallServer::futexWake(thread_id_t thread_id, int *uaddr, int nr_wake, int mask, SubsecondTime curr_time,
                                SubsecondTime &end_time)
{
   SimFutex *sim_futex = findFutexByUaddr(uaddr, thread_id);

   int num_procs_woken_up = 0;
   while (num_procs_woken_up < nr_wake) {
      thread_id_t woken_thread_id = sim_futex->dequeueWaiter(thread_id, mask, curr_time, m_thread_manager);
      if (woken_thread_id == INVALID_THREAD_ID)
         break;
      num_procs_woken_up++;
   }

   end_time = curr_time;
   return num_procs_woken_up;
}

IntPtr SyscallServer::futexWakeOp(thread_id_t thread_id, int *uaddr, int *uaddr2, int nr_wake, int nr_wake2, int op,
                                  SubsecondTime curr_time, SubsecondTime &end_time)
{
   SimFutex *sim_futex = findFutexByUaddr(uaddr, thread_id);
   SimFutex *sim_futex2 = findFutexByUaddr(uaddr2, thread_id);

   int num_procs_woken_up = 0;
   while (num_procs_woken_up < nr_wake) {
      thread_id_t woken_thread_id = sim_futex->dequeueWaiter(thread_id, FUTEX_BITSET_MATCH_ANY, curr_time, m_thread_manager);
      if (woken_thread_id == INVALID_THREAD_ID)
         break;
      num_procs_woken_up++;
   }

   Core *core = m_thread_manager->getThreadFromID(thread_id)->getCore();
   if (futexDoOp(core, op, uaddr2)) {
      while (num_procs_woken_up < (nr_wake + nr_wake2)) {
         thread_id_t woken_thread_id =
             sim_futex2->dequeueWaiter(thread_id, FUTEX_BITSET_MATCH_ANY, curr_time, m_thread_manager);
         if (woken_thread_id == INVALID_THREAD_ID)
            break;
         num_procs_woken_up++;
      }
   }

   end_time = curr_time;
   return num_procs_woken_up;
}

IntPtr SyscallServer::futexCmpRequeue(thread_id_t thread_id, int *uaddr, int val, int *uaddr2, int nr_requeue,
                                      int act_val, SubsecondTime curr_time, SubsecondTime &end_time)
{
   SimFutex *sim_futex = findFutexByUaddr(uaddr, thread_id);
   SimFutex *sim_futex2 = findFutexByUaddr(uaddr2, thread_id);

   if (val != act_val) {
      end_time = curr_time;
      return -EAGAIN;
   }

   int num_procs_woken_up = 0;
   // Wake up 1 thread
   thread_id_t woken_thread_id = sim_futex->dequeueWaiter(thread_id, FUTEX_BITSET_MATCH_ANY, curr_time, m_thread_manager);
   if (woken_thread_id != INVALID_THREAD_ID)
      num_procs_woken_up++;

   // Requeue nr_requeue threads
   int num_procs_requeued = 0;
   
   // Locking both futexes for requeue.
   // To avoid deadlock, we should ideally have an order. uaddr < uaddr2?
   SimFutex *f1 = sim_futex;
   SimFutex *f2 = sim_futex2;
   if (f1 > f2) std::swap(f1, f2);
   
   f1->acquire();
   f2->acquire();
   while (num_procs_requeued < nr_requeue) {
      thread_id_t requeued_thread_id = sim_futex->requeueWaiter(sim_futex2); // This currently doesn't use internal locks
      if (requeued_thread_id == INVALID_THREAD_ID)
         break;
      num_procs_requeued++;
   }
   f2->release();
   f1->release();

   end_time = curr_time;
   return num_procs_woken_up + num_procs_requeued;
}

SimFutex *SyscallServer::findFutexByUaddr(int *uaddr, thread_id_t thread_id)
{
   IntPtr address = m_thread_manager->getThreadFromID(thread_id)->va2pa((IntPtr)uaddr);
   ScopedLock sl(m_futexes_lock);
   return &m_futexes[address];
}

int SyscallServer::futexDoOp(Core *core, int op, int *uaddr)
{
   int old_val, new_val;
   int op_arg = (op >> 12) & 0xfff;
   int cmp_arg = (op >> 20) & 0xfff;
   int op_type = (op >> 4) & 0xf;
   int cmp_type = op & 0xf;

   core->accessMemory(Core::NONE, Core::READ, (IntPtr)uaddr, (char *)&old_val, sizeof(old_val));

   switch (op_type) {
   case FUTEX_OP_SET:
      new_val = op_arg;
      break;
   case FUTEX_OP_ADD:
      new_val = old_val + op_arg;
      break;
   case FUTEX_OP_OR:
      new_val = old_val | op_arg;
      break;
   case FUTEX_OP_ANDN:
      new_val = old_val & ~op_arg;
      break;
   case FUTEX_OP_XOR:
      new_val = old_val ^ op_arg;
      break;
   default:
      LOG_PRINT_ERROR("Unknown futex op_type %d", op_type);
   }

   core->accessMemory(Core::NONE, Core::WRITE, (IntPtr)uaddr, (char *)&new_val, sizeof(new_val));

   switch (cmp_type) {
   case FUTEX_OP_CMP_EQ:
      return (old_val == cmp_arg);
   case FUTEX_OP_CMP_NE:
      return (old_val != cmp_arg);
   case FUTEX_OP_CMP_LT:
      return (old_val < cmp_arg);
   case FUTEX_OP_CMP_LE:
      return (old_val <= cmp_arg);
   case FUTEX_OP_CMP_GT:
      return (old_val > cmp_arg);
   case FUTEX_OP_CMP_GE:
      return (old_val >= cmp_arg);
   default:
      LOG_PRINT_ERROR("Unknown futex cmp_type %d", cmp_type);
   }
}

void SyscallServer::futexPeriodic(SubsecondTime time)
{
   m_sleeping_lock.acquire();
   for (SimFutex::ThreadQueue::iterator it = m_sleeping.begin(); it != m_sleeping.end(); ) {
      if (it->timeout <= time) {
         thread_id_t waiter = it->thread_id;
         it = m_sleeping.erase(it);

         m_thread_manager->resumeThread(waiter, waiter, time, (void *)false);
      } else {
         ++it;
      }
   }
   m_sleeping_lock.release();

   m_futexes_lock.acquire();
   for (FutexMap::iterator it = m_futexes.begin(); it != m_futexes.end(); ++it) {
      it->second.wakeTimedOut(time, m_thread_manager);
   }
   m_futexes_lock.release();
}

SubsecondTime SyscallServer::getNextTimeout(SubsecondTime time)
{
   SubsecondTime next = SubsecondTime::MaxTime();
   
   m_sleeping_lock.acquire();
   for (SimFutex::ThreadQueue::iterator it = m_sleeping.begin(); it != m_sleeping.end(); ++it) {
      if (it->timeout < next)
         next = it->timeout;
   }
   m_sleeping_lock.release();

   m_futexes_lock.acquire();
   for (FutexMap::iterator it = m_futexes.begin(); it != m_futexes.end(); ++it) {
      SubsecondTime t = it->second.getNextTimeout(time);
      if (t < next)
         next = t;
   }
   m_futexes_lock.release();
   
   return next;
}

// -- SimFutex -- //
SimFutex::SimFutex()
{
}

SimFutex::~SimFutex()
{
}

bool SimFutex::enqueueWaiter(thread_id_t thread_id, int mask, SubsecondTime time, SubsecondTime timeout_time,
                             SubsecondTime &time_end, ThreadManager *thread_manager)
{
   m_lock.acquire();
   m_waiting.push_back(Waiter(thread_id, mask, timeout_time));
   m_lock.release();
   
   time_end = thread_manager->stallThread(thread_id, STALL_FUTEX, time);
   
   return thread_manager->getThreadFromID(thread_id)->getWakeupMsg();
}

thread_id_t SimFutex::dequeueWaiter(thread_id_t thread_by, int mask, SubsecondTime time, ThreadManager *thread_manager)
{
   m_lock.acquire();
   if (m_waiting.empty()) {
      m_lock.release();
      return INVALID_THREAD_ID;
   }
   else {
      for (ThreadQueue::iterator it = m_waiting.begin(); it != m_waiting.end(); ++it) {
         if (mask & it->mask) {
            thread_id_t waiter = it->thread_id;
            m_waiting.erase(it);

            thread_manager->resumeThread(waiter, thread_by, time, (void *)true);
            
            m_lock.release();
            return waiter;
         }
      }
      m_lock.release();
      return INVALID_THREAD_ID;
   }
}

thread_id_t SimFutex::requeueWaiter(SimFutex *requeue_futex)
{
   // Assumes both f1->m_lock and f2->m_lock are held by caller (e.g. in futexCmpRequeue)
   if (m_waiting.empty())
      return INVALID_THREAD_ID;
   else {
      Waiter waiter = m_waiting.front();
      m_waiting.pop_front();
      requeue_futex->m_waiting.push_back(waiter);

      return waiter.thread_id;
   }
}

void SimFutex::wakeTimedOut(SubsecondTime time, ThreadManager *thread_manager)
{
   m_lock.acquire();
   for (ThreadQueue::iterator it = m_waiting.begin(); it != m_waiting.end(); ) {
      if (it->timeout <= time) {
         thread_id_t waiter = it->thread_id;
         it = m_waiting.erase(it);

         thread_manager->resumeThread(waiter, INVALID_THREAD_ID, time, (void *)false);
      } else {
         ++it;
      }
   }
   m_lock.release();
}

SubsecondTime SimFutex::getNextTimeout(SubsecondTime time)
{
   SubsecondTime next = SubsecondTime::MaxTime();
   m_lock.acquire();
   for (ThreadQueue::iterator it = m_waiting.begin(); it != m_waiting.end(); ++it) {
      if (it->timeout < next)
         next = it->timeout;
   }
   m_lock.release();
   return next;
}
