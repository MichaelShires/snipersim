#ifndef __ROUTINE_TRACER_FUNCSTATS_H
#define __ROUTINE_TRACER_FUNCSTATS_H

#include "cache_efficiency_tracker.h"
#include "routine_tracer.h"
#include "thread_stats_manager.h"
#include "simulation_context.h"

#include <unordered_map>
#include <vector>

class StatsMetricBase;
class Config;
class CoreManager;
class StatsManager;

class RoutineTracerFunctionStats
{
 public:
   class RtnMaster;
   class RtnThread : public RoutineTracerThread
   {
    public:
      RtnThread(RtnMaster *master, Thread *thread);
      virtual ~RtnThread();

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
      class RtnMaster *m_master;
   };

   class RtnMaster : public RoutineTracer
   {
    public:
      RtnMaster(SimulationContext *context);
      virtual ~RtnMaster();

      virtual RoutineTracerThread *getThreadHandler(Thread *thread)
      {
         return new RtnThread(this, thread);
      }
      virtual void addRoutine(IntPtr eip, const char *name, const char *imgname, IntPtr offset, int column, int line,
                              const char *filename);
      virtual bool hasRoutine(IntPtr eip);

      virtual const Routine *getRoutineInfo(IntPtr eip)
      {
         return m_routines.count(eip) ? m_routines[eip] : NULL;
      }

      HooksManager* getHooksManager() { return m_hooks_manager; }
      MagicServer* getMagicServer() { return m_magic_server; }
      ThreadStatsManager* getThreadStatsManager() { return m_thread_stats_manager; }
      StatsManager* getStatsManager() { return m_stats_manager; }
      CoreManager* getCoreManager() { return m_core_manager; }
      Config* getConfig() { return m_config; }
      SimulationContext* getContext() { return m_context; }

    private:
      Lock m_lock;
      typedef std::unordered_map<IntPtr, RoutineTracer::Routine *> RoutineMap;
      RoutineMap m_routines;
      SimulationContext *m_context;
      HooksManager *m_hooks_manager;
      MagicServer *m_magic_server;
      ThreadStatsManager *m_thread_stats_manager;
      StatsManager *m_stats_manager;
      CoreManager *m_core_manager;
      Config *m_config;
   };

   class ThreadStatAggregates
   {
    public:
      static ThreadStatsManager::ThreadStatType registerStats(String name, String objectName, String metricName, SimulationContext *context);

    private:
      std::vector<std::vector<StatsMetricBase *>> m_stats;
      ThreadStatAggregates(String objectName, String metricName, StatsManager *stats_manager, Config *config);
      static UInt64 callback(ThreadStatsManager::ThreadStatType type, thread_id_t thread_id, Core *core, UInt64 user);
   };

   class ThreadStatCpiMem
   {
    public:
      static ThreadStatsManager::ThreadStatType registerStat(SimulationContext *context);

    private:
      std::vector<std::vector<StatsMetricBase *>> m_stats;
      ThreadStatCpiMem(ThreadStatsManager *tsm, CoreManager *core_manager, Config *config, StatsManager *stats_manager);
      static UInt64 callback(ThreadStatsManager::ThreadStatType type, thread_id_t thread_id, Core *core, UInt64 user);
   };
};

#endif // __ROUTINE_TRACER_FUNCSTATS_H
