#ifndef __PR_L1_CACHE_BLOCK_INFO_H__
#define __PR_L1_CACHE_BLOCK_INFO_H__

#include "cache_block_info.h"
#include "cache_state.h"

class PrL1CacheBlockInfo : public CacheBlockInfo
{
 public:
   PrL1CacheBlockInfo(IntPtr tag = ~0, CacheState::cstate_t cstate = CacheState::INVALID) : CacheBlockInfo(tag, cstate)
   {
   }

   ~PrL1CacheBlockInfo()
   {
   }
};
#endif /* __PR_L1_CACHE_BLOCK_INFO_H__ */
