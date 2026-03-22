#include "routine_tracer_ondemand.h"
#include "hooks_manager.h"
#include "simulator.h"
#include "syscall_model.h"
#include "thread.h"
#include "thread_manager.h"

#include <signal.h>

void RoutineTracerOndemand::RtnThread::printStack()
{
   Core::State state = Sim()->getThreadManager()->getThreadState(m_thread->getId());
   printf("Thread %d (app %d): %s", m_thread->getId(), m_thread->getAppId(), Core::CoreStateString(state));
   if (m_thread->getCore())
      printf(" on core %d", m_thread->getCore()->getId());
   else if (state == Core::STALLED)
      printf(" for %s",
             ThreadManager::stall_type_names[Sim()->getThreadManager()->getThreadStallReason(m_thread->getId())]);
   printf("\n");
   if (m_thread->getSyscallMdl()->inSyscall())
      printf("\tSyscall: %s\n", m_thread->getSyscallMdl()->formatSyscall().c_str());
   for (std::deque<IntPtr>::reverse_iterator it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
      printf("\t(%12" PRIxPTR ") %s\n", *it,
             m_master->getRoutine(*it) ? m_master->getRoutine(*it)->m_name : "(unknown)");
   }
   printf("\n");
}

RoutineTracerOndemand::RtnMaster::RtnMaster(SimulationContext *context)
    : m_context(context), m_hooks_manager(context->getHooksManager()), m_magic_server(context->getMagicServer())
{
   m_hooks_manager->registerHook(HookType::HOOK_SIGUSR1, signalHandler, (UInt64)this);
}

SInt64 RoutineTracerOndemand::RtnMaster::signalHandler(UInt64 self, UInt64)
{
   ScopedLock sl(((RtnMaster*)self)->m_thread_manager->getLock());

   for (thread_id_t thread_id = 0; thread_id < (thread_id_t)((RtnMaster*)self)->m_thread_manager->getNumThreads(); ++thread_id) {
      Thread *thread = ((RtnMaster*)self)->m_thread_manager->getThreadFromID(thread_id);
      RoutineTracerThread *tracer = thread->getRoutineTracer();
      RoutineTracerOndemand::RtnThread *ondemand_tracer = dynamic_cast<RoutineTracerOndemand::RtnThread *>(tracer);
      LOG_ASSERT_ERROR(ondemand_tracer, "Expected a routine tracer of type RoutineTracerOndemand::RtnThread");

      ondemand_tracer->printStack();
   }

   return 0;
}

void RoutineTracerOndemand::RtnMaster::addRoutine(IntPtr eip, const char *name, const char *imgname, IntPtr offset,
                                                  int column, int line, const char *filename)
{
   ScopedLock sl(m_lock);

   if (m_routines.count(eip) == 0) {
      m_routines[eip] = new RoutineTracer::Routine(eip, name, imgname, offset, column, line, filename);
   }
}

bool RoutineTracerOndemand::RtnMaster::hasRoutine(IntPtr eip)
{
   ScopedLock sl(m_lock);
   return m_routines.count(eip) > 0;
}

RoutineTracer::Routine *RoutineTracerOndemand::RtnMaster::getRoutine(IntPtr eip)
{
   ScopedLock sl(m_lock);
   return m_routines[eip];
}
