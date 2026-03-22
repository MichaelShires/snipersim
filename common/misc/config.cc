#include "config.h"
#include "config.hpp"
#include "log.h"
#include "simulator.h"
#include "utils.h"
#include "subsecond_time.h"

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

#undef LOG_ASSERT_ERROR
#define LOG_ASSERT_ERROR(cond, ...) if (!(cond)) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1); }

Config *Config::m_singleton = NULL;

String Config::m_knob_output_directory;
UInt32 Config::m_knob_total_cores;
UInt32 Config::m_knob_num_host_cores;
bool Config::m_knob_enable_smc_support;
bool Config::m_knob_enable_icache_modeling;
bool Config::m_knob_issue_memops_at_functional;
Config::SimulationROI Config::m_knob_roi;
bool Config::m_knob_enable_progress_trace;
bool Config::m_knob_enable_sync;
bool Config::m_knob_enable_sync_report;
bool Config::m_knob_osemu_pthread_replace;
UInt32 Config::m_knob_osemu_nprocs;
bool Config::m_knob_osemu_clock_replace;
time_t Config::m_knob_osemu_time_start;
bool Config::m_knob_bbvs;
ComponentPeriod Config::m_knob_core_frequency;
ClockSkewMinimizationObject::Scheme Config::m_knob_clock_skew_minimization_scheme;
UInt64 Config::m_knob_hpi_percore;
UInt64 Config::m_knob_hpi_global;
bool Config::m_knob_enable_spinloopdetection;
bool Config::m_suppress_stdout;
bool Config::m_suppress_stderr;
bool Config::m_circular_log_enabled;
bool Config::m_knob_enable_pinplay;
bool Config::m_knob_enable_syscall_emulation;
CacheEfficiencyTracker::Callbacks Config::m_cache_efficiency_callbacks;

Config::Config(SimulationMode mode)
{
   m_knob_output_directory = Sim()->getCfg()->getString("general/output_dir");
   m_knob_total_cores = Sim()->getCfg()->getInt("general/total_cores");

   m_knob_num_host_cores = Sim()->getCfg()->getInt("general/num_host_cores");
   if (m_knob_num_host_cores == 0) {
      cpu_set_t mask;
      int res = sched_getaffinity(0, sizeof(mask), &mask);
      if (res == 0) {
         for (UInt32 cpu = 0; cpu < (UInt32)sysconf(_SC_NPROCESSORS_ONLN); ++cpu)
            if (CPU_ISSET(cpu, &mask))
               ++m_knob_num_host_cores;
      }
      else
         m_knob_num_host_cores = sysconf(_SC_NPROCESSORS_ONLN);
   }

   m_knob_enable_smc_support = Sim()->getCfg()->getBool("general/enable_smc_support");
   m_knob_enable_icache_modeling = Sim()->getCfg()->getBool("general/enable_icache_modeling");
   m_knob_issue_memops_at_functional = Sim()->getCfg()->getBool("general/issue_memops_at_functional");

   if (Sim()->getCfg()->getBool("general/roi_script"))
      m_knob_roi = ROI_SCRIPT;
   else if (Sim()->getCfg()->getBool("general/magic"))
      m_knob_roi = ROI_MAGIC;
   else
      m_knob_roi = ROI_FULL;

   m_knob_enable_progress_trace = Sim()->getCfg()->getBool("progress_trace/enabled");
   m_knob_enable_sync = Sim()->getCfg()->getString("clock_skew_minimization/scheme") != "none";
   m_knob_enable_sync_report = Sim()->getCfg()->getBool("clock_skew_minimization/report");

   m_simulation_mode = mode;
   m_knob_bbvs = false;

   m_knob_osemu_pthread_replace = Sim()->getCfg()->getBool("osemu/pthread_replace");
   m_knob_osemu_nprocs = Sim()->getCfg()->getInt("osemu/nprocs");
   m_knob_osemu_clock_replace = Sim()->getCfg()->getBool("osemu/clock_replace");
   m_knob_osemu_time_start = Sim()->getCfg()->getInt("osemu/time_start");

   m_knob_hpi_percore = Sim()->getCfg()->getInt("core/hook_periodic_ins/ins_per_core");
   m_knob_hpi_global = Sim()->getCfg()->getInt("core/hook_periodic_ins/ins_global");

   UInt64 freq_hz = (UInt64(Sim()->getCfg()->getFloat("perf_model/core/frequency") * 1000) * 1000000);
   m_knob_core_frequency = ComponentPeriod(SubsecondTime::SEC() / freq_hz);

   m_knob_clock_skew_minimization_scheme = ClockSkewMinimizationObject::parseScheme(Sim()->getCfg()->getString("clock_skew_minimization/scheme"));
   m_knob_enable_spinloopdetection = Sim()->getCfg()->getBool("core/spin_loop_detection/enabled");
   m_suppress_stdout = Sim()->getCfg()->getBool("general/suppress_stdout");
   m_suppress_stderr = Sim()->getCfg()->getBool("general/suppress_stderr");
   m_circular_log_enabled = Sim()->getCfg()->getBool("general/circular_log");
   m_knob_enable_pinplay = Sim()->getCfg()->getBool("general/enable_pinplay");
   m_knob_enable_syscall_emulation = Sim()->getCfg()->getBool("general/enable_syscall_emulation");

   m_core_id_length = computeCoreIDLength(m_knob_total_cores);
}

Config::~Config()
{
}

Config *Config::getSingleton()
{
   return Sim()->getConfig();
}

UInt32 Config::getTotalCores()
{
   return m_knob_total_cores;
}

UInt32 Config::getApplicationCores()
{
   return m_knob_total_cores;
}

void Config::updateCommToCoreMap(UInt32 comm_id, core_id_t core_id)
{
   m_comm_to_core_map[comm_id] = core_id;
}

UInt32 Config::getCoreFromCommId(UInt32 comm_id)
{
   return m_comm_to_core_map[comm_id];
}

void Config::getNetworkModels(UInt32 *models) const
{
   // Stub
}

UInt32 Config::computeCoreIDLength(UInt32 core_count)
{
   if (core_count <= 256)
      return 1;
   else if (core_count <= 65536)
      return 2;
   else
      return 4;
}

String Config::getOutputDirectory() const
{
   return m_knob_output_directory;
}

String Config::formatOutputFileName(String filename) const
{
   return m_knob_output_directory + "/" + filename;
}

void Config::logCoreMap()
{
}

void Config::setCacheEfficiencyCallbacks(CacheEfficiencyTracker::CallbackGetOwner get_owner_func,
                                         CacheEfficiencyTracker::CallbackNotifyAccess notify_access_func,
                                         CacheEfficiencyTracker::CallbackNotifyEvict notify_evict_func, UInt64 user_arg)
{
   m_cache_efficiency_callbacks.get_owner_func = get_owner_func;
   m_cache_efficiency_callbacks.notify_access_func = notify_access_func;
   m_cache_efficiency_callbacks.notify_evict_func = notify_evict_func;
   m_cache_efficiency_callbacks.user_arg = user_arg;
}
