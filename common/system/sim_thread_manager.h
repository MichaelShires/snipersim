#ifndef SIM_THREAD_MANAGER_H
#define SIM_THREAD_MANAGER_H

#include "core_thread.h"
#include "sim_thread.h"
#include "simulation_context.h"

class Config;
class Transport;

class SimThreadManager
{
 public:
   SimThreadManager(SimulationContext *context);
   ~SimThreadManager();

   void spawnSimThreads();
   void quitSimThreads();

   void simThreadStartCallback();
   void simThreadExitCallback();

 private:
   SimulationContext *m_context;
   SimThread *m_sim_threads;
   CoreThread *m_core_threads;

   Lock m_active_threads_lock;
   UInt32 m_active_threads;

   Config *m_config;
   Transport *m_transport;
};

#endif // SIM_THREAD_MANAGER
