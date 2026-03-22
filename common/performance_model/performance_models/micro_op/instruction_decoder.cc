#include "instruction_decoder.h"
#include "instruction.h"
#include "micro_op.h"
#include <string.h>

extern "C" {
#include <xed-interface.h>
#include <xed-reg-class.h>
#if PIN_REV >= 62732
#include <xed-decoded-inst-api.h>
#endif
}

static bool is_ccmp_ctest(xed_iclass_enum_t iclass)
{
   const char *name = xed_iclass_enum_t2str(iclass);
   return (strncmp(name, "CCMP", 4) == 0 || strncmp(name, "CTEST", 5) == 0);
}

static bool is_push2_pop2(xed_iclass_enum_t iclass)
{
   const char *name = xed_iclass_enum_t2str(iclass);
   return (strncmp(name, "PUSH2", 5) == 0 || strncmp(name, "POP2", 4) == 0);
}

void InstructionDecoder::addSrcs(std::set<xed_reg_enum_t> regs, MicroOp *currentMicroOp)
{
   for (std::set<xed_reg_enum_t>::iterator it = regs.begin(); it != regs.end(); ++it)
      if (*it != XED_REG_INVALID) {
         xed_reg_enum_t reg = xed_get_largest_enclosing_register(*it);
         if (reg == XED_REG_EIP || reg == XED_REG_RIP)
            continue; // eip/rip is known at decode time, shouldn't be a dependency
         currentMicroOp->addSourceRegister(reg, String(xed_reg_enum_t2str(reg)));
      }
}

void InstructionDecoder::addAddrs(std::set<xed_reg_enum_t> regs, MicroOp *currentMicroOp)
{
   for (std::set<xed_reg_enum_t>::iterator it = regs.begin(); it != regs.end(); ++it)
      if (*it != XED_REG_INVALID) {
         xed_reg_enum_t reg = xed_get_largest_enclosing_register(*it);
         if (reg == XED_REG_EIP || reg == XED_REG_RIP)
            continue; // eip/rip is known at decode time, shouldn't be a dependency
         currentMicroOp->addAddressRegister(reg, String(xed_reg_enum_t2str(reg)));
      }
}

void InstructionDecoder::addDsts(std::set<xed_reg_enum_t> regs, MicroOp *currentMicroOp)
{
   for (std::set<xed_reg_enum_t>::iterator it = regs.begin(); it != regs.end(); ++it)
      if (*it != XED_REG_INVALID) {
         xed_reg_enum_t reg = xed_get_largest_enclosing_register(*it);
         if (reg == XED_REG_EIP || reg == XED_REG_RIP)
            continue; // eip/rip is known at decode time, shouldn't be a dependency
         currentMicroOp->addDestinationRegister(reg, String(xed_reg_enum_t2str(reg)));
      }
}

unsigned int InstructionDecoder::getNumExecs(const xed_decoded_inst_t *ins, int numLoads, int numStores)
{
   xed_iclass_enum_t iclass = xed_decoded_inst_get_iclass(ins);

   // APX: CCMP and CTEST are basically branchless comparisons
   if (is_ccmp_ctest(iclass)) {
      return 1;
   }

   // AVX-10 Support: Expand decoder to identify AVX-10 Converged ISA features
   const char *ext_name = xed_extension_enum_t2str(xed_decoded_inst_get_extension(ins));
   if (ext_name && (strncmp(ext_name, "AVX10", 5) == 0)) {
      // AVX-10 specific latency/throughput modeling
      // Placeholder for converged 128/256/512 bit dispatch
      return 1; // Base throughput for AVX-10
   }

   if (xed_decoded_inst_get_category(ins) == XED_CATEGORY_DATAXFER ||
       xed_decoded_inst_get_category(ins) == XED_CATEGORY_CMOV || iclass == XED_ICLASS_PUSH ||
       iclass == XED_ICLASS_POP || is_push2_pop2(iclass))
   {
      unsigned int numExecs = 0;

      // Move instructions with additional microops to process the load and store information
      switch (iclass) {
      case XED_ICLASS_MOVLPS:
      case XED_ICLASS_MOVLPD:
      case XED_ICLASS_MOVHPS:
      case XED_ICLASS_MOVHPD:
      case XED_ICLASS_SHUFPS:
      case XED_ICLASS_SHUFPD:
      case XED_ICLASS_BLENDPS:
      case XED_ICLASS_BLENDPD:
      case XED_ICLASS_EXTRACTPS:
      case XED_ICLASS_ROUNDSS:
      case XED_ICLASS_ROUNDPS:
      case XED_ICLASS_ROUNDSD:
      case XED_ICLASS_ROUNDPD:
         numExecs += 1;
         break;
      case XED_ICLASS_INSERTPS:
         numExecs += 2;
         break;
      default:
         break;
      }

      // Explicit register moves. Normal loads and stores do not require this.
      if ((numLoads + numStores) == 0) {
         numExecs += 1;
      }

      return numExecs;
   }
   else {
      return 1;
   }
}

