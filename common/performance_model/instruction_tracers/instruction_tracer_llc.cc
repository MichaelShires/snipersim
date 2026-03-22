#include "instruction_tracer_llc.h"
#include "config.hpp"
#include "core.h"
#include "dynamic_micro_op.h"
#include "hit_where.h"
#include "instruction.h"
#include "micro_op.h"
#include "simulator.h"
#include <set>

/**
 * InstructionTracerLLC implementation.
 * Leverages Intel XED (via Sniper's Decoder interface) to identify register and memory dependencies
 * and build a back-slice of instructions leading to long-latency LLC misses.
 */

InstructionTracerLLC::InstructionTracerLLC(const Core *core) : m_core(core)
{
   if (Sim()->getCfg()->hasKey("instruction_tracer/llc/window_size"))
      m_window_size = Sim()->getCfg()->getInt("instruction_tracer/llc/window_size");
   else
      m_window_size = 100;
}

InstructionTracerLLC::~InstructionTracerLLC()
{
}

void InstructionTracerLLC::traceInstruction(const DynamicMicroOp *uop, uop_times_t *times)
{
   // Only add to history if it's the first uop of the instruction
   if (uop->isFirst()) {
      InstInfo info;
      const MicroOp *mop = uop->getMicroOp();
      if (mop) {
         info.ip = mop->getInstructionPointer().address;
         if (mop->getDecodedInstruction()) {
            info.disassembly = mop->getDecodedInstruction()->disassembly_to_str().c_str();
         }
         else if (mop->getInstruction() && !mop->getInstruction()->getDisassembly().empty()) {
            info.disassembly = mop->getInstruction()->getDisassembly();
         }
         else {
            info.disassembly = Sim()->getDecoder()->inst_name(mop->getInstructionOpcode());
         }

         // Collect registers. We use a set to avoid duplicates.
         // Source registers + Address registers (both are consumers for calculating the final value/address)
         std::set<uint32_t> consumers;
         for (uint32_t i = 0; i < mop->getSourceRegistersLength(); ++i)
            consumers.insert(mop->getSourceRegister(i));
         for (uint32_t i = 0; i < mop->getAddressRegistersLength(); ++i)
            consumers.insert(mop->getAddressRegister(i));

         for (uint32_t reg : consumers)
            if (reg != 0)
               info.src_regs.push_back(reg);

         for (uint32_t i = 0; i < mop->getDestinationRegistersLength(); ++i) {
            uint32_t reg = mop->getDestinationRegister(i);
            if (reg != 0)
               info.dst_regs.push_back(reg);
         }
      }
      else {
         info.ip = 0;
         info.disassembly = "no-mop";
      }
      info.is_miss = false;

      m_history.push_back(info);
      if (m_history.size() > m_window_size) {
         m_history.pop_front();
      }
   }

   // Accumulate memory info for the current instruction
   if (!m_history.empty()) {
      const MicroOp *mop = uop->getMicroOp();
      if (mop->isLoad() || mop->isStore()) {
         MemAccess ma;
         ma.addr = uop->getAddress().address;
         ma.is_load = mop->isLoad();
         ma.is_store = mop->isStore();
         m_history.back().mem_accesses.push_back(ma);
      }
   }

   // Check for LLC miss on this uop
   HitWhere::where_t where = uop->getDCacheHitWhere();
   if (where == HitWhere::DRAM || where == HitWhere::DRAM_LOCAL || where == HitWhere::DRAM_REMOTE ||
       where == HitWhere::MISS)
   {
      if (!m_history.empty()) {
         m_history.back().is_miss = true;
      }
   }

   // If this is the last uop of the instruction and we have a miss, print the trace
   if (uop->isLast() && !m_history.empty() && m_history.back().is_miss) {
      printTrace();
   }
}

void InstructionTracerLLC::printTree(int index, int depth, uint32_t target_reg, IntPtr target_addr)
{
   if (depth > 5 || index < 0)
      return;

   for (int i = 0; i < depth; ++i)
      std::cout << "  ";

   if (target_reg != 0) {
      std::cout << "[" << Sim()->getDecoder()->reg_name(target_reg) << "] <- ";
   }
   else if (target_addr != 0) {
      std::cout << "[mem:0x" << std::hex << target_addr << std::dec << "] <- ";
   }

   std::cout << "0x" << std::hex << m_history[index].ip << std::dec << " : " << m_history[index].disassembly
             << std::endl;

   // For each source register of this instruction, find its most recent producer
   for (uint32_t src : m_history[index].src_regs) {
      // Search backwards from index-1
      for (int i = index - 1; i >= 0; --i) {
         bool found = false;
         for (uint32_t dst : m_history[i].dst_regs) {
            if (src == dst) {
               printTree(i, depth + 1, src, 0);
               found = true;
               break;
            }
         }
         if (found)
            break;
      }
   }

   // For each load of this instruction, find the most recent store to the same address
   for (const auto &ma : m_history[index].mem_accesses) {
      if (ma.is_load) {
         for (int i = index - 1; i >= 0; --i) {
            bool found = false;
            for (const auto &prev_ma : m_history[i].mem_accesses) {
               if (prev_ma.is_store && prev_ma.addr == ma.addr) {
                  printTree(i, depth + 1, 0, ma.addr);
                  found = true;
                  break;
               }
            }
            if (found)
               break;
         }
      }
   }
}

void InstructionTracerLLC::printTrace()
{
   std::cout << "[LLC_MISS_TRACE:" << m_core->getId() << "] Dependency back-slice for miss at IP 0x" << std::hex
             << m_history.back().ip << std::dec << " : " << m_history.back().disassembly << std::endl;

   printTree((int)m_history.size() - 1, 0, 0, 0);
   std::cout << "--------------------------------------------------------" << std::endl;
}
