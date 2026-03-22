#include "sampling_manager.h"
#include "config.hpp"
#include "core.h"
#include "core_manager.h"
#include "fastforward_performance_model.h"
#include "hooks_manager.h"
#include "magic_client.h"
#include "magic_server.h"
#include "performance_model.h"
#include "sampling_provider.h"
#include "simulator.h"
#include "thread_manager.h"

SamplingManager::SamplingManager(SimulationContext *context)
    : m_sampling_enabled(context->getConfigFile()->getBool("sampling/enabled")), m_fastforward(false), m_warmup(false),
      m_target_ffend(SubsecondTime::Zero()), m_sampling_provider(NULL), m_sampling_algorithm(NULL),
      m_instructions(context->getConfig()->getApplicationCores(), 0),
      m_time_total(context->getConfig()->getApplicationCores(), SubsecondTime::Zero()),
      m_time_nonidle(context->getConfig()->getApplicationCores(), SubsecondTime::Zero()),
      m_context(context), m_config(context->getConfig()), m_cfg(context->getConfigFile()),
      m_hooks_manager(context->getHooksManager()), m_core_manager(context->getCoreManager()),
      m_thread_manager(context->getThreadManager()), m_magic_server(context->getMagicServer()),
      m_clock_skew_minimization_server(context->getClockSkewMinimizationServer()),
      m_simulator(context->getSimulator())
{
   if (!m_sampling_enabled)
      return;

   m_uncoordinated = m_cfg->getBool("sampling/uncoordinated");

   LOG_ASSERT_ERROR(m_config->getSimulationMode() == Config::PINTOOL,
                    "Sampling is only supported in Pin mode");

   m_hooks_manager->registerHook(
       HookType::HOOK_INSTR_COUNT, (HooksManager::HookCallbackFunc)SamplingManager::hook_instr_count, (UInt64)this);
   m_hooks_manager->registerHook(HookType::HOOK_PERIODIC,
                                          (HooksManager::HookCallbackFunc)SamplingManager::hook_periodic, (UInt64)this);

   m_sampling_provider = SamplingProvider::create();
   m_sampling_algorithm = SamplingAlgorithm::create(this);
}

SamplingManager::~SamplingManager(void)
{
   if (m_sampling_provider)
      delete m_sampling_provider;
   if (m_sampling_algorithm)
      delete m_sampling_algorithm;
}

void SamplingManager::periodic(SubsecondTime time)
{
   if (m_fastforward)
      m_sampling_algorithm->callbackFastForward(time, m_warmup);
   else
      m_sampling_algorithm->callbackDetailed(time);

#if 0
   if (m_fastforward) {
      // Debug: print out a skew report
      printf("[SKEW] %lu", Timer::now());
      UInt64 t = m_core_manager->getCoreFromID(0)->getPerformanceModel()->getElapsedTime().getNS();
      for(unsigned int c = 0; c < m_config->getApplicationCores(); ++c)
         printf(" %ld", m_core_manager->getCoreFromID(c)->getPerformanceModel()->getElapsedTime().getNS() - t);
      printf("\n");
   }
#endif
}

void SamplingManager::setInstrumentationMode(InstMode::inst_mode_t mode)
{
   // We're counting on the barrier for callbacks (time is still running),
   // so tell Simulator not to disable it even when going out of DETAILED.
   m_simulator->setInstrumentationMode(mode, false /* update_barrier */);
}

void SamplingManager::disableFastForward()
{
   m_fastforward = false;
   m_warmup = false;
   SubsecondTime barrier_next = SubsecondTime::Zero();
   SubsecondTime core_last = SubsecondTime::MaxTime();
   for (unsigned int core_id = 0; core_id < m_config->getApplicationCores(); ++core_id) {
      Core *core = m_core_manager->getCoreFromID(core_id);
      core->getPerformanceModel()->setFastForward(false, true);
      core->disableInstructionsCallback();
      if (m_thread_manager->isThreadRunning(core_id)) {
         barrier_next = std::max(barrier_next, core->getPerformanceModel()->getElapsedTime());
         core_last = std::min(core_last, core->getPerformanceModel()->getElapsedTime());
      }
   }
   if (m_uncoordinated) {
      for (UInt32 core_id = 0; core_id < m_config->getApplicationCores(); core_id++) {
         // In uncoordinated mode, some cores will be behind because they didn't reach their target instruction count.
         // Reset all their times to the same value so they won't have to catch up in detailed mode.
         PerformanceModel *perf = m_core_manager->getCoreFromID(core_id)->getPerformanceModel();
         perf->getFastforwardPerformanceModel()->incrementElapsedTime(barrier_next - perf->getElapsedTime());
      }
   }
   // Tell the barrier to go to detailed mode again, but make sure it skips over the fastforwarded section of time.
   // FIXME:
   // We restart the barrier at the maximum time, and have all threads make uncoordinated progress
   // towards the fastest thread. There won't be any HOOK_PERIODIC calls (which may not make much sense anyway,
   // as during this period at least one thread is way ahead and won't be running).
   // Alternatively, we could reset the barrier to the min time of all cores, this way, all cores that are behind
   // the fastest one, will start to make detailed progress again in lockstep. We'll also get HOOK_PERIODIC calls
   // during this time.
   if (m_clock_skew_minimization_server)
      m_clock_skew_minimization_server->setFastForward(false, barrier_next);
   this->setInstrumentationMode(InstMode::DETAILED);
}

