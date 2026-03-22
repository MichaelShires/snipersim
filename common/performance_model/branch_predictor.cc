#include "branch_predictor.h"
#include "config.hpp"
#include "simulator.h"
#include "stats.h"

BranchPredictor::BranchPredictor() : m_correct_predictions(0), m_incorrect_predictions(0)
{
}

BranchPredictor::BranchPredictor(String name, core_id_t core_id) : m_correct_predictions(0), m_incorrect_predictions(0)
{
   registerStatsMetric(name, core_id, "num-correct", &m_correct_predictions);
   registerStatsMetric(name, core_id, "num-incorrect", &m_incorrect_predictions);
}

BranchPredictor::~BranchPredictor()
{
}

UInt64 BranchPredictor::m_mispredict_penalty;

BranchPredictor *BranchPredictor::create(core_id_t core_id)
{
   try {
      config::Config *cfg = Sim()->getCfg();
      assert(cfg);

      m_mispredict_penalty = cfg->getIntArray("perf_model/branch_predictor/mispredict_penalty", core_id);

      String type = cfg->getStringArray("perf_model/branch_predictor/type", core_id);
      if (type == "none") {
         return nullptr;
      }

      BranchPredictor* predictor = ComponentRegistry<BranchPredictor, String, core_id_t>::get().create(type, "branch_predictor", core_id);
      if (!predictor) {
         LOG_PRINT_ERROR("Invalid branch predictor type: %s", type.c_str());
      }
      return predictor;
   } catch (...) {
      LOG_PRINT_ERROR("Config info not available while constructing branch predictor.");
      return nullptr;
   }
}

UInt64 BranchPredictor::getMispredictPenalty()
{
   return m_mispredict_penalty;
}

void BranchPredictor::resetCounters()
{
   m_correct_predictions = 0;
   m_incorrect_predictions = 0;
}

void BranchPredictor::updateCounters(bool predicted, bool actual)
{
   if (predicted == actual)
      ++m_correct_predictions;
   else
      ++m_incorrect_predictions;
}
