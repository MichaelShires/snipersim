#include "routine_tracer.h"
#include "config.hpp"
#include "core_manager.h"
#include "memory_tracker.h"
#include "routine_tracer_funcstats.h"
#include "routine_tracer_ondemand.h"
#include "routine_tracer_print.h"
#include "simulation_context.h"
#include "hooks_manager.h"
#include "magic_server.h"

RoutineTracer::Routine::Routine(IntPtr eip, const char *name, const char *imgname, IntPtr offset, int column, int line,
                                const char *filename)
    : m_eip(eip), m_name(strdup(name)), m_imgname(strdup(imgname)), m_filename(strdup(filename)), m_offset(offset),
      m_column(column), m_line(line)
{
   m_location = NULL;
}

void RoutineTracer::Routine::updateLocation(const char *name, const char *imgname, IntPtr offset, int column, int line,
                                           const char *filename)
{
   if (m_name)
      free(m_name);
   if (m_imgname)
      free(m_imgname);
   if (m_filename)
      free(m_filename);
   m_name = strdup(name);
   m_imgname = strdup(imgname);
   m_filename = strdup(filename);
   m_offset = offset;
   m_column = column;
   m_line = line;
}

RoutineTracer::RoutineTracer() : m_thread_manager(NULL)
{
}

RoutineTracer::~RoutineTracer()
{
}

RoutineTracer *RoutineTracer::create(SimulationContext *context)
{
   String type = context->getConfigFile()->getString("routine_tracer/type");

   if (type == "none")
      return NULL;
   else if (type == "print")
      return new RoutineTracerPrint::RtnMaster(context);
   else if (type == "ondemand")
      return new RoutineTracerOndemand::RtnMaster(context);
   else if (type == "funcstats")
      return new RoutineTracerFunctionStats::RtnMaster(context);
   else if (type == "memory_tracker")
      return new MemoryTracker::RoutineTracer(context);
   else
      LOG_PRINT_ERROR("Unknown routine tracer type %s", type.c_str());
}

RoutineTracerThread::RoutineTracerThread(Thread *thread, HooksManager *hooks_manager, MagicServer *magic_server)
    : m_thread(thread), m_last_esp(0), m_hooks_manager(hooks_manager), m_magic_server(magic_server)
{
   m_hooks_manager->registerHook(HookType::HOOK_APPLICATION_ROI_BEGIN, __hook_roi_begin, (UInt64)this);
   m_hooks_manager->registerHook(HookType::HOOK_APPLICATION_ROI_END, __hook_roi_end, (UInt64)this);
}

RoutineTracerThread::~RoutineTracerThread()
{
}

void RoutineTracerThread::routineEnter(IntPtr eip, IntPtr esp, IntPtr returnEip)
{
   ScopedLock sl(m_lock);
   routineEnter_unlocked(eip, esp, returnEip);
}

void RoutineTracerThread::routineEnter_unlocked(IntPtr eip, IntPtr esp, IntPtr callEip)
{
   if (m_magic_server->inROI()) {
      if (!m_stack.empty())
         functionChildEnter(m_stack.back(), eip);
      functionEnter(eip, callEip);
   }
   m_stack.push_back(eip);
   m_last_esp = esp;
}

void RoutineTracerThread::routineExit(IntPtr eip, IntPtr esp)
{
   ScopedLock sl(m_lock);
   if (unwindTo(eip)) {
      m_stack.pop_back();
      if (m_magic_server->inROI()) {
         functionExit(eip);
         if (!m_stack.empty())
            functionChildExit(m_stack.back(), eip);
      }
   }
   m_last_esp = esp;
}

void RoutineTracerThread::routineAssert(IntPtr eip, IntPtr esp)
{
   ScopedLock sl(m_lock);
   if (m_stack.empty() || m_stack.back() != eip) {
      // Something is wrong, maybe we missed an enter/exit
   }
}

bool RoutineTracerThread::unwindTo(IntPtr eip)
{
   while (!m_stack.empty() && m_stack.back() != eip) {
      IntPtr top = m_stack.back();
      m_stack.pop_back();
      if (m_magic_server->inROI()) {
         functionExit(top);
         if (!m_stack.empty())
            functionChildExit(m_stack.back(), top);
      }
   }
   return !m_stack.empty();
}

void RoutineTracerThread::hookRoiBegin()
{
   ScopedLock sl(m_lock);
   for (CallStack::iterator it = m_stack.begin(); it != m_stack.end(); ++it) {
      if (it != m_stack.begin())
         functionChildEnter(*(it - 1), *it);
      functionEnter(*it, 0);
   }
}

void RoutineTracerThread::hookRoiEnd()
{
   ScopedLock sl(m_lock);
   for (CallStack::reverse_iterator it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
      functionExit(*it);
      if ((it + 1) != m_stack.rend())
         functionChildExit(*(it + 1), *it);
   }
}
