#include "dvfs_manager.h"
#include "config.hpp"
#include "core.h"
#include "core_manager.h"
#include "instruction.h"
#include "log.h"
#include "performance_model.h"
#include "simulator.h"
#include "simulation_context.h"

#include <cassert>

DvfsManager::DvfsManager(SimulationContext *context)
    : m_context(context), m_cfg(context->getConfigFile()), m_config(context->getConfig()), m_core_manager(NULL)
{
   m_num_app_cores = m_config->getApplicationCores();
   m_cores_per_socket = m_cfg->getInt("dvfs/simple/cores_per_socket");
   m_transition_latency = SubsecondTime::NS() * m_cfg->getInt("dvfs/transition_latency");

   LOG_ASSERT_ERROR("simple" == m_cfg->getString("dvfs/type"),
                    "Currently, only this simple dvfs scheme is defined");

   // Initial configuration provides for socket-wide frequency control, with [dvfs/simple/cores_per_socket] cores per
   // socket
   m_num_proc_domains = m_num_app_cores / m_cores_per_socket;
   if (m_num_app_cores % m_cores_per_socket != 0) {
      // Round up if necessary
      m_num_proc_domains++;
   }

   app_proc_domains.resize(m_num_proc_domains, m_config->getCoreFrequency());
   global_domains.resize(DOMAIN_GLOBAL_MAX, m_config->getCoreFrequency());
}

UInt32 DvfsManager::getCoreDomainId(UInt32 core_id)
{
   assert(core_id < m_num_app_cores);
   return core_id / m_cores_per_socket;
}

const ComponentPeriod *DvfsManager::getCoreDomain(UInt32 core_id)
{
   return &app_proc_domains.at(getCoreDomainId(core_id));
}

const ComponentPeriod *DvfsManager::getGlobalDomain(DvfsGlobalDomain domain_id)
{
   return &global_domains.at(domain_id);
}

void DvfsManager::setCoreDomain(UInt32 core_id, ComponentPeriod new_freq)
{
   UInt32 domain_id = getCoreDomainId(core_id);
   app_proc_domains.at(domain_id) = new_freq;

   // Update performance model's frequency
   if (m_core_manager) {
      for (UInt32 i = 0; i < m_num_app_cores; i++) {
         if (getCoreDomainId(i) == domain_id) {
            Core *core = m_core_manager->getCoreFromID(i);
            if (core) {
               // Notify performance model
            }
         }
      }
   }
}