void SamplingManager::enableFastForward(SubsecondTime until, bool warmup, bool detailed_sync)
{
   m_fastforward = true;
   m_warmup = warmup;
   // Approximate time we want to leave fastforward mode
   m_target_ffend = until;

   SubsecondTime barrier_next = SubsecondTime::Zero();
   for (unsigned int core_id = 0; core_id < m_config->getApplicationCores(); ++core_id) {
      PerformanceModel *perf = m_core_manager->getCoreFromID(core_id)->getPerformanceModel();
      perf->setFastForward(true, detailed_sync);
      barrier_next = std::max(barrier_next, perf->getElapsedTime());
      recalibrateInstructionsCallback(core_id);
   }
   // Set barrier to fastforward, and update next_barrier_time to the maximum of all core times so we definitely release
   // everyone
   if (m_clock_skew_minimization_server)
      m_clock_skew_minimization_server->setFastForward(true, barrier_next);

   if (m_warmup)
      this->setInstrumentationMode(InstMode::CACHE_ONLY);
   else
      this->setInstrumentationMode(InstMode::FAST_FORWARD);

   if (m_sampling_provider) {
      m_sampling_provider->startSampling(until);
   }
}

SubsecondTime SamplingManager::getCoreHistoricCPI(Core *core, bool non_idle, SubsecondTime min_nonidle_time) const
{
   UInt64 d_instrs = 0;
   SubsecondTime d_time = SubsecondTime::Zero();

   d_instrs = core->getPerformanceModel()->getInstructionCount() - m_instructions[core->getId()];
   if (non_idle)
      d_time = core->getPerformanceModel()->getNonIdleElapsedTime() - m_time_nonidle[core->getId()];
   else
      d_time = core->getPerformanceModel()->getElapsedTime() - m_time_total[core->getId()];

   if (d_instrs == 0) {
      return SubsecondTime::MaxTime();
   }
   else if (d_time <= min_nonidle_time) {
      return SubsecondTime::Zero();
   }
   else {
      return d_time / d_instrs;
   }
}

void SamplingManager::resetCoreHistoricCPIs()
{
   for (UInt32 core_id = 0; core_id < m_config->getApplicationCores(); core_id++) {
      Core *core = m_core_manager->getCoreFromID(core_id);
      m_instructions[core->getId()] = core->getPerformanceModel()->getInstructionCount();
      m_time_total[core->getId()] = core->getPerformanceModel()->getElapsedTime();
      m_time_nonidle[core->getId()] = core->getPerformanceModel()->getNonIdleElapsedTime();
   }
}

void SamplingManager::recalibrateInstructionsCallback(core_id_t core_id)
{
   Core *core = m_core_manager->getCoreFromID(core_id);
   SubsecondTime now = core->getPerformanceModel()->getElapsedTime();
   if (now > m_target_ffend) {
      // Just a single instruction so we call into the barrier immediately
      core->setInstructionsCallback(1);
   }
   else {
      SubsecondTime cpi = core->getPerformanceModel()->getFastforwardPerformanceModel()->getCurrentCPI();
      // If CPI hasn't been set up, fall back to 1 IPC to avoid division by zero
      if (cpi == SubsecondTime::Zero())
         cpi = m_core_manager->getCoreFromID(core_id)->getDvfsDomain()->getPeriod();
      UInt64 ninstrs = SubsecondTime::divideRounded(m_target_ffend - now, cpi);
      core->setInstructionsCallback(ninstrs);
   }
}

void SamplingManager::instr_count(core_id_t core_id)
{
   if (m_fastforward && m_magic_server->inROI()) {
      if (m_uncoordinated) {
         for (UInt32 core_id = 0; core_id < m_config->getApplicationCores(); core_id++) {
            // In uncoordinated mode, the first processor to reach his target instruction count
            // ends fast-forward mode for everyone
            m_core_manager->getCoreFromID(core_id)->setInstructionsCallback(1);
         }
      }
      m_core_manager->getCoreFromID(core_id)->getClockSkewMinimizationClient()->synchronize(
          SubsecondTime::Zero(), true);
   }
}
