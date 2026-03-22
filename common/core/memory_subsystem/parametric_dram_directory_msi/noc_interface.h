#ifndef NOC_INTERFACE_H
#define NOC_INTERFACE_H

#include "fixed_types.h"
#include "mem_component.h"

namespace ParametricDramDirectoryMSI
{

class MemoryManager;

class NoCInterface
{
 private:
   MemoryManager *m_memory_manager;
   core_id_t m_core_id;

 public:
   NoCInterface(MemoryManager *mm, core_id_t core_id) : m_memory_manager(mm), m_core_id(core_id)
   {
   }

   // Methods to interact with other components via the NoC
   // This is a placeholder for actual message passing logic
};

} // namespace ParametricDramDirectoryMSI

#endif
