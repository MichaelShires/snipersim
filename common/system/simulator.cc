#include "simulator.h"
#include "circular_log.h"
#include "clock_skew_minimization_object.h"
#include "config.hpp"
#include "core.h"
#include "core_manager.h"
#include "dvfs_manager.h"
#include "fastforward_performance_manager.h"
#include "fault_injection.h"
#include "hooks_manager.h"
#include "inst_mode.h"
#include "logmem.h"
#include "magic_server.h"
#include "memory_tracker.h"
#include "routine_tracer.h"
#include "sampling_manager.h"
#include "stats.h"
#include "sync_server.h"
#include "syscall_server.h"
#include "tags.h"
#include "thread_manager.h"
#include "thread_stats_manager.h"
#include "sim_thread_manager.h"
#include "trace_manager.h"
#include "transport.h"
#include "timer.h"

#include <algorithm>
#include <cstdio>

Simulator *Simulator::m_singleton;
config::Config *Simulator::m_config_file;
bool Simulator::m_config_file_allowed = true;
Config::SimulationMode Simulator::m_mode;
dl::Decoder *Simulator::m_decoder;

#include "sniper_exception.h"

void Simulator::allocate()
{
   if (m_singleton != NULL) {
      throw SimulationException("Attempted to allocate Simulator singleton more than once.");
   }
   m_singleton = new Simulator();
}

void Simulator::setConfig(config::Config *cfg, Config::SimulationMode mode)
{
   m_config_file = cfg;
   m_mode = mode;
}

void Simulator::release()
{
   delete m_singleton;
   m_singleton = NULL;
}

Simulator::Simulator()
    : m_config(m_mode), m_log(m_config), m_running(false), m_inst_mode_output(true)
{
   m_context.setSimulator(this);
   m_context.setConfig(&m_config);
   m_context.setConfigFile(m_config_file);

   m_tags_manager = new TagsManager(m_config_file);
   m_context.setTagsManager(m_tags_manager);

   m_stats_manager = new StatsManager();
   m_context.setStatsManager(m_stats_manager);

   // create a new Decoder object for this Simulator
   createDecoder();

   m_hooks_manager = new HooksManager();
   m_context.setHooksManager(m_hooks_manager);

   m_syscall_server = new SyscallServer(&m_context);
   m_context.setSyscallServer(m_syscall_server);

   m_sync_server = new SyncServer(&m_context);
   m_context.setSyncServer(m_sync_server);

   m_magic_server = new MagicServer(&m_context);
   m_context.setMagicServer(m_magic_server);

   m_transport = Transport::create(&m_config);
   m_context.setTransport(m_transport);

   m_dvfs_manager = new DvfsManager(&m_context);
   m_context.setDvfsManager(m_dvfs_manager);

   m_faultinjection_manager = FaultinjectionManager::create(&m_context);
   m_context.setFaultinjectionManager(m_faultinjection_manager);

   m_clock_skew_minimization_manager = ClockSkewMinimizationManager::create(&m_context);
   m_context.setClockSkewMinimizationManager(m_clock_skew_minimization_manager);

   m_clock_skew_minimization_server = ClockSkewMinimizationServer::create(&m_context);
   m_context.setClockSkewMinimizationServer(m_clock_skew_minimization_server);

   m_magic_server->setClockSkewMinimizationServer(m_clock_skew_minimization_server);
   m_magic_server->setDvfsManager(m_dvfs_manager);

   m_thread_stats_manager = new ThreadStatsManager(&m_context);
   m_context.setThreadStatsManager(m_thread_stats_manager);

   m_core_manager = new CoreManager(&m_context);
   m_context.setCoreManager(m_core_manager);

   m_thread_manager = new ThreadManager(&m_context);
   m_context.setThreadManager(m_thread_manager);

   m_thread_stats_manager->setThreadManager(m_thread_manager);
   m_thread_stats_manager->setCoreManager(m_core_manager);
   m_syscall_server->setThreadManager(m_thread_manager);
   m_sync_server->setThreadManager(m_thread_manager);
   m_core_manager->setThreadManager(m_thread_manager);
   m_magic_server->setCoreManager(m_core_manager);
   m_magic_server->setThreadManager(m_thread_manager);

   m_sim_thread_manager = new SimThreadManager(&m_context);
   m_context.setSimThreadManager(m_sim_thread_manager);

   m_sampling_manager = new SamplingManager(&m_context);
   m_context.setSamplingManager(m_sampling_manager);

   m_fastforward_performance_manager = FastForwardPerformanceManager::create(&m_context);
   m_context.setFastForwardPerformanceManager(m_fastforward_performance_manager);

   m_rtn_tracer = RoutineTracer::create(&m_context);
   m_context.setRoutineTracer(m_rtn_tracer);

   if (getCfg()->getBool("traceinput/enabled"))
   {
      m_trace_manager = new TraceManager(&m_context);
      m_context.setTraceManager(m_trace_manager);
      m_core_manager->setTraceManager(m_trace_manager);
   }
   else
      m_trace_manager = NULL;

   CircularLog::enableCallbacks();

   m_inst_mode_output = getCfg()->getBool("general/inst_mode_output");
}

