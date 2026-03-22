#include "cache_set_round_robin.h"

CacheSetRoundRobin::CacheSetRoundRobin(CacheBase::cache_t cache_type, UInt32 associativity, UInt32 blocksize)
    : CacheSet(cache_type, associativity, blocksize)
{
   m_replacement_index = m_associativity - 1;
}

CacheSetRoundRobin::~CacheSetRoundRobin()
{
}

UInt32 CacheSetRoundRobin::getReplacementIndex(CacheCntlr *cntlr)
{
   UInt32 curr_replacement_index = m_replacement_index;
   m_replacement_index = (m_replacement_index == 0) ? (m_associativity - 1) : (m_replacement_index - 1);

   if (!isValidReplacement(m_replacement_index))
      return getReplacementIndex(cntlr);
   else
      return curr_replacement_index;
}

void CacheSetRoundRobin::updateReplacementIndex(UInt32 accessed_index)
{
   return;
}

static ComponentRegistrar<CacheSet, String, core_id_t, CacheBase::cache_t, UInt32, UInt32, CacheSetInfo*> 
   round_robin_registrar("round_robin", [](String cfgname, core_id_t core_id, CacheBase::cache_t cache_type, UInt32 associativity, UInt32 blocksize, CacheSetInfo *set_info) -> CacheSet* {
      return new CacheSetRoundRobin(cache_type, associativity, blocksize);
   });
