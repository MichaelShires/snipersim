#ifndef SIMULATION_CONTEXT_H
#define SIMULATION_CONTEXT_H

#include "fixed_types.h"

class Config;
namespace config { class Config; }
class TagsManager;
class SyscallServer;
class SyncServer;
class MagicServer;
class ClockSkewMinimizationServer;
class StatsManager;
class Transport;
class CoreManager;
class ThreadManager;
class ThreadStatsManager;
class SimThreadManager;
class ClockSkewMinimizationManager;
class FastForwardPerformanceManager;
class TraceManager;
class DvfsManager;
class HooksManager;
class SamplingManager;
class FaultinjectionManager;
class RoutineTracer;
class MemoryTracker;
class Simulator;

class SimulationContext {
public:
    SimulationContext() :
        m_config(nullptr),
        m_config_file(nullptr),
        m_tags_manager(nullptr),
        m_syscall_server(nullptr),
        m_sync_server(nullptr),
        m_magic_server(nullptr),
        m_clock_skew_minimization_server(nullptr),
        m_stats_manager(nullptr),
        m_transport(nullptr),
        m_core_manager(nullptr),
        m_thread_manager(nullptr),
        m_thread_stats_manager(nullptr),
        m_sim_thread_manager(nullptr),
        m_clock_skew_minimization_manager(nullptr),
        m_fastforward_performance_manager(nullptr),
        m_trace_manager(nullptr),
        m_dvfs_manager(nullptr),
        m_hooks_manager(nullptr),
        m_sampling_manager(nullptr),
        m_faultinjection_manager(nullptr),
        m_rtn_tracer(nullptr),
        m_memory_tracker(nullptr),
        m_simulator(nullptr)
    {}

    // Setters
    void setConfig(Config* config) { m_config = config; }
    void setConfigFile(config::Config* config_file) { m_config_file = config_file; }
    void setTagsManager(TagsManager* tags_manager) { m_tags_manager = tags_manager; }
    void setSyscallServer(SyscallServer* syscall_server) { m_syscall_server = syscall_server; }
    void setSyncServer(SyncServer* sync_server) { m_sync_server = sync_server; }
    void setMagicServer(MagicServer* magic_server) { m_magic_server = magic_server; }
    void setClockSkewMinimizationServer(ClockSkewMinimizationServer* server) { m_clock_skew_minimization_server = server; }
    void setStatsManager(StatsManager* stats_manager) { m_stats_manager = stats_manager; }
    void setTransport(Transport* transport) { m_transport = transport; }
    void setCoreManager(CoreManager* core_manager) { m_core_manager = core_manager; }
    void setThreadManager(ThreadManager* thread_manager) { m_thread_manager = thread_manager; }
    void setThreadStatsManager(ThreadStatsManager* manager) { m_thread_stats_manager = manager; }
    void setSimThreadManager(SimThreadManager* manager) { m_sim_thread_manager = manager; }
    void setClockSkewMinimizationManager(ClockSkewMinimizationManager* manager) { m_clock_skew_minimization_manager = manager; }
    void setFastForwardPerformanceManager(FastForwardPerformanceManager* manager) { m_fastforward_performance_manager = manager; }
    void setTraceManager(TraceManager* trace_manager) { m_trace_manager = trace_manager; }
    void setDvfsManager(DvfsManager* dvfs_manager) { m_dvfs_manager = dvfs_manager; }
    void setHooksManager(HooksManager* hooks_manager) { m_hooks_manager = hooks_manager; }
    void setSamplingManager(SamplingManager* sampling_manager) { m_sampling_manager = sampling_manager; }
    void setFaultinjectionManager(FaultinjectionManager* manager) { m_faultinjection_manager = manager; }
    void setRoutineTracer(RoutineTracer* rtn_tracer) { m_rtn_tracer = rtn_tracer; }
    void setMemoryTracker(MemoryTracker* memory_tracker) { m_memory_tracker = memory_tracker; }
    void setSimulator(Simulator* simulator) { m_simulator = simulator; }

    // Getters
    Config* getConfig() const { return m_config; }
    config::Config* getConfigFile() const { return m_config_file; }
    TagsManager* getTagsManager() const { return m_tags_manager; }
    SyscallServer* getSyscallServer() const { return m_syscall_server; }
    SyncServer* getSyncServer() const { return m_sync_server; }
    MagicServer* getMagicServer() const { return m_magic_server; }
    ClockSkewMinimizationServer* getClockSkewMinimizationServer() const { return m_clock_skew_minimization_server; }
    StatsManager* getStatsManager() const { return m_stats_manager; }
    Transport* getTransport() const { return m_transport; }
    CoreManager* getCoreManager() const { return m_core_manager; }
    ThreadManager* getThreadManager() const { return m_thread_manager; }
    ThreadStatsManager* getThreadStatsManager() const { return m_thread_stats_manager; }
    SimThreadManager* getSimThreadManager() const { return m_sim_thread_manager; }
    ClockSkewMinimizationManager* getClockSkewMinimizationManager() const { return m_clock_skew_minimization_manager; }
    FastForwardPerformanceManager* getFastForwardPerformanceManager() const { return m_fastforward_performance_manager; }
    TraceManager* getTraceManager() const { return m_trace_manager; }
    DvfsManager* getDvfsManager() const { return m_dvfs_manager; }
    HooksManager* getHooksManager() const { return m_hooks_manager; }
    SamplingManager* getSamplingManager() const { return m_sampling_manager; }
    FaultinjectionManager* getFaultinjectionManager() const { return m_faultinjection_manager; }
    RoutineTracer* getRoutineTracer() const { return m_rtn_tracer; }
    MemoryTracker* getMemoryTracker() const { return m_memory_tracker; }
    Simulator* getSimulator() const { return m_simulator; }

private:
    Config* m_config;
    config::Config* m_config_file;
    TagsManager* m_tags_manager;
    SyscallServer* m_syscall_server;
    SyncServer* m_sync_server;
    MagicServer* m_magic_server;
    ClockSkewMinimizationServer* m_clock_skew_minimization_server;
    StatsManager* m_stats_manager;
    Transport* m_transport;
    CoreManager* m_core_manager;
    ThreadManager* m_thread_manager;
    ThreadStatsManager* m_thread_stats_manager;
    SimThreadManager* m_sim_thread_manager;
    ClockSkewMinimizationManager* m_clock_skew_minimization_manager;
    FastForwardPerformanceManager* m_fastforward_performance_manager;
    TraceManager* m_trace_manager;
    DvfsManager* m_dvfs_manager;
    HooksManager* m_hooks_manager;
    SamplingManager* m_sampling_manager;
    FaultinjectionManager* m_faultinjection_manager;
    RoutineTracer* m_rtn_tracer;
    MemoryTracker* m_memory_tracker;
    Simulator* m_simulator;
};

#endif // SIMULATION_CONTEXT_H
