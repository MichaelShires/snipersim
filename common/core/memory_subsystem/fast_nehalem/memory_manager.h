#ifndef __FAST_NEHALEM_H
#define __FAST_NEHALEM_H

#include "memory_manager_fast.h"
#include "simulation_context.h"

namespace FastNehalem
{
class CacheBase
{
 public:
   virtual ~CacheBase()
   {
   }
   virtual SubsecondTime access(Core::mem_op_t mem_op_type, IntPtr tag) = 0;
};

class MemoryManager : public MemoryManagerFast
{
 private:
   CacheBase *icache, *dcache, *l2cache;
   static CacheBase *l3cache, *dram;

 public:
   MemoryManager(Core *core, Network *network, ShmemPerfModel *shmem_perf_model, SimulationContext *context);
   ~MemoryManager();

   SubsecondTime coreInitiateMemoryAccessFast(bool use_icache, Core::mem_op_t mem_op_type, IntPtr address)
   {
      IntPtr tag = address >> CACHE_LINE_BITS;
      return (use_icache ? icache : dcache)->access(mem_op_type, tag);
   }
};
} // namespace FastNehalem

#endif // __FAST_NEHALEM_H
