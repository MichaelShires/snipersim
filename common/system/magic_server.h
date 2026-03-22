#ifndef MAGIC_SERVER_H
#define MAGIC_SERVER_H

#include "fixed_types.h"
#include "progress.h"
#include "simulation_context.h"

class Config;
class HooksManager;
class CoreManager;
class ThreadManager;
class StatsManager;
class ClockSkewMinimizationServer;
class DvfsManager;
class Simulator;

class MagicServer
{
 public:
   // data type to hold arguments in a HOOK_MAGIC_MARKER callback
   struct MagicMarkerType
   {
      thread_id_t thread_id;
      core_id_t core_id;
      UInt64 arg0, arg1;
      const char *str;
   };

   MagicServer(SimulationContext *context);
   ~MagicServer();

   UInt64 Magic(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1);
   bool inROI(void) const
   {
      return m_performance_enabled;
   }
   static UInt64 getGlobalInstructionCount(CoreManager *core_manager, Config *config);
   static UInt64 getGlobalInstructionCount();

   // To be called while holding the thread manager lock
   UInt64 Magic_unlocked(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1);
   UInt64 setFrequency(UInt64 core_number, UInt64 freq_in_mhz);
   UInt64 getFrequency(UInt64 core_number);

   void enablePerformance();
   void disablePerformance();
   UInt64 setPerformance(bool enabled);

   UInt64 setInstrumentationMode(UInt64 sim_api_opt);

   void setCoreManager(CoreManager *core_manager) { m_core_manager = core_manager; }
   void setThreadManager(ThreadManager *thread_manager) { m_thread_manager = thread_manager; }
   void setClockSkewMinimizationServer(ClockSkewMinimizationServer *clock_skew_minimization_server) { m_clock_skew_minimization_server = clock_skew_minimization_server; }
   void setDvfsManager(DvfsManager *dvfs_manager) { m_dvfs_manager = dvfs_manager; }

   void setProgress(float progress)
   {
      m_progress.setProgress(progress);
   }

 private:
   Lock m_lock;
   bool m_performance_enabled;
   Progress m_progress;

   SimulationContext *m_context;
   Config *m_config;
   HooksManager *m_hooks_manager;
   StatsManager *m_stats_manager;
   Simulator *m_simulator;
   CoreManager *m_core_manager;
   ThreadManager *m_thread_manager;
   ClockSkewMinimizationServer *m_clock_skew_minimization_server;
   DvfsManager *m_dvfs_manager;
};

#endif // MAGIC_SERVER_H
