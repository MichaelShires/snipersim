#ifndef __FASTFORWARD_PERFORMANCE_MANAGER_H
#define __FASTFORWARD_PERFORMANCE_MANAGER_H

#include "fixed_types.h"
#include "subsecond_time.h"
#include "simulation_context.h"

class Config;
namespace config { class Config; }
class HooksManager;
class CoreManager;
class ThreadManager;
class ClockSkewMinimizationServer;
class Simulator;

class FastForwardPerformanceManager
{
 public:
   static FastForwardPerformanceManager *create(SimulationContext *context);

   FastForwardPerformanceManager(SimulationContext *context);
   void enable();
   void disable();

 protected:
   friend class FastforwardPerformanceModel;

   void recalibrateInstructionsCallback(core_id_t core_id);

 private:
   const SubsecondTime m_sync_interval;
   bool m_enabled;
   SubsecondTime m_target_sync_time;

   SimulationContext *m_context;
   Config *m_config;
   config::Config *m_cfg;
   HooksManager *m_hooks_manager;
   CoreManager *m_core_manager;
   ThreadManager *m_thread_manager;
   ClockSkewMinimizationServer *m_clock_skew_minimization_server;
   Simulator *m_simulator;

   static SInt64 hook_instr_count(UInt64 self, UInt64 core_id)
   {
      ((FastForwardPerformanceManager *)self)->instr_count(core_id);
      return 0;
   }
   static SInt64 hook_periodic(UInt64 self, UInt64 time)
   {
      ((FastForwardPerformanceManager *)self)->periodic(*(subsecond_time_t *)&time);
      return 0;
   }

   void instr_count(core_id_t core_id);
   void periodic(SubsecondTime time);
   void step();
};

#endif // __FASTFORWARD_PERFORMANCE_MANAGER_H
