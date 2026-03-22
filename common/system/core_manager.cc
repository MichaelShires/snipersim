#include <algorithm>
#include <limits.h>
#include <linux/unistd.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "cache.h"
#include "config.h"
#include "core.h"
#include "core_manager.h"
#include "network.h"

#include "log.h"

CoreManager::CoreManager(SimulationContext *context)
    : m_context(context), m_core_tls(TLS::create()), m_thread_type_tls(TLS::create()), 
      m_num_registered_sim_threads(0), m_num_registered_core_threads(0),
      m_config(context->getConfig()), m_dvfs_manager(context->getDvfsManager()),
      m_stats_manager(context->getStatsManager()), m_faultinjection_manager(context->getFaultinjectionManager()),
      m_hooks_manager(context->getHooksManager()), m_cfg(context->getConfigFile()), 
      m_thread_manager(NULL), m_trace_manager(NULL), m_simulator(context->getSimulator())
{
   LOG_PRINT("Starting CoreManager Constructor.");

   for (UInt32 i = 0; i < m_config->getTotalCores(); i++) {
      m_cores.push_back(new Core(i, m_context));
   }

   LOG_PRINT("Finished CoreManager Constructor.");
}

CoreManager::~CoreManager()
{
   for (std::vector<Core *>::iterator i = m_cores.begin(); i != m_cores.end(); i++)
      delete *i;

   delete m_core_tls;
   delete m_thread_type_tls;
}

void CoreManager::initializeCommId(UInt32 comm_id)
{
   LOG_PRINT("initializeCommId - current core (id) = %p (%d)", getCurrentCore(), getCurrentCoreID());

   core_id_t core_id = getCurrentCoreID();

   LOG_ASSERT_ERROR(core_id != INVALID_CORE_ID, "Unexpected invalid core id : %d", core_id);

   LOG_PRINT("Initializing comm_id: %d to core_id: %d", comm_id, core_id);

   // Broadcast this update to other processes

   m_config->updateCommToCoreMap(comm_id, core_id);

   LOG_PRINT("Finished.");
}

Core *CoreManager::getCurrentCore()
{
   return (Core *)m_core_tls->get();
}

core_id_t CoreManager::getCurrentCoreID()
{
   Core *core = getCurrentCore();
   return core ? core->getId() : INVALID_CORE_ID;
}

void CoreManager::initializeThread(core_id_t core_id)
{
   m_core_tls->set(m_cores.at(core_id));
   m_thread_type_tls->setInt(APP_THREAD);

   LOG_PRINT("Initialize thread for core %p (%d)", m_cores.at(core_id), m_cores.at(core_id)->getId());
   LOG_ASSERT_ERROR(m_core_tls->get() == (void *)(m_cores.at(core_id)), "TLS appears to be broken. %p != %p",
                    m_core_tls->get(), (void *)(m_cores.at(core_id)));
}

void CoreManager::terminateThread()
{
   LOG_ASSERT_WARNING(m_core_tls->get() != NULL, "Thread not initialized while terminating.");
   m_core_tls->set(NULL);
}

Core *CoreManager::getCoreFromID(core_id_t id)
{
   LOG_ASSERT_ERROR(id < (core_id_t)m_config->getTotalCores(), "Illegal index in getCoreFromID!");
   return m_cores.at(id);
}

core_id_t CoreManager::registerSimThread(ThreadType type)
{
   if (getCurrentCore() != NULL) {
      LOG_PRINT_ERROR("registerSimMemThread - Initialized thread twice");
      return getCurrentCore()->getId();
   }

   ScopedLock sl(m_num_registered_threads_lock);

   UInt32 *num_registered_threads = NULL;
   if (type == SIM_THREAD)
      num_registered_threads = &m_num_registered_sim_threads;
   else if (type == CORE_THREAD)
      num_registered_threads = &m_num_registered_core_threads;
   else
      LOG_ASSERT_ERROR(false, "Unknown thread type %d", type);

   LOG_ASSERT_ERROR(*num_registered_threads < m_config->getTotalCores(),
                    "All sim threads already registered. %d > %d", *num_registered_threads + 1,
                    m_config->getTotalCores());

   Core *core = m_cores.at(*num_registered_threads);

   m_core_tls->set(core);
   m_thread_type_tls->setInt(type);

   ++(*num_registered_threads);

   return core->getId();
}

bool CoreManager::amiSimThread()
{
   return m_thread_type_tls->getInt() == SIM_THREAD;
}

bool CoreManager::amiCoreThread()
{
   return m_thread_type_tls->getInt() == CORE_THREAD;
}

bool CoreManager::amiUserThread()
{
   return m_thread_type_tls->getInt() == APP_THREAD;
}
