#include "instruction_tracer_llc.h"
#include "config.hpp"
#include "core.h"
#include "dynamic_micro_op.h"
#include "hit_where.h"
#include "instruction.h"
#include "micro_op.h"
#include "simulator.h"
#include <set>
#include <fstream>
#include <iostream>

InstructionTracerLLC::InstructionTracerLLC(const Core *core) : m_core(core), m_current_seq_id(0)
{
   if (Sim()->getCfg()->hasKey("instruction_tracer/llc/window_size"))
      m_window_size = Sim()->getCfg()->getInt("instruction_tracer/llc/window_size");
   else
      m_window_size = 4096; // Much larger default window size for guaranteed depth
}

InstructionTracerLLC::~InstructionTracerLLC()
{
   dumpEdges();
}

void InstructionTracerLLC::traceInstruction(const DynamicMicroOp *uop, uop_times_t *times)
{
   // Process only once per instruction (uop->isFirst())
   if (uop->isFirst()) {
      m_current_seq_id++;
      
      InstInfo info;
      info.seq_id = m_current_seq_id;
      info.is_miss = false;

      const MicroOp *mop = uop->getMicroOp();
      if (mop) {
         info.ip = mop->getInstructionPointer().address;
         if (mop->getDecodedInstruction()) {
            info.disassembly = mop->getDecodedInstruction()->disassembly_to_str().c_str();
         } else if (mop->getInstruction() && !mop->getInstruction()->getDisassembly().empty()) {
            info.disassembly = mop->getInstruction()->getDisassembly();
         } else {
            info.disassembly = Sim()->getDecoder()->inst_name(mop->getInstructionOpcode());
         }
         info.opcode = Sim()->getDecoder()->inst_name(mop->getInstructionOpcode());

         // Build consumers set
         std::set<uint32_t> consumers;
         for (uint32_t i = 0; i < mop->getSourceRegistersLength(); ++i)
            consumers.insert(mop->getSourceRegister(i));
         for (uint32_t i = 0; i < mop->getAddressRegistersLength(); ++i)
            consumers.insert(mop->getAddressRegister(i));

         // Lookup producers for registers
         for (uint32_t reg : consumers) {
            if (reg != 0) {
               auto it = last_reg_writer.find(reg);
               if (it != last_reg_writer.end()) {
                  info.producers.push_back(it->second);
               }
            }
         }

         // Update producers for registers written by this instruction
         for (uint32_t i = 0; i < mop->getDestinationRegistersLength(); ++i) {
            uint32_t reg = mop->getDestinationRegister(i);
            if (reg != 0) {
               last_reg_writer[reg] = m_current_seq_id;
            }
         }
      } else {
         info.ip = 0;
         info.disassembly = "no-mop";
         info.opcode = "no-mop";
      }

      m_history[m_current_seq_id] = info;
      m_seq_queue.push_back(m_current_seq_id);

      if (m_seq_queue.size() > m_window_size) {
         uint64_t old_seq = m_seq_queue.front();
         m_history.erase(old_seq);
         m_seq_queue.pop_front();
      }
   }

   // Update memory producers/consumers
   const MicroOp *mop = uop->getMicroOp();
   if (mop) {
      if (mop->isLoad()) {
         auto it = last_mem_store.find(uop->getAddress().address);
         if (it != last_mem_store.end()) {
            // It's possible the seq_id is no longer in m_history if it's too old,
            // but we'll check it during traversal
            m_history[m_current_seq_id].producers.push_back(it->second);
         }
      }
      if (mop->isStore()) {
         last_mem_store[uop->getAddress().address] = m_current_seq_id;
      }
   }

   // Check for LLC miss on this uop
   HitWhere::where_t where = uop->getDCacheHitWhere();
   if (where == HitWhere::DRAM || where == HitWhere::DRAM_LOCAL || where == HitWhere::DRAM_REMOTE ||
       where == HitWhere::MISS)
   {
      m_history[m_current_seq_id].is_miss = true;
   }

   // If this is the last uop of the instruction and we have a miss, traverse!
   if (uop->isLast() && m_history[m_current_seq_id].is_miss) {
      traverseGraph(m_current_seq_id, 0, m_history[m_current_seq_id]);
   }
}

void InstructionTracerLLC::traverseGraph(uint64_t seq_id, int depth, const InstInfo& original_miss)
{
   if (depth > 5) return;

   auto it = m_history.find(seq_id);
   if (it == m_history.end()) return;

   const InstInfo& current = it->second;

   // Log edge from producer to the original miss instruction
   if (depth > 0) {
      addEdge(current, original_miss);
   }

   for (uint64_t prod_seq : current.producers) {
       traverseGraph(prod_seq, depth + 1, original_miss);
   }
}

void InstructionTracerLLC::addEdge(const InstInfo& parent, const InstInfo& child)
{
   int64_t dist = (int64_t)child.ip - (int64_t)parent.ip;
   
   // Only consider candidates close in program order (e.g., macro-fusion range)
   if (dist > 0 && dist < 32) {
       EdgeKey key;
       key.parent_opcode = parent.opcode;
       key.child_opcode = child.opcode;
       key.ip_dist = dist;
       m_edge_histogram[key]++;
   }
}

void InstructionTracerLLC::dumpEdges()
{
   if (m_edge_histogram.empty()) return;

   std::string output_dir = ".";
   if (Sim()->getCfg()->hasKey("general/output_dir")) {
      output_dir = Sim()->getCfg()->getString("general/output_dir");
   }
   std::string filename = output_dir + "/llc_fusion_edges.csv";
   std::ofstream out(filename);
   out << "Parent_Opcode,Child_Opcode,IP_Distance,Frequency\n";
   
   for (const auto& pair : m_edge_histogram) {
       out << pair.first.parent_opcode << ","
           << pair.first.child_opcode << ","
           << pair.first.ip_dist << ","
           << pair.second << "\n";
   }
}
