#ifndef __INSTRUCTION_TRACER_LLC_H
#define __INSTRUCTION_TRACER_LLC_H

#include "fixed_types.h"
#include "instruction_tracer.h"
#include <deque>
#include <vector>

class Core;

class InstructionTracerLLC : public InstructionTracer
{
 public:
   struct MemAccess
   {
      IntPtr addr;
      bool is_load;
      bool is_store;
   };
   struct InstInfo
   {
      IntPtr ip;
      String disassembly;
      bool is_miss;
      // Store registers to help building the producer/consumer tree later
      std::vector<uint32_t> src_regs;
      std::vector<uint32_t> dst_regs;
      // Memory info
      std::vector<MemAccess> mem_accesses;
   };

   InstructionTracerLLC(const Core *core);
   virtual ~InstructionTracerLLC();

   virtual void traceInstruction(const DynamicMicroOp *uop, uop_times_t *times);

 private:
   const Core *m_core;
   uint32_t m_window_size;

   std::deque<InstInfo> m_history;

   void printTrace();
   void printTree(int index, int depth, uint32_t target_reg, IntPtr target_addr);
};

#endif // __INSTRUCTION_TRACER_LLC_H
