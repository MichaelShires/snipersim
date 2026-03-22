#include "routine_tracer_funcstats.h"
#include "cache_efficiency_tracker.h"
#include "core.h"
#include "core_manager.h"
#include "log.h"
#include "performance_model.h"
#include "simulator.h"
#include "stats.h"
#include "thread.h"
#include "simulation_context.h"

#include <cstring>

RoutineTracerFunctionStats::RtnThread::RtnThread(RoutineTracerFunctionStats::RtnMaster *master, Thread *thread)
    : RoutineTracerThread(thread, master->getHooksManager(), master->getMagicServer()), m_master(master)
{
}

RoutineTracerFunctionStats::RtnThread::~RtnThread()
{
}

void RoutineTracerFunctionStats::RtnThread::functionEnter(IntPtr eip, IntPtr callEip)
{
   m_master->getThreadStatsManager()->update(m_thread->getId());
}

void RoutineTracerFunctionStats::RtnThread::functionExit(IntPtr eip)
{
   m_master->getThreadStatsManager()->update(m_thread->getId());
}

RoutineTracerFunctionStats::RtnMaster::RtnMaster(SimulationContext *context)
    : m_context(context), m_hooks_manager(context->getHooksManager()), m_magic_server(context->getMagicServer()),
      m_thread_stats_manager(context->getThreadStatsManager()), m_stats_manager(context->getStatsManager()),
      m_core_manager(context->getCoreManager()), m_config(context->getConfig())
{
   ThreadStatAggregates::registerStats("instructions", "core", "instructions", m_context);
   ThreadStatAggregates::registerStats("latency", "performance_model", "elapsed_time", m_context);
   ThreadStatCpiMem::registerStat(m_context);
}

RoutineTracerFunctionStats::RtnMaster::~RtnMaster()
{
}

void RoutineTracerFunctionStats::RtnMaster::addRoutine(IntPtr eip, const char *name, const char *imgname,
                                                      IntPtr offset, int column, int line, const char *filename)
{
   ScopedLock sl(m_lock);

   if (m_routines.count(eip) == 0) {
      m_routines[eip] = new RoutineTracer::Routine(eip, name, imgname, offset, column, line, filename);
   }
}

bool RoutineTracerFunctionStats::RtnMaster::hasRoutine(IntPtr eip)
{
   ScopedLock sl(m_lock);

   return m_routines.count(eip) > 0;
}

ThreadStatsManager::ThreadStatType RoutineTracerFunctionStats::ThreadStatAggregates::registerStats(String name,
                                                                                                   String objectName,
                                                                                                   String metricName,
                                                                                                   SimulationContext *context)
{
   StatsManager *stats_manager = context->getStatsManager();
   ThreadStatsManager *tsm = context->getThreadStatsManager();
   Config *config = context->getConfig();

   if (stats_manager->getMetricObject(objectName, 0, metricName)) {
      ThreadStatAggregates *tsa = new ThreadStatAggregates(objectName, metricName, stats_manager, config);
      return tsm->registerThreadStatMetric(ThreadStatsManager::DYNAMIC, name.c_str(), callback, (UInt64)tsa);
   }
   else {
      return ThreadStatsManager::INVALID;
   }
}

RoutineTracerFunctionStats::ThreadStatAggregates::ThreadStatAggregates(String objectName, String metricName, StatsManager *stats_manager, Config *config)
{
   for (core_id_t core_id = 0; core_id < (core_id_t)config->getApplicationCores(); ++core_id) {
      StatsMetricBase *m = stats_manager->getMetricObject(objectName, core_id, metricName);
      std::vector<StatsMetricBase *> stats;
      stats.push_back(m);
      m_stats.push_back(stats);
   }
}

UInt64 RoutineTracerFunctionStats::ThreadStatAggregates::callback(ThreadStatsManager::ThreadStatType type,
                                                                  thread_id_t thread_id, Core *core, UInt64 user)
{
   ThreadStatAggregates *tsa = (ThreadStatAggregates *)user;
   std::vector<StatsMetricBase *> &stats = tsa->m_stats[core->getId()];

   UInt64 result = 0;
   for (std::vector<StatsMetricBase *>::iterator it = stats.begin(); it != stats.end(); ++it)
      result += (*it)->recordMetric();
   return result;
}

ThreadStatsManager::ThreadStatType RoutineTracerFunctionStats::ThreadStatCpiMem::registerStat(SimulationContext *context)
{
   ThreadStatsManager *tsm = context->getThreadStatsManager();
   CoreManager *core_manager = context->getCoreManager();
   Config *config = context->getConfig();
   StatsManager *stats_manager = context->getStatsManager();

   ThreadStatCpiMem *tsns = new ThreadStatCpiMem(tsm, core_manager, config, stats_manager);
   return tsm->registerThreadStatMetric(ThreadStatsManager::DYNAMIC, "cpi-mem", callback, (UInt64)tsns);
}

RoutineTracerFunctionStats::ThreadStatCpiMem::ThreadStatCpiMem(ThreadStatsManager *tsm, CoreManager *core_manager, Config *config, StatsManager *stats_manager)
{
   for (core_id_t core_id = 0; core_id < (core_id_t)config->getApplicationCores(); ++core_id) {
      std::vector<StatsMetricBase *> stats;
      stats.push_back(stats_manager->getMetricObject("performance_model", core_id, "dtlb-access-latency"));
      stats.push_back(stats_manager->getMetricObject("performance_model", core_id, "dcache-access-latency"));
      m_stats.push_back(stats);
   }
}

UInt64 RoutineTracerFunctionStats::ThreadStatCpiMem::callback(ThreadStatsManager::ThreadStatType type,
                                                              thread_id_t thread_id, Core *core, UInt64 user)
{
   ThreadStatCpiMem *tsns = (ThreadStatCpiMem *)user;
   std::vector<StatsMetricBase *> &stats = tsns->m_stats[core->getId()];

   UInt64 result = 0;
   for (std::vector<StatsMetricBase *>::iterator it = stats.begin(); it != stats.end(); ++it)
      result += (*it)->recordMetric();
   return result;
}
