#ifndef __ROUTINE_TRACER_ONDEMAND_H
#define __ROUTINE_TRACER_ONDEMAND_H

#include "routine_tracer.h"
#include "simulation_context.h"

#include <unordered_map>

class RoutineTracerOndemand
{
 public:
   class RtnMaster : public RoutineTracer
   {
    public:
      RtnMaster(SimulationContext *context);
      virtual ~RtnMaster()
      {
      }

      virtual RoutineTracerThread *getThreadHandler(Thread *thread)
      {
         return new RtnThread(this, thread);
      }
      virtual void addRoutine(IntPtr eip, const char *name, const char *imgname, IntPtr offset, int column, int line,
                              const char *filename);
      virtual bool hasRoutine(IntPtr eip);
      RoutineTracer::Routine *getRoutine(IntPtr eip);

      HooksManager* getHooksManager() { return m_hooks_manager; }
      MagicServer* getMagicServer() { return m_magic_server; }
      SimulationContext* getContext() { return m_context; }

    private:
      static SInt64 signalHandler(UInt64, UInt64);

      Lock m_lock;
      std::unordered_map<IntPtr, RoutineTracer::Routine *> m_routines;
      SimulationContext *m_context;
      HooksManager *m_hooks_manager;
      MagicServer *m_magic_server;
   };

   class RtnThread : public RoutineTracerThread
   {
    public:
      RtnThread(RtnMaster *master, Thread *thread) 
          : RoutineTracerThread(thread, master->getHooksManager(), master->getMagicServer()), 
            m_master(master)
      {
      }

      void printStack();

    protected:
      virtual void functionEnter(IntPtr eip, IntPtr callEip)
      {
      }
      virtual void functionExit(IntPtr eip)
      {
      }
      virtual void functionChildEnter(IntPtr eip, IntPtr eip_child)
      {
      }
      virtual void functionChildExit(IntPtr eip, IntPtr eip_child)
      {
      }

    private:
      RtnMaster *m_master;
   };
};

#endif // __ROUTINE_TRACER_ONDEMAND_H
