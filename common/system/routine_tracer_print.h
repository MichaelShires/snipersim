#ifndef __ROUTINE_TRACER_PRINT_H
#define __ROUTINE_TRACER_PRINT_H

#include "routine_tracer.h"
#include "simulation_context.h"

#include <unordered_map>

class RoutineTracerPrint
{
 public:
   class RtnMaster : public RoutineTracer
   {
    public:
      RtnMaster(SimulationContext *context)
          : m_context(context), m_hooks_manager(context->getHooksManager()), m_magic_server(context->getMagicServer())
      {
      }
      virtual ~RtnMaster()
      {
      }

      virtual RoutineTracerThread *getThreadHandler(Thread *thread);
      virtual void addRoutine(IntPtr eip, const char *name, const char *imgname, IntPtr offset, int column, int line,
                              const char *filename);
      virtual bool hasRoutine(IntPtr eip);
      RoutineTracer::Routine *getRoutine(IntPtr eip);

      HooksManager* getHooksManager() { return m_hooks_manager; }
      MagicServer* getMagicServer() { return m_magic_server; }
      SimulationContext* getContext() { return m_context; }

    private:
      Lock m_lock;
      std::unordered_map<IntPtr, RoutineTracer::Routine *> m_routines;
      SimulationContext *m_context;
      HooksManager *m_hooks_manager;
      MagicServer *m_magic_server;
   };

   class RtnThread : public RoutineTracerThread
   {
    public:
      RtnThread(RtnMaster *master, Thread *thread);

    protected:
      virtual void functionEnter(IntPtr eip, IntPtr callEip);
      virtual void functionExit(IntPtr eip);
      virtual void functionChildEnter(IntPtr eip, IntPtr eip_child)
      {
      }
      virtual void functionChildExit(IntPtr eip, IntPtr eip_child)
      {
      }

    private:
      RtnMaster *m_master;
      int m_depth;
   };
};

#endif // __ROUTINE_TRACER_PRINT_H
