#ifndef COHERENCE_ENGINE_H
#define COHERENCE_ENGINE_H

#include "cache_state.h"
#include "core.h"
#include "fixed_types.h"

namespace ParametricDramDirectoryMSI
{

class CoherenceEngine
{
 public:
   virtual ~CoherenceEngine()
   {
   }

   virtual bool isPermissible(CacheState::cstate_t state, Core::mem_op_t mem_op) const = 0;

   // Returns true if an upgrade is needed
   virtual bool needsUpgrade(CacheState::cstate_t state, Core::mem_op_t mem_op) const = 0;
};

class MSICoherenceEngine : public CoherenceEngine
{
 public:
   bool isPermissible(CacheState::cstate_t state, Core::mem_op_t mem_op) const override
   {
      switch (mem_op) {
      case Core::READ:
         return state == CacheState::SHARED || state == CacheState::EXCLUSIVE || state == CacheState::MODIFIED ||
                state == CacheState::OWNED;
      case Core::READ_EX:
      case Core::WRITE:
         return state == CacheState::EXCLUSIVE || state == CacheState::MODIFIED;
      default:
         return false;
      }
   }

   bool needsUpgrade(CacheState::cstate_t state, Core::mem_op_t mem_op) const override
   {
      if (mem_op == Core::READ_EX || mem_op == Core::WRITE) {
         return state == CacheState::SHARED || state == CacheState::OWNED;
      }
      return false;
   }
};

} // namespace ParametricDramDirectoryMSI

#endif
