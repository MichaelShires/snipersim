#include "magic_server.h"
#include "core_manager.h"
#include "dvfs_manager.h"
#include "fastforward_performance_model.h"
#include "hooks_manager.h"
#include "logmem.h"
#include "performance_model.h"
#include "sim_api.h"
#include "simulator.h"
#include "stats.h"
#include "thread.h"
#include "thread_manager.h"
#include "timer.h"

MagicServer::MagicServer(SimulationContext *context)
    : m_performance_enabled(false), m_context(context), m_config(context->getConfig()),
      m_hooks_manager(context->getHooksManager()), m_stats_manager(context->getStatsManager()),
      m_simulator(context->getSimulator()),
      m_core_manager(NULL), m_thread_manager(NULL), 
      m_clock_skew_minimization_server(NULL), m_dvfs_manager(NULL)
{
}

MagicServer::~MagicServer()
{
}

UInt64 MagicServer::Magic(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1)
{
   ScopedLock sl(m_lock);

   return Magic_unlocked(thread_id, core_id, cmd, arg0, arg1);
}

UInt64 MagicServer::Magic_unlocked(thread_id_t thread_id, core_id_t core_id, UInt64 cmd, UInt64 arg0, UInt64 arg1)
{
   switch (cmd) {
   case SIM_CMD_ROI_TOGGLE:
      if (m_config->getSimulationROI() == Config::ROI_MAGIC) {
         return setPerformance(!m_performance_enabled);
      }
      else {
         return 0;
      }
   case SIM_CMD_ROI_START:
      m_hooks_manager->callHooks(HookType::HOOK_APPLICATION_ROI_BEGIN, 0);
      if (m_config->getSimulationROI() == Config::ROI_MAGIC) {
         return setPerformance(true);
      }
      else {
         return 0;
      }
   case SIM_CMD_ROI_END:
      m_hooks_manager->callHooks(HookType::HOOK_APPLICATION_ROI_END, 0);
      if (m_config->getSimulationROI() == Config::ROI_MAGIC) {
         return setPerformance(false);
      }
      else {
         return 0;
      }
   case SIM_CMD_MHZ_SET:
      return setFrequency(arg0, arg1);
   case SIM_CMD_NAMED_MARKER: {
      char str[256];
      Core *core = m_core_manager->getCoreFromID(core_id);
      core->accessMemory(Core::NONE, Core::READ, arg1, str, 256, Core::MEM_MODELED_NONE);
      str[255] = '\0';

      MagicMarkerType args = {thread_id : thread_id, core_id : core_id, arg0 : arg0, arg1 : 0, str : str};
      m_hooks_manager->callHooks(HookType::HOOK_MAGIC_MARKER, (UInt64)&args);
      return 0;
   }
   case SIM_CMD_SET_THREAD_NAME: {
      char str[256];
      Core *core = m_core_manager->getCoreFromID(core_id);
      core->accessMemory(Core::NONE, Core::READ, arg0, str, 256, Core::MEM_MODELED_NONE);
      str[255] = '\0';

      m_stats_manager->logEvent(StatsManager::EVENT_THREAD_NAME, SubsecondTime::MaxTime(), core_id, thread_id,
                                         0, 0, str);
      m_thread_manager->getThreadFromID(thread_id)->setName(str);
      return 0;
   }
   case SIM_CMD_MARKER: {
      MagicMarkerType args = {thread_id : thread_id, core_id : core_id, arg0 : arg0, arg1 : arg1, str : NULL};
      m_hooks_manager->callHooks(HookType::HOOK_MAGIC_MARKER, (UInt64)&args);
      return 0;
   }
   case SIM_CMD_USER: {
      MagicMarkerType args = {thread_id : thread_id, core_id : core_id, arg0 : arg0, arg1 : arg1, str : NULL};
      return m_hooks_manager->callHooks(HookType::HOOK_MAGIC_USER, (UInt64)&args,
                                                 true /* expect return value */);
   }
   case SIM_CMD_INSTRUMENT_MODE:
      return setInstrumentationMode(arg0);
   case SIM_CMD_MHZ_GET:
      return getFrequency(arg0);
   case SIM_CMD_GET_SIM_TIME: {
      StatsMetricBase *tmp = m_stats_manager->getMetricObject("barrier", 0, "global_time");
      if (tmp == NULL) {
         return 0;
      }
      else {
         UInt64 ret = tmp->recordMetric();
         return ret;
      }
   }
   case SIM_CMD_GET_INS_NUM: {
      StatsMetricBase *tmp = m_stats_manager->getMetricObject("core", arg0, "instructions");
      if (tmp == NULL) {
         return 0;
      }
      else {

         UInt64 ret = tmp->recordMetric();
         return ret;
      }
   }
   case SIM_CMD_GET_L2_COUNT: {
      UInt32 total_cores = m_config->getApplicationCores();
      UInt64 total_load_count = 0;
      UInt64 total_store_count = 0;

      for (uint32_t core_i = 0; core_i < total_cores; core_i++) {
         StatsMetricBase *tmp = m_stats_manager->getMetricObject("L2", core_i, "stores");
         UInt64 store_count;
         if (tmp == NULL) {
            store_count = 0;
         }
         else {
            UInt64 ret = tmp->recordMetric();
            store_count = ret;
         }
         tmp = m_stats_manager->getMetricObject("L2", core_i, "loads");
         UInt64 load_count;
         if (tmp == NULL) {
            load_count = 0;
         }
         else {
            UInt64 ret = tmp->recordMetric();
            load_count = ret;
         }
         total_load_count += load_count;
         total_store_count += store_count;
      }
      return total_load_count + total_store_count;
   }
   case SIM_CMD_GET_L2_MISS_COUNT: {
      UInt32 total_cores = m_config->getApplicationCores();
      UInt64 total_load_count = 0;
      UInt64 total_store_count = 0;
      for (uint32_t core_i = 0; core_i < total_cores; core_i++) {
         StatsMetricBase *tmp = m_stats_manager->getMetricObject("L2", core_i, "store-misses");
         UInt64 store_count;
         if (tmp == NULL) {
            store_count = 0;
         }
         else {
            UInt64 ret = tmp->recordMetric();
            store_count = ret;
         }
         tmp = m_stats_manager->getMetricObject("L2", core_i, "load-misses");
         UInt64 load_count;
         if (tmp == NULL) {
            load_count = 0;
         }
         else {
            UInt64 ret = tmp->recordMetric();
            load_count = ret;
         }
         total_load_count += load_count;
         total_store_count += store_count;
      }
      return total_load_count + total_store_count;
   }
   case SIM_CMD_GET_BARRIER_REACHED: {
      bool reached = m_clock_skew_minimization_server->onlyMainCoreRunning();
      UInt64 ret = (UInt64)reached;
      m_clock_skew_minimization_server->printState();
      return ret;
   }

   default:
      LOG_ASSERT_ERROR(false, "Got invalid Magic %lu, arg0(%lu) arg1(%lu)", cmd, arg0, arg1);
   }
   return 0;
}

