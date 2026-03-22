#ifndef CORE_H
#define CORE_H

// some forward declarations for cross includes
class Thread;
class Network;
class MemoryManagerBase;
class MemoryManagerFast;
class PerformanceModel;
class ClockSkewMinimizationClient;
class ShmemPerfModel;
class TopologyInfo;
class CheetahManager;
class DvfsManager;
class StatsManager;
class FaultinjectionManager;
class HooksManager;
class CoreManager;
class Config;
class Simulator;
class SimulationContext;
namespace config { class Config; }

#include "bbv_count.h"
#include "cpuid.h"
#include "fixed_types.h"
#include "hit_where.h"
#include "lock.h"
#include "mem_component.h"
#include "subsecond_time.h"
#include "simulation_context.h"

struct MemoryResult
{
   HitWhere::where_t hit_where;
   SubsecondTime latency;
};

MemoryResult makeMemoryResult(HitWhere::where_t _hit_where, SubsecondTime _latency);

class Core
{
 public:
   enum State
   {
      RUNNING = 0,
      INITIALIZING,
      STALLED,
      SLEEPING,
      WAKING_UP,
      IDLE,
      BROKEN,
      NUM_STATES
   };

   enum mem_op_t
   {
      READ = 0,
      READ_EX,
      WRITE,
      NUM_MEM_OP_TYPES
   };

   enum lock_signal_t
   {
      NONE = 0,
      LOCK,
      UNLOCK
   };

   enum MemModeled
   {
      MEM_MODELED_NONE = 0,
      MEM_MODELED_COUNT,         /* Only count instructions, do not model memory access */
      MEM_MODELED_COUNT_TLBTIME, /* Count + model TLB time */
      MEM_MODELED_TIME,          /* Count + model time */
      MEM_MODELED_FENCED,        /* Fenced (strictly ordered) memory access */
      MEM_MODELED_RETURN,        /* Count + time + return data to construct DynamicInstruction */
   };

   static const char *CoreStateString(State state);

   Core(SInt32 id, SimulationContext *context);
   ~Core();

   // Query and update branch predictor, return true on mispredict
   bool accessBranchPredictor(IntPtr eip, bool taken, bool indirect, IntPtr target);

   MemoryResult readInstructionMemory(IntPtr address, UInt32 instruction_size);

   MemoryResult initiateMemoryAccess(MemComponent::component_t mem_component, lock_signal_t lock_signal,
                                     mem_op_t mem_op_type, IntPtr address, Byte *data_buf, UInt32 data_size,
                                     MemModeled modeled, IntPtr eip, SubsecondTime now, bool is_fault_mask = false);

   MemoryResult accessMemory(lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr d_addr, char *data_buffer,
                             UInt32 data_size, MemModeled modeled = MEM_MODELED_TIME, IntPtr eip = 0,
                             SubsecondTime now = SubsecondTime::Zero(), bool is_fault_mask = false);

   MemoryResult nativeMemOp(lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr address, char *data_buf,
                    UInt32 data_size);

   bool countInstructions(IntPtr address, UInt32 count);
   void updateSpinCount(UInt64 instructions, SubsecondTime elapsed_time);

   void enablePerformanceModels();
   void disablePerformanceModels();

   void updateThreadStats(SubsecondTime time);

   // Getters
   core_id_t getId() const
   {
      return m_core_id;
   }
   const ComponentPeriod *getDvfsDomain() const
   {
      return m_dvfs_domain;
   }
   MemoryManagerBase *getMemoryManager() const
   {
      return m_memory_manager;
   }
   Thread *getThread() const
   {
      return m_thread;
   }
   Network *getNetwork() const
   {
      return m_network;
   }
   PerformanceModel *getPerformanceModel() const
   {
      return m_performance_model;
   }
   ClockSkewMinimizationClient *getClockSkewMinimizationClient() const
   {
      return m_clock_skew_minimization_client;
   }
   ShmemPerfModel *getShmemPerfModel() const
   {
      return m_shmem_perf_model;
   }
   State getState() const
   {
      return m_core_state;
   }
   void setState(State core_state)
   {
      m_core_state = core_state;
   }
   UInt64 getInstructionCount() const
   {
      return m_instructions;
   }
   BbvCount *getBbvCount() const
   {
      return const_cast<BbvCount*>(&m_bbv);
   }
   void setThread(Thread *thread)
   {
      m_thread = thread;
   }
   TopologyInfo *getTopologyInfo() const
   {
      return m_topology_info;
   }
   CheetahManager *getCheetahManager() const
   {
      return m_cheetah_manager;
   }
   SimulationContext* getContext() const { return m_context; }

   Lock &getMemoryLock()
   {
      return m_mem_lock;
   }

   void setInstructionsCallback(UInt64 count)
   {
      m_instructions_callback = m_instructions + count;
   }
   void disableInstructionsCallback()
   {
      m_instructions_callback = UINT64_MAX;
   }
   bool isEnabledInstructionsCallback() const
   {
      return m_instructions_callback != UINT64_MAX;
   }

   void emulateCpuid(UInt32 eax, UInt32 ecx, cpuid_result_t &res) const;

   void logMemoryHit(bool icache, mem_op_t mem_op_type, IntPtr address, MemModeled modeled, IntPtr eip);
   void accessMemoryFast(bool icache, mem_op_t mem_op_type, IntPtr address);

 private:
   core_id_t m_core_id;
   const ComponentPeriod *m_dvfs_domain;
   MemoryManagerBase *m_memory_manager;
   Thread *m_thread;
   Network *m_network;
   PerformanceModel *m_performance_model;
   ClockSkewMinimizationClient *m_clock_skew_minimization_client;
   Lock m_mem_lock;
   ShmemPerfModel *m_shmem_perf_model;
   BbvCount m_bbv;
   TopologyInfo *m_topology_info;
   CheetahManager *m_cheetah_manager;

   DvfsManager *m_dvfs_manager;
   StatsManager *m_stats_manager;
   FaultinjectionManager *m_fault_injection_manager;
   HooksManager *m_hooks_manager;
   CoreManager *m_core_manager;
   Config *m_config;
   config::Config *m_cfg;
   Simulator *m_simulator;
   SimulationContext *m_context;

   State m_core_state;

   static Lock m_global_core_lock;

   void hookPeriodicInsCheck();
   void hookPeriodicInsCall();

   IntPtr m_icache_last_block;

   UInt64 m_spin_loops;
   UInt64 m_spin_instructions;
   SubsecondTime m_spin_elapsed_time;

   UInt64 m_instructions;
   UInt64 m_instructions_callback;
   // HOOK_PERIODIC_INS implementation
   UInt64 m_instructions_hpi_callback;
   UInt64 m_instructions_hpi_last;
   static UInt64 g_instructions_hpi_global;
   static UInt64 g_instructions_hpi_global_callback;
};

#endif
