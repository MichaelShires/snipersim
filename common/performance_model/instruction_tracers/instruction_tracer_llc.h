#ifndef __INSTRUCTION_TRACER_LLC_H
#define __INSTRUCTION_TRACER_LLC_H

#include "fixed_types.h"
#include "instruction_tracer.h"
#include <deque>
#include <vector>
#include <unordered_map>
#include <map>
#include <string>

class Core;

class InstructionTracerLLC : public InstructionTracer
{
 public:
   struct InstInfo
   {
      uint64_t seq_id;
      IntPtr ip;
      String disassembly;
      String opcode;
      std::vector<uint64_t> producers;
      bool is_miss;
   };

   InstructionTracerLLC(const Core *core);
   virtual ~InstructionTracerLLC();

   virtual void traceInstruction(const DynamicMicroOp *uop, uop_times_t *times);

 private:
   const Core *m_core;
   uint32_t m_window_size;
   uint64_t m_current_seq_id;

   std::unordered_map<uint64_t, InstInfo> m_history;
   std::deque<uint64_t> m_seq_queue;

   // O(1) tracking structures
   std::unordered_map<uint32_t, uint64_t> last_reg_writer;
   std::unordered_map<IntPtr, uint64_t> last_mem_store;

   struct EdgeKey {
       std::string parent_opcode;
       std::string child_opcode;
       int64_t ip_dist;

       bool operator<(const EdgeKey& o) const {
           if (parent_opcode != o.parent_opcode) return parent_opcode < o.parent_opcode;
           if (child_opcode != o.child_opcode) return child_opcode < o.child_opcode;
           return ip_dist < o.ip_dist;
       }
   };

   std::map<EdgeKey, uint64_t> m_edge_histogram;

   void traverseGraph(uint64_t seq_id, int depth, const InstInfo& original_miss);
   void addEdge(const InstInfo& parent, const InstInfo& child);
   void dumpEdges();
};

#endif // __INSTRUCTION_TRACER_LLC_H