UInt64 MagicServer::getGlobalInstructionCount(CoreManager *core_manager, Config *config)
{
   UInt64 ninstrs = 0;
   for (UInt32 i = 0; i < config->getApplicationCores(); i++)
      ninstrs += core_manager->getCoreFromID(i)->getInstructionCount();
   return ninstrs;
}

UInt64 MagicServer::getGlobalInstructionCount()
{
   return getGlobalInstructionCount(Sim()->getCoreManager(), Sim()->getConfig());
}

static Timer t_start;
UInt64 ninstrs_start;
__attribute__((weak)) void PinDetach(void)
{
}

void MagicServer::enablePerformance()
{
   m_stats_manager->recordStats("roi-begin");
   ninstrs_start = getGlobalInstructionCount(m_core_manager, m_config);
   t_start.start();

   Simulator::enablePerformanceModels();
   m_simulator->setInstrumentationMode(InstMode::inst_mode_roi, true /* update_barrier */);
}

void MagicServer::disablePerformance()
{
   Simulator::disablePerformanceModels();
   m_stats_manager->recordStats("roi-end");

   float seconds = t_start.getTime() / 1e9;
   UInt64 ninstrs = getGlobalInstructionCount(m_core_manager, m_config) - ninstrs_start;
   double simtime = m_clock_skew_minimization_server->getGlobalTime().getNS() * 1e-9;
   UInt64 cycles =
       SubsecondTime::divideRounded(m_clock_skew_minimization_server->getGlobalTime(),
                                    m_core_manager->getCoreFromID(0)->getDvfsDomain()->getPeriod());
   printf("[SNIPER] Simulated %.1fM instructions, %.1fM cycles, %.2f IPC\n", ninstrs / 1e6, cycles / 1e6,
          float(ninstrs) / (cycles ? cycles : 1));
   printf("[SNIPER] Simulation Time %.3f\n", simtime);
   printf("[SNIPER] Simulation speed %.1f KIPS (%.1f KIPS / target core - %.1fns/instr)\n", ninstrs / seconds / 1e3,
          ninstrs / seconds / 1e3 / m_config->getApplicationCores(),
          seconds * 1e9 / (float(ninstrs ? ninstrs : 1.) / m_config->getApplicationCores()));

   PerformanceModel *perf = m_core_manager->getCoreFromID(0)->getPerformanceModel();
   if (perf->getFastforwardPerformanceModel()->getFastforwardedTime() > SubsecondTime::Zero()) {
      // NOTE: Prints out the non-idle ratio for core 0 only, but it's just indicative anyway
      double ff_ratio = double(perf->getFastforwardPerformanceModel()->getFastforwardedTime().getNS()) /
                        double(perf->getNonIdleElapsedTime().getNS());
      double percent_detailed = 100. * (1. - ff_ratio);
      printf("[SNIPER] Sampling: executed %.2f%% of simulated time in detailed mode\n", percent_detailed);
   }

   fflush(NULL);

   m_simulator->setInstrumentationMode(InstMode::inst_mode_end, true /* update_barrier */);
   PinDetach();
}

