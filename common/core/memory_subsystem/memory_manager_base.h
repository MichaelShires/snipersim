#ifndef __MEMORY_MANAGER_BASE_H__
#define __MEMORY_MANAGER_BASE_H__

#include "core.h"
#include "mem_component.h"
#include "network.h"
#include "performance_model.h"
#include "pr_l1_pr_l2_dram_directory_msi/shmem_msg.h"
#include "shmem_perf_model.h"
#include "inst_mode.h"
#include "simulation_context.h"

void MemoryManagerNetworkCallback(void *obj, NetPacket packet);

class Core;
class Network;
class PerformanceModel;
class ShmemPerfModel;
class Config; // legacy
namespace config { class Config; } // modern
class DvfsManager;
class StatsManager;
class FaultinjectionManager;
class HooksManager;
class CoreManager;

class MemoryManagerBase
{
 public:
   enum CachingProtocol_t
   {
      PARAMETRIC_DRAM_DIRECTORY_MSI,
      FAST_NEHALEM,
      NUM_CACHING_PROTOCOL_TYPES
   };

 private:
   Core *m_core;
   Network *m_network;
   ShmemPerfModel *m_shmem_perf_model;
   SimulationContext *m_context;
   Config *m_config_legacy;
   config::Config *m_config;
   DvfsManager *m_dvfs_manager;
   StatsManager *m_stats_manager;
   FaultinjectionManager *m_fault_injection_manager;
   HooksManager *m_hooks_manager;
   CoreManager *m_core_manager;

   void parseMemoryControllerList(String &memory_controller_positions, std::vector<core_id_t> &core_list_from_cfg_file,
                                  SInt32 application_core_count);

 public:
   Network *getNetwork()
   {
      return m_network;
   }
   ShmemPerfModel *getShmemPerfModel()
   {
      return m_shmem_perf_model;
   }
   config::Config *getConfig()
   {
      return m_config;
   }
   Config *getConfigLegacy()
   {
      return m_config_legacy;
   }
   DvfsManager *getDvfsManager()
   {
      return m_dvfs_manager;
   }
   StatsManager *getStatsManager()
   {
      return m_stats_manager;
   }
   FaultinjectionManager *getFaultinjectionManager()
   {
      return m_fault_injection_manager;
   }
   HooksManager *getHooksManager()
   {
      return m_hooks_manager;
   }
   CoreManager *getCoreManager()
   {
      return m_core_manager;
   }
   SimulationContext* getContext()
   {
      return m_context;
   }
   InstMode::inst_mode_t getInstrumentationMode()
   {
      return InstMode::getInstrumentationMode();
   }

   std::vector<core_id_t> getCoreListWithMemoryControllers(void);
   void printCoreListWithMemoryControllers(std::vector<core_id_t> &core_list_with_memory_controllers);

 public:
   MemoryManagerBase(Core *core, Network *network, ShmemPerfModel *shmem_perf_model, SimulationContext *context)
       : m_core(core), m_network(network), m_shmem_perf_model(shmem_perf_model), m_context(context),
         m_config_legacy(context->getConfig()), m_config(context->getConfigFile()),
         m_dvfs_manager(context->getDvfsManager()), m_stats_manager(context->getStatsManager()),
         m_fault_injection_manager(context->getFaultinjectionManager()),
         m_hooks_manager(context->getHooksManager()), m_core_manager(context->getCoreManager())
   {
   }
   virtual ~MemoryManagerBase()
   {
   }

   virtual HitWhere::where_t coreInitiateMemoryAccess(MemComponent::component_t mem_component,
                                                      Core::lock_signal_t lock_signal, Core::mem_op_t mem_op_type,
                                                      IntPtr address, UInt32 offset, Byte *data_buf, UInt32 data_length,
                                                      Core::MemModeled modeled) = 0;
   virtual SubsecondTime coreInitiateMemoryAccessFast(bool icache, Core::mem_op_t mem_op_type, IntPtr address)
   {
      // Emulate fast interface by calling into slow interface
      SubsecondTime initial_time = getCore()->getPerformanceModel()->getElapsedTime();
      getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD, initial_time);

      coreInitiateMemoryAccess(icache ? MemComponent::L1_ICACHE : MemComponent::L1_DCACHE, Core::NONE, mem_op_type,
                               address - (address % getCacheBlockSize()), 0, NULL, getCacheBlockSize(),
                               Core::MEM_MODELED_COUNT_TLBTIME);

      // Get the final cycle time
      SubsecondTime final_time = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
      SubsecondTime latency = final_time - initial_time;
      return latency;
   }

   virtual void handleMsgFromNetwork(NetPacket &packet) = 0;

   // FIXME: Take this out of here
   virtual UInt64 getCacheBlockSize() const = 0;

   virtual SubsecondTime getL1HitLatency(void) = 0;
   virtual void addL1Hits(bool icache, Core::mem_op_t mem_op_type, UInt64 hits) = 0;

   virtual core_id_t getShmemRequester(const void *pkt_data) = 0;

   virtual void enableModels() = 0;
   virtual void disableModels() = 0;

   // Modeling
   virtual UInt32 getModeledLength(const void *pkt_data) = 0;

   Core *getCore()
   {
      return m_core;
   }

   virtual void sendMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::msg_t msg_type,
                        MemComponent::component_t sender_mem_component,
                        MemComponent::component_t receiver_mem_component, core_id_t requester, core_id_t receiver,
                        IntPtr address, Byte *data_buf = NULL, UInt32 data_length = 0,
                        HitWhere::where_t where = HitWhere::UNKNOWN, ShmemPerf *perf = NULL,
                        ShmemPerfModel::Thread_t thread_num = ShmemPerfModel::NUM_CORE_THREADS) = 0;
   virtual void broadcastMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::msg_t msg_type,
                             MemComponent::component_t sender_mem_component,
                             MemComponent::component_t receiver_mem_component, core_id_t requester, IntPtr address,
                             Byte *data_buf = NULL, UInt32 data_length = 0, ShmemPerf *perf = NULL,
                             ShmemPerfModel::Thread_t thread_num = ShmemPerfModel::NUM_CORE_THREADS) = 0;

   static CachingProtocol_t parseProtocolType(String &protocol_type);
   static MemoryManagerBase *createMMU(String protocol_type, Core *core, Network *network,
                                       ShmemPerfModel *shmem_perf_model, SimulationContext *context);
};

#endif /* __MEMORY_MANAGER_BASE_H__ */
