#include "config.hpp"
#include "one_bit_branch_predictor.h"
#include "simulator.h"

OneBitBranchPredictor::OneBitBranchPredictor(String name, core_id_t core_id, UInt32 size)
    : BranchPredictor(name, core_id), m_bits(size)
{
}

OneBitBranchPredictor::~OneBitBranchPredictor()
{
}

bool OneBitBranchPredictor::predict(bool indirect, IntPtr ip, IntPtr target)
{
   UInt32 index = ip % m_bits.size();
   return m_bits[index];
}

void OneBitBranchPredictor::update(bool predicted, bool actual, bool indirect, IntPtr ip, IntPtr target)
{
   updateCounters(predicted, actual);
   UInt32 index = ip % m_bits.size();
   m_bits[index] = actual;
}

static ComponentRegistrar<BranchPredictor, String, core_id_t> 
   one_bit_registrar("one_bit", [](String name, core_id_t core_id) -> BranchPredictor* {
      UInt32 size = Sim()->getCfg()->getIntArray("perf_model/branch_predictor/size", core_id);
      return new OneBitBranchPredictor(name, core_id, size);
   });