//////////////////////////////////////////
///// IMPLEMENTATION OF INSTRUCTIONS /////
//////////////////////////////////////////

const std::vector<const MicroOp *> *InstructionDecoder::decode(IntPtr address, const xed_decoded_inst_t *ins,
                                                               Instruction *ins_ptr)
{
   xed_iclass_enum_t iclass = xed_decoded_inst_get_iclass(ins);

   // Determine register dependencies and number of microops per type

   std::vector<std::set<xed_reg_enum_t>> regs_loads, regs_stores;
   std::set<xed_reg_enum_t> regs_mem, regs_src, regs_dst;
   std::vector<uint16_t> memop_load_size, memop_store_size;

   int numLoads = 0;
   int numExecs = 0;
   int numStores = 0;

   // Ignore memory-referencing operands in NOP instructions
   if (!xed_decoded_inst_get_attribute(ins, XED_ATTRIBUTE_NOP)) {
      for (uint32_t mem_idx = 0; mem_idx < xed_decoded_inst_number_of_memory_operands(ins); ++mem_idx) {
         std::set<xed_reg_enum_t> regs;
         regs.insert(xed_decoded_inst_get_base_reg(ins, mem_idx));
         regs.insert(xed_decoded_inst_get_index_reg(ins, mem_idx));

         if (xed_decoded_inst_mem_read(ins, mem_idx)) {
            regs_loads.push_back(regs);
            memop_load_size.push_back(xed_decoded_inst_get_memory_operand_length(ins, mem_idx));
            numLoads++;
         }

         if (xed_decoded_inst_mem_written(ins, mem_idx)) {
            regs_stores.push_back(regs);
            memop_store_size.push_back(xed_decoded_inst_get_memory_operand_length(ins, mem_idx));
            numStores++;
         }

         regs_mem.insert(regs.begin(), regs.end());
      }

      // APX PUSH2 and POP2: XED might report them as 1 memory operand, but they are 2
      const char *name = xed_iclass_enum_t2str(iclass);
      if (strncmp(name, "PUSH2", 5) == 0) {
         if (numStores == 1) {
            regs_stores.push_back(regs_stores[0]);
            memop_store_size.push_back(memop_store_size[0]);
            numStores++;
         }
      }
      if (strncmp(name, "POP2", 4) == 0) {
         if (numLoads == 1) {
            regs_loads.push_back(regs_loads[0]);
            memop_load_size.push_back(memop_load_size[0]);
            numLoads++;
         }
      }
   }

   bool is_atomic = false;
   const xed_operand_values_t *ops = xed_decoded_inst_operands_const(ins);
   if (xed_operand_values_get_atomic(ops))
      is_atomic = true;

   const xed_inst_t *inst = xed_decoded_inst_inst(ins);
   for (uint32_t idx = 0; idx < xed_inst_noperands(inst); ++idx) {
      const xed_operand_t *op = xed_inst_operand(inst, idx);
      xed_operand_enum_t name = xed_operand_name(op);

      if (name == XED_OPERAND_AGEN) {
         /* LEA instruction */
         regs_src.insert(regs_mem.begin(), regs_mem.end());
      }
      else if (xed_operand_is_register(name)) {
         xed_reg_enum_t reg = xed_decoded_inst_get_reg(ins, name);

         if (xed_operand_read(op) && regs_mem.count(reg) == 0)
            regs_src.insert(reg);
         if (xed_operand_written(op))
            regs_dst.insert(reg);
      }
   }

   // Explicitly check for RFLAGS dependencies (crucial for APX NF/CCMP)
   if (xed_decoded_inst_uses_rflags(ins)) {
      const xed_simple_flag_t *si = xed_decoded_inst_get_rflags_info(ins);
      if (si) {
         if (xed_simple_flag_reads_flags(si))
            regs_src.insert(XED_REG_RFLAGS);
         if (xed_simple_flag_writes_flags(si))
            regs_dst.insert(XED_REG_RFLAGS);
      }
   }

// APX specific: Check for NF (No Flags) prefix
#if defined(XED_OPERAND_NF)
   if (xed_decoded_inst_get_operand_present(ins, XED_OPERAND_NF)) {
      // If NF is present, flags are NOT written
      regs_dst.erase(XED_REG_RFLAGS);
   }
#endif

   numExecs = getNumExecs(ins, numLoads, numStores);

   // Determine some extra instruction characteristics that will affect timing

   // Determine instruction operand width
   uint16_t operand_size = 0;
   for (uint32_t idx = 0; idx < xed_inst_noperands(inst); ++idx) {
      const xed_operand_t *op = xed_inst_operand(inst, idx);
      xed_operand_enum_t name = xed_operand_name(op);

      if (xed_operand_is_register(name)) {
         xed_reg_enum_t reg = xed_decoded_inst_get_reg(ins, name);
         switch (reg) {
         case XED_REG_RFLAGS:
         case XED_REG_RIP:
         case XED_REG_RSP:
            continue;
         default:;
         }
      }
      operand_size = std::max(operand_size, (uint16_t)xed_decoded_inst_get_operand_width(ins));
   }
   if (operand_size == 0)
      operand_size = 64;

   bool is_serializing = false;
   switch (iclass) {
   // TODO: There may be more (newer) instructions, but they are all kernel only
   case XED_ICLASS_JMP_FAR:
   case XED_ICLASS_CALL_FAR:
   case XED_ICLASS_RET_FAR:
   case XED_ICLASS_IRET:
   case XED_ICLASS_CPUID:
   case XED_ICLASS_LGDT:
   case XED_ICLASS_LIDT:
   case XED_ICLASS_LLDT:
   case XED_ICLASS_LTR:
   case XED_ICLASS_LMSW:
   case XED_ICLASS_WBINVD:
   case XED_ICLASS_INVD:
   case XED_ICLASS_INVLPG:
   case XED_ICLASS_RSM:
   case XED_ICLASS_WRMSR:
   case XED_ICLASS_SYSENTER:
   case XED_ICLASS_SYSRET:
      is_serializing = true;
      break;
   default:
      is_serializing = false;
      break;
   }

   // Generate list of microops

   std::vector<const MicroOp *> *uops = new std::vector<const MicroOp *>(); //< Return value
   int totalMicroOps = numLoads + numExecs + numStores;

   for (int index = 0; index < totalMicroOps; ++index) {

      MicroOp *currentMicroOp = new MicroOp();
      currentMicroOp->setInstructionPointer(Memory::make_access(address));

      // We don't necessarily know the address at this point as it could
      // be dependent on register values.  Therefore, fill it in at simulation time.

      if (index < numLoads) /* LOAD */
      {
         size_t loadIndex = index;
         currentMicroOp->makeLoad(loadIndex, iclass, xed_iclass_enum_t2str(iclass), memop_load_size[loadIndex]);
      }
      else if (index < numLoads + numExecs) /* EXEC */
      {
         size_t execIndex = index - numLoads;
         LOG_ASSERT_ERROR(numExecs <= 1, "More than 1 exec uop");

         bool is_cond_branch = (xed_decoded_inst_get_category(ins) == XED_CATEGORY_COND_BR);
         // APX CCMP/CTEST are NOT branches
         if (is_ccmp_ctest(iclass))
            is_cond_branch = false;

         currentMicroOp->makeExecute(execIndex, numLoads, iclass, xed_iclass_enum_t2str(iclass), is_cond_branch);
      }
      else /* STORE */
      {
         size_t storeIndex = index - numLoads - numExecs;
         currentMicroOp->makeStore(storeIndex, numExecs, iclass, xed_iclass_enum_t2str(iclass),
                                   memop_store_size[storeIndex]);
         if (is_atomic)
            currentMicroOp->setMemBarrier(true);
      }

      // Fill in the destination registers for both loads and executes, also on stores if there are no loads or executes

      if (index < numLoads) /* LOAD */
      {
         size_t loadIndex = index;
         addSrcs(regs_loads[loadIndex], currentMicroOp);
         addAddrs(regs_loads[loadIndex], currentMicroOp);

         if (numExecs == 0) {
            // No execute microop: we inherit its read operands
            addSrcs(regs_src, currentMicroOp);
            if (numStores == 0)
               // No store microop either: we also inherit its write operands
               addDsts(regs_dst, currentMicroOp);
         }
      }

      else if (index < numLoads + numExecs) /* EXEC */
      {
         addSrcs(regs_src, currentMicroOp);
         addDsts(regs_dst, currentMicroOp);

         if (iclass == XED_ICLASS_MFENCE || iclass == XED_ICLASS_LFENCE || iclass == XED_ICLASS_SFENCE)
            currentMicroOp->setMemBarrier(true);

         // Special cases
         if ((iclass == XED_ICLASS_MOVHPD) || (iclass == XED_ICLASS_MOVHPS) || (iclass == XED_ICLASS_MOVLPD) ||
             (iclass == XED_ICLASS_MOVLPS) || (iclass == XED_ICLASS_MOVSD_XMM) // EXEC exists only for reg-to-reg moves
             || (iclass == XED_ICLASS_MOVSS))
         {
            // In this case, we have a memory to XMM load, where the result merges the source and destination
            addSrcs(regs_dst, currentMicroOp);
         }
      }

      else /* STORE */
      {
         size_t storeIndex = index - numLoads - numExecs;
         addSrcs(regs_stores[storeIndex], currentMicroOp);
         addAddrs(regs_stores[storeIndex], currentMicroOp);

         if (numExecs == 0) {
            // No execute microop: we inherit its write operands
            addDsts(regs_dst, currentMicroOp);
            if (numLoads == 0)
               // No load microops either: we also inherit its read operands
               addSrcs(regs_src, currentMicroOp);
         }
         if (is_atomic)
            currentMicroOp->setMemBarrier(true);
      }

      /* Extra information on all micro ops */

      currentMicroOp->setOperandSize(operand_size);
      currentMicroOp->setInstruction(ins_ptr);

      /* Extra information of first micro op */

      if (index == 0) {
         currentMicroOp->setFirst(true);

         // Use of x87 FPU?
         if (toupper(xed_iclass_enum_t2str(iclass)[0]) == 'F')
            currentMicroOp->setIsX87(true);
      }

      /* Extra information on last micro op */

      if (index == totalMicroOps - 1) {
         currentMicroOp->setLast(true);

         // Check if the instruction is serializing, place the serializing flag
         if (is_serializing)
            currentMicroOp->setSerializing(true);
      }

      uops->push_back(currentMicroOp);
   }

   return uops;
}
