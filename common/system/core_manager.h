#ifndef CORE_MANAGER_H
#define CORE_MANAGER_H

#include "config.h"
#include "core.h"
#include "fixed_types.h"
#include "lock.h"
#include "log.h"
#include "tls.h"
#include "simulation_context.h"

#include <fstream>
#include <iostream>
#include <map>
#include <vector>

class Core;
class Config;
namespace config { class Config; }
class DvfsManager;
class StatsManager;
class FaultinjectionManager;
class HooksManager;
class ThreadManager;
class TraceManager;
class Simulator;

class CoreManager
{
 public:
   CoreManager(SimulationContext *context);
   ~CoreManager();

   void setThreadManager(ThreadManager *thread_manager) { m_thread_manager = thread_manager; }
   void setTraceManager(TraceManager *trace_manager) { m_trace_manager = trace_manager; }

   Config* getConfig() { return m_config; }
   config::Config* getCfg() { return m_cfg; }
   DvfsManager* getDvfsManager() { return m_dvfs_manager; }
   StatsManager* getStatsManager() { return m_stats_manager; }
   FaultinjectionManager* getFaultinjectionManager() { return m_faultinjection_manager; }
   HooksManager* getHooksManager() { return m_hooks_manager; }
   ThreadManager* getThreadManager() { return m_thread_manager; }
   TraceManager* getTraceManager() { return m_trace_manager; }
   Simulator* getSimulator() { return m_simulator; }
   SimulationContext* getContext() { return m_context; }

   enum ThreadType
   {
      INVALID,
      APP_THREAD,  // Application (Pin) thread
      CORE_THREAD, // Core (Performance model) thread
      SIM_THREAD,  // System thread
   };

   core_id_t registerThread(Thread *thread);
   void unregisterThread(Thread *thread);

   void initializeThread(core_id_t core_id);
   void terminateThread();

   core_id_t registerSimThread(ThreadType type);
   void unregisterSimThread();

   void registerCoreThread();
   void unregisterCoreThread();

   Core *getCoreFromID(core_id_t id);
   Core *getCurrentCore();
   core_id_t getCurrentCoreID();

   UInt32 getNumThreads() { return m_config->getTotalCores(); }

   bool amiSimThread();
   bool amiCoreThread();
   bool amiUserThread();

   void initializeCommId(UInt32 comm_id);

 private:
   SimulationContext *m_context;
   TLS *m_core_tls;
   TLS *m_thread_type_tls;

   UInt32 m_num_registered_sim_threads;
   UInt32 m_num_registered_core_threads;
   Lock m_num_registered_threads_lock;

   Config *m_config;
   DvfsManager *m_dvfs_manager;
   StatsManager *m_stats_manager;
   FaultinjectionManager *m_faultinjection_manager;
   HooksManager *m_hooks_manager;
   config::Config *m_cfg;
   ThreadManager *m_thread_manager;
   TraceManager *m_trace_manager;
   Simulator *m_simulator;

   std::vector<Core *> m_cores;
};

#endif
