#ifndef CACHE_LATENCY_MODEL_H
#define CACHE_LATENCY_MODEL_H

#include "shmem_perf.h"
#include "subsecond_time.h"

namespace ParametricDramDirectoryMSI
{

class CacheLatencyModel
{
 private:
   SubsecondTime m_total_latency;
   ShmemPerf *m_shmem_perf;

 public:
   CacheLatencyModel() : m_total_latency(SubsecondTime::Zero()), m_shmem_perf(new ShmemPerf())
   {
   }
   ~CacheLatencyModel()
   {
      delete m_shmem_perf;
   }

   void addLatency(SubsecondTime latency)
   {
      m_total_latency += latency;
   }

   SubsecondTime getTotalLatency() const
   {
      return m_total_latency;
   }
   SubsecondTime *getTotalLatencyPtr()
   {
      return &m_total_latency;
   }
   ShmemPerf *getShmemPerf()
   {
      return m_shmem_perf;
   }
};

} // namespace ParametricDramDirectoryMSI

#endif