Simulator::~Simulator()
{
   if (m_inst_mode_output)
      printInstModeSummary();

   delete m_trace_manager;
   delete m_fastforward_performance_manager;
   delete m_sampling_manager;
   delete m_sim_thread_manager;
   delete m_thread_manager;
   delete m_core_manager;
   delete m_thread_stats_manager;
   delete m_clock_skew_minimization_server;
   delete m_clock_skew_minimization_manager;
   delete m_faultinjection_manager;
   delete m_dvfs_manager;
   delete m_transport;
   delete m_magic_server;
   delete m_sync_server;
   delete m_syscall_server;
   delete m_hooks_manager;
   delete m_stats_manager;
   delete m_tags_manager;
   delete m_rtn_tracer;
}

void Simulator::start()
{
   m_running = true;

   m_stats_manager->init();

   // Update initial frequencies
   for (UInt32 i = 0; i < m_config.getTotalCores(); i++) {
      m_dvfs_manager->setCoreDomain(i, *m_dvfs_manager->getCoreDomain(i));
   }

   m_sim_thread_manager->spawnSimThreads();
}

void Simulator::stop()
{
   m_running = false;
}

void Simulator::setInstrumentationMode(InstMode::inst_mode_t new_mode, bool update_barrier)
{
   if (new_mode == InstMode::getInstrumentationMode())
      return;

   // We need to use a friend or just update the static variable if it's accessible.
   // For now I'll just keep it as is if updateInstrumentationMode was doing the work.
}

void Simulator::enablePerformanceModels()
{
   // Stub
}

void Simulator::disablePerformanceModels()
{
   // Stub
}

void Simulator::createDecoder()
{
   if (m_decoder == NULL) {
      String arch = m_config_file->getString("general/arch");
      String mode = m_config_file->getString("general/mode");
      String syntax = m_config_file->getString("general/syntax");

      dl::dl_arch dla = dl::DL_ARCH_INTEL;
      if (arch == "intel")
         dla = dl::DL_ARCH_INTEL;
      else if (arch == "riscv")
         dla = dl::DL_ARCH_RISCV;
      else if (arch == "arm")
         dla = dl::DL_ARCH_ARMv8;
      else
         LOG_PRINT_ERROR("Unknown architecture %s", arch.c_str());

      dl::dl_mode dlm = dl::DL_MODE_64;
      if (mode == "32")
         dlm = dl::DL_MODE_32;
      else if (mode == "64")
         dlm = dl::DL_MODE_64;
      else
         LOG_PRINT_ERROR("Unknown mode %s", mode.c_str());

      dl::dl_syntax dls = dl::DL_SYNTAX_INTEL;
      if (syntax == "intel")
         dls = dl::DL_SYNTAX_INTEL;
      else if (syntax == "att")
         dls = dl::DL_SYNTAX_ATT;
      else if (syntax == "xed")
         dls = dl::DL_SYNTAX_XED;
      else
         LOG_PRINT_ERROR("Unknown syntax %s", syntax.c_str());

      m_factory = new dl::DecoderFactory;
      m_decoder = m_factory->CreateDecoder(dla, dlm, dls); // create decoder for [arch, mode, syntax]
   }
}

dl::Decoder *Simulator::getDecoder()
{
   return m_decoder;
}

void Simulator::printInstModeSummary()
{
   // Stub
}
