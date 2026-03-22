#include "syscall_model.h"
#include "circular_log.h"
#include "config.h"
#include "core.h"
#include "core_manager.h"
#include "hooks_manager.h"
#include "instruction.h"
#include "performance_model.h"
#include "pthread_emu.h"
#include "scheduler.h"
#include "simulator.h"
#include "stats.h"
#include "syscall_server.h"
#include "syscall_strings.h"
#include "thread.h"
#include "thread_manager.h"

#include <errno.h>
#include <linux/futex.h>
#include <sys/syscall.h>

#include "os_compat.h"

#include <boost/algorithm/string.hpp>

const char *SyscallMdl::futex_names[] = {"FUTEX_WAIT",          "FUTEX_WAKE",        "FUTEX_FD",
                                         "FUTEX_REQUEUE",       "FUTEX_CMP_REQUEUE", "FUTEX_WAKE_OP",
                                         "FUTEX_LOCK_PI",       "FUTEX_UNLOCK_PI",   "FUTEX_TRYLOCK_PI",
                                         "FUTEX_WAIT_BITSET",   "FUTEX_WAKE_BITSET", "FUTEX_WAIT_REQUEUE_PI",
                                         "FUTEX_CMP_REQUEUE_PI"};

SyscallMdl::SyscallMdl(Thread *thread)
    : m_thread(thread), m_emulated(false), m_stalled(false), m_ret_val(0), m_stdout_bytes(0), m_stderr_bytes(0)
{
   UInt32 futex_counters_size = sizeof(struct futex_counters_t);
   __attribute__((unused)) int rc = posix_memalign(
       (void **)&futex_counters, 64, futex_counters_size); // Align by cache line size to prevent thread contention
   LOG_ASSERT_ERROR(rc == 0, "posix_memalign failed to allocate memory");
   bzero(futex_counters, futex_counters_size);

   // Register the metrics
   for (unsigned int e = 0; e < sizeof(futex_names) / sizeof(futex_names[0]); e++) {
      registerStatsMetric("futex", thread->getId(), boost::to_lower_copy(String(futex_names[e]) + "_count"),
                          &(futex_counters->count[e]));
      registerStatsMetric("futex", thread->getId(), boost::to_lower_copy(String(futex_names[e]) + "_delay"),
                          &(futex_counters->delay[e]));
   }

   registerStatsMetric("syscall", thread->getId(), "stdout-bytes", &m_stdout_bytes);
   registerStatsMetric("syscall", thread->getId(), "stderr-bytes", &m_stderr_bytes);
}

SyscallMdl::~SyscallMdl()
{
   free(futex_counters);
}

bool SyscallMdl::runEnter(IntPtr syscall_number, syscall_args_t &args)
{
   Core *core = m_thread->getCore();
   LOG_ASSERT_ERROR(core != NULL, "Syscall by thread %d: core should not be null", m_thread->getId());

   LOG_PRINT("Got Syscall: %i", syscall_number);
   CLOG("syscall", "Enter thread %d core %d syscall %" PRIdPTR, m_thread->getId(), core->getId(), syscall_number);

   m_syscall_number = syscall_number;
   m_in_syscall = true;
   m_syscall_args = args;

   HookSyscallEnter hook_args;
   hook_args.thread_id = m_thread->getId();
   hook_args.core_id = core->getId();
   hook_args.time = core->getPerformanceModel()->getElapsedTime();
   hook_args.syscall_number = syscall_number;
   hook_args.args = args;
   Sim()->getHooksManager()->callHooks(HookType::HOOK_SYSCALL_ENTER, (UInt64)&hook_args);

   switch (syscall_number) {
   case SYS_futex:
      m_ret_val = handleFutexCall(args);
      m_emulated = true;
      break;

   case SYS_clock_gettime: {
      if (Sim()->getConfig()->getOSEmuClockReplace()) {
         clockid_t clock = (clockid_t)args.arg0;
         struct timespec *ts = (struct timespec *)args.arg1;
         UInt64 time_ns = Sim()->getConfig()->getOSEmuTimeStart() * 1000000000 +
                          m_thread->getCore()->getPerformanceModel()->getElapsedTime().getNS();

         switch (clock) {
         case CLOCK_REALTIME:
         case CLOCK_MONOTONIC:
         case CLOCK_MONOTONIC_COARSE:
         case CLOCK_MONOTONIC_RAW:
            ts->tv_sec = time_ns / 1000000000;
            ts->tv_nsec = time_ns % 1000000000;
            break;
         default:
            // return real time
            break;
         }
      }
      break;
   }

   case SYS_pause:
   case SYS_wait4: {
      // System call is blocking, mark thread as asleep
      Sim()->getThreadManager()->stallThread_async(
          m_thread->getId(), syscall_number == SYS_pause ? STALL_PAUSE : STALL_SYSCALL,
          m_thread->getCore()->getPerformanceModel()->getElapsedTime());
      m_stalled = true;
      break;
   }

   case SYS_sched_yield: {
      {
         ScopedLock sl(Sim()->getThreadManager()->getScheduler()->getLock());
         Sim()->getThreadManager()->getScheduler()->threadYield(m_thread->getId());
      }

      // We may have been rescheduled
      SubsecondTime time = core->getPerformanceModel()->getElapsedTime();
      if (m_thread->reschedule(time, core))
         core = m_thread->getCore();
      core->getPerformanceModel()->queuePseudoInstruction(new SyncInstruction(time, SyncInstruction::UNSCHEDULED));

      // Always succeeds
      m_ret_val = 0;
      m_emulated = true;

      break;
   }

   case SYS_sched_setaffinity: {
      pid_t pid = (pid_t)args.arg0;
      size_t cpusetsize = (size_t)args.arg1;
      const cpu_set_t *cpuset = (const cpu_set_t *)args.arg2;
      Thread *thread;

      if (cpuset == NULL) {
         m_ret_val = -EFAULT;
         m_emulated = true;
         break;
      }

      if (pid == 0)
         thread = m_thread;
      else
         thread = Sim()->getThreadManager()->findThreadByTid(pid);

      if (thread) {
         char *local_cpuset = new char[cpusetsize];
         core->accessMemory(Core::NONE, Core::READ, (IntPtr)cpuset, local_cpuset, cpusetsize);
         
         ScopedLock sl(Sim()->getThreadManager()->getScheduler()->getLock());
         Sim()->getThreadManager()->getScheduler()->threadSetAffinity(m_thread->getId(), thread->getId(), cpusetsize, (const cpu_set_t *)local_cpuset);
         delete[] local_cpuset;
      }

      m_ret_val = 0;
      m_emulated = true;
      break;
   }

   default:
      break;
   }

   LOG_PRINT("Syscall finished");
   CLOG("syscall", "Finished thread %d", m_thread->getId());

   return m_stalled;
}

