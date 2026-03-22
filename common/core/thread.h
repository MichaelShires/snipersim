#ifndef __THREAD_H
#define __THREAD_H

#include "cond.h"
#include "subsecond_time.h"
#include "core.h"
#include "stall_types.h"

class Core;
class SyscallMdl;
class SyncClient;
class RoutineTracerThread;

class Thread
{
 public:
   typedef UInt64 (*va2pa_func_t)(UInt64 arg, UInt64 va);

   struct ThreadState
   {
      Core::State status;
      stall_type_t stalled_reason;
      thread_id_t waiter;
      ThreadState() : status(Core::IDLE), stalled_reason(STALL_UNSCHEDULED), waiter(INVALID_THREAD_ID) {}
   };

 private:
   thread_id_t m_thread_id;
   app_id_t m_app_id;
   String m_name;

   ConditionVariable m_cond;
   Lock m_lock;
   ThreadState m_state;

   SubsecondTime m_wakeup_time;
   void *m_wakeup_msg;
   Core *m_core;
   SyscallMdl *m_syscall_model;
   SyncClient *m_sync_client;
   RoutineTracerThread *m_rtn_tracer;
   va2pa_func_t m_va2pa_func;
   UInt64 m_va2pa_arg;

 public:
   Thread(thread_id_t thread_id, app_id_t app_id);
   ~Thread();

   struct
   {
      pid_t tid;
      IntPtr tid_ptr;
      bool clear_tid;
   } m_os_info;

   thread_id_t getId() const
   {
      return m_thread_id;
   }
   app_id_t getAppId() const
   {
      return m_app_id;
   }

   String getName() const
   {
      return m_name;
   }
   void setName(String name)
   {
      m_name = name;
   }

   SyncClient *getSyncClient() const
   {
      return m_sync_client;
   }
   RoutineTracerThread *getRoutineTracer() const
   {
      return m_rtn_tracer;
   }

   void setVa2paFunc(va2pa_func_t va2pa_func, UInt64 m_va2pa_arg);
   UInt64 va2pa(UInt64 logical_address) const
   {
      if (m_va2pa_func)
         return m_va2pa_func(m_va2pa_arg, logical_address);
      else
         return logical_address;
   }

   SubsecondTime wait()
   {
      ScopedLock sl(m_lock);
      m_wakeup_msg = NULL;
      while(m_state.status == Core::STALLED || m_state.status == Core::INITIALIZING)
         m_cond.wait(m_lock);
      return m_wakeup_time;
   }
   void signal(SubsecondTime time, void *msg = NULL)
   {
      ScopedLock sl(m_lock);
      m_wakeup_time = time;
      m_wakeup_msg = msg;
      m_cond.signal();
   }
   SubsecondTime getWakeupTime() const
   {
      return m_wakeup_time;
   }
   void *getWakeupMsg() const
   {
      return m_wakeup_msg;
   }

   Core *getCore() const
   {
      return m_core;
   }
   void setCore(Core *core);

   bool reschedule(SubsecondTime &time, Core *current_core);
   bool updateCoreTLS(int threadIndex = -1);

   SyscallMdl *getSyscallMdl()
   {
      return m_syscall_model;
   }

   Lock& getLock() { return m_lock; }
   ThreadState& getState() { return m_state; }
   Core::State getStatus() const { return m_state.status; }
   void setStatus(Core::State status) { m_state.status = status; }
   stall_type_t getStalledReason() const { return m_state.stalled_reason; }
   void setStalledReason(stall_type_t reason) { m_state.stalled_reason = reason; }
   thread_id_t getWaiter() const { return m_state.waiter; }
   void setWaiter(thread_id_t waiter) { m_state.waiter = waiter; }
};

#endif // __THREAD_H