void print_allocations();

UInt64 MagicServer::setPerformance(bool enabled)
{
   if (m_performance_enabled == enabled)
      return 1;

   m_performance_enabled = enabled;

   // static bool enabled = false;
   static Timer t_start;
   // ScopedLock sl(l_alloc);

   if (m_performance_enabled) {
      printf("[SNIPER] Enabling performance models\n");
      fflush(NULL);
      t_start.start();
      logmem_enable(true);
      m_hooks_manager->callHooks(HookType::HOOK_ROI_BEGIN, 0);
   }
   else {
      m_hooks_manager->callHooks(HookType::HOOK_ROI_END, 0);
      printf("[SNIPER] Disabling performance models\n");
      float seconds = t_start.getTime() / 1e9;
      printf("[SNIPER] Leaving ROI after %.2f seconds\n", seconds);
      fflush(NULL);
      logmem_enable(false);
      logmem_write_allocations();
   }

   if (enabled)
      enablePerformance();
   else
      disablePerformance();

   return 0;
}

UInt64 MagicServer::setFrequency(UInt64 core_number, UInt64 freq_in_mhz)
{
   UInt32 num_cores = m_config->getApplicationCores();
   UInt64 freq_in_hz;
   if (core_number >= num_cores)
      return 1;
   freq_in_hz = 1000000 * freq_in_mhz;

   printf("[SNIPER] Setting frequency for core %" PRId64 " in DVFS domain %d to %" PRId64 " MHz\n", core_number,
          m_dvfs_manager->getCoreDomainId(core_number), freq_in_mhz);

   if (freq_in_hz > 0)
      m_dvfs_manager->setCoreDomain(core_number, ComponentPeriod::fromFreqHz(freq_in_hz));
   else {
      m_thread_manager->stallThread_async(core_number, STALL_BROKEN, SubsecondTime::MaxTime());
      m_core_manager->getCoreFromID(core_number)->setState(Core::BROKEN);
   }

   // First set frequency, then call hooks so hook script can find the new frequency by querying the DVFS manager
   m_hooks_manager->callHooks(HookType::HOOK_CPUFREQ_CHANGE, core_number);

   return 0;
}

UInt64 MagicServer::getFrequency(UInt64 core_number)
{
   UInt32 num_cores = m_config->getApplicationCores();
   if (core_number >= num_cores)
      return UINT64_MAX;

   const ComponentPeriod *per = m_dvfs_manager->getCoreDomain(core_number);
   return per->getPeriodInFreqMHz();
}

UInt64 MagicServer::setInstrumentationMode(UInt64 sim_api_opt)
{
   InstMode::inst_mode_t inst_mode;
   switch (sim_api_opt) {
   case SIM_OPT_INSTRUMENT_DETAILED:
      inst_mode = InstMode::DETAILED;
      break;
   case SIM_OPT_INSTRUMENT_WARMUP:
      inst_mode = InstMode::CACHE_ONLY;
      break;
   case SIM_OPT_INSTRUMENT_FASTFORWARD:
      inst_mode = InstMode::FAST_FORWARD;
      break;
   default:
      LOG_PRINT_ERROR("Unexpected magic instrument opt type: %lx.", sim_api_opt);
   }
   m_simulator->setInstrumentationMode(inst_mode, true /* update_barrier */);

   return 0;
}