IntPtr SyscallMdl::runExit(IntPtr old_return)
{
   CLOG("syscall", "Exit thread %d", m_thread->getId());

   if (m_stalled) {
      SubsecondTime time_wake = Sim()->getClockSkewMinimizationServer()->getGlobalTime(true /*upper_bound*/);

      {
         // System call is blocking, mark thread as awake
         Sim()->getThreadManager()->resumeThread_async(m_thread->getId(), INVALID_THREAD_ID, time_wake, NULL);
      }

      Core *core = Sim()->getCoreManager()->getCurrentCore();
      m_thread->reschedule(time_wake, core);
      core = m_thread->getCore();

      core->getPerformanceModel()->queuePseudoInstruction(new SyncInstruction(
          time_wake, m_syscall_number == SYS_pause ? SyncInstruction::PAUSE : SyncInstruction::SYSCALL));

      m_stalled = false;
   }

   if (!m_emulated) {
      m_ret_val = old_return;
   }

   Core *core = m_thread->getCore();
   HookSyscallExit hook_args;
   hook_args.thread_id = m_thread->getId();
   hook_args.core_id = core->getId();
   hook_args.time = core->getPerformanceModel()->getElapsedTime();
   hook_args.ret_val = m_ret_val;
   hook_args.emulated = m_emulated;
   Sim()->getHooksManager()->callHooks(HookType::HOOK_SYSCALL_EXIT, (UInt64)&hook_args);

   m_emulated = false;
   m_in_syscall = false;

   return m_ret_val;
}

IntPtr SyscallMdl::handleFutexCall(syscall_args_t &args)
{
   SyscallServer::futex_args_t futex_args;
   futex_args.uaddr = (int *)args.arg0;
   futex_args.op = (int)args.arg1;
   futex_args.val = (int)args.arg2;
   futex_args.timeout = (const struct timespec *)args.arg3;
   futex_args.uaddr2 = (int *)args.arg4;
   futex_args.val3 = (int)args.arg5;

   SubsecondTime curr_time = m_thread->getCore()->getPerformanceModel()->getElapsedTime();
   SubsecondTime end_time;

   IntPtr res = Sim()->getSyscallServer()->handleFutexCall(m_thread->getId(), futex_args, curr_time, end_time);

   // Update stats
   int cmd = (futex_args.op & FUTEX_CMD_MASK) & ~FUTEX_PRIVATE_FLAG;
   if (cmd >= 0 && cmd < (int)(sizeof(futex_names) / sizeof(futex_names[0]))) {
      futex_counters->count[cmd]++;
      futex_counters->delay[cmd] += (end_time - curr_time);
   }

   return res;
}

String SyscallMdl::formatSyscall() const
{
   return String(syscall_string(m_syscall_number));
}
