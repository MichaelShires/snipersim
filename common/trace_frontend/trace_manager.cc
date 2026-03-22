#include "trace_manager.h"
#include "config.hpp"
#include "hooks_manager.h"
#include "sim_api.h"
#include "simulator.h"
#include "stats.h"
#include "thread_manager.h"
#include "trace_thread.h"
#include "simulation_context.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

TraceManager::TraceManager(SimulationContext *context)
    : m_monitor(new Monitor(this, context->getConfigFile()->getInt("traceinput/timeout"))), m_threads(0),
      m_num_threads_started(0), m_num_threads_running(0), m_done(0),
      m_stop_with_first_app(context->getConfigFile()->getBool("traceinput/stop_with_first_app")),
      m_app_restart(context->getConfigFile()->getBool("traceinput/restart_apps")),
      m_emulate_syscalls(context->getConfigFile()->getBool("traceinput/emulate_syscalls")),
      m_num_apps(context->getConfigFile()->getInt("traceinput/num_apps")), m_num_apps_nonfinish(m_num_apps),
      m_app_info(m_num_apps), m_tracefiles(m_num_apps), m_responsefiles(m_num_apps), m_context(context)
{
   setupTraceFiles(0);
}

TraceManager::~TraceManager()
{
   delete m_monitor;
   for (std::vector<TraceThread *>::iterator it = m_threads.begin(); it != m_threads.end(); ++it)
      delete *it;
}

void TraceManager::setupTraceFiles(int index)
{
   m_trace_prefix = m_context->getConfigFile()->getStringArray("traceinput/trace_prefix", index);

   if (m_emulate_syscalls) {
      if (m_trace_prefix == "") {
         std::cerr << "Error: a trace prefix is required when emulating syscalls." << std::endl;
         exit(1);
      }
   }

   if (m_trace_prefix != "") {
      for (UInt32 i = 0; i < m_num_apps; i++) {
         m_tracefiles[i] = getFifoName(i, 0, false /*response*/, false /*create*/);
         m_responsefiles[i] = getFifoName(i, 0, true /*response*/, false /*create*/);
      }
   }
   else {
      for (UInt32 i = 0; i < m_num_apps; i++) {
         m_tracefiles[i] = m_context->getConfigFile()->getStringArray("traceinput/thread_" + itostr(i), index);
      }
   }
}

void TraceManager::init()
{
   for (UInt32 i = 0; i < m_num_apps; i++) {
      newThread(i /*app_id*/, true /*first*/, false /*init_fifo*/, false /*spawn*/, SubsecondTime::Zero(),
                INVALID_THREAD_ID);
   }
}

void TraceManager::start()
{
   m_monitor->spawn();
}

void TraceManager::stop()
{
   m_done.signal();
}

void TraceManager::mark_done()
{
   m_done.signal();
}

void TraceManager::wait()
{
   m_done.wait();
}

void TraceManager::run()
{
   start();
   wait();
}

void TraceManager::cleanup()
{
   ScopedLock sl(m_lock);
   for (std::vector<TraceThread *>::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
      (*it)->stop();
      delete *it;
   }
   m_threads.clear();
   m_num_threads_started = 0;
   m_num_threads_running = 0;
   m_num_apps_nonfinish = m_num_apps;
}

String TraceManager::getFifoName(app_id_t app_id, UInt64 thread_num, bool response, bool create)
{
   String filename =
       m_trace_prefix + (response ? "_response" : "") + ".app" + itostr(app_id) + ".th" + itostr(thread_num) + ".sift";
   if (create)
      mkfifo(filename.c_str(), 0600);
   return filename;
}

thread_id_t TraceManager::createThread(app_id_t app_id, SubsecondTime time, thread_id_t creator_thread_id)
{
   // External version: acquire lock first
   ScopedLock sl(m_lock);

   return newThread(app_id, false /*first*/, true /*init_fifo*/, true /*spawn*/, time, creator_thread_id);
}

thread_id_t TraceManager::newThread(app_id_t app_id, bool first, bool init_fifo, bool spawn, SubsecondTime time,
                                    thread_id_t creator_thread_id)
{
   // Internal version: assume we're already holding the lock

   assert(static_cast<decltype(app_id)>(m_num_apps) > app_id);

   String tracefile = "", responsefile = "";
   int thread_num;
   if (first) {
      m_app_info[app_id].num_threads = 1;
      m_app_info[app_id].thread_count = 1;
      m_context->getHooksManager()->callHooks(HookType::HOOK_APPLICATION_START, (UInt64)app_id);
      m_context->getStatsManager()->logEvent(StatsManager::EVENT_APP_START, SubsecondTime::MaxTime(), INVALID_CORE_ID,
                                         INVALID_THREAD_ID, (UInt64)app_id, 0, "");
      thread_num = 0;

      if (!init_fifo) {
         tracefile = m_tracefiles[app_id];
         if (m_responsefiles.size())
            responsefile = m_responsefiles[app_id];
      }
   }
   else {
      m_app_info[app_id].num_threads++;
      thread_num = m_app_info[app_id].thread_count++;

      if (init_fifo) {
         tracefile = getFifoName(app_id, thread_num, false /*response*/, true /*create*/);
         responsefile = getFifoName(app_id, thread_num, true /*response*/, true /*create*/);
      }
   }

   TraceThread *new_thread = new TraceThread(this, m_context, app_id, thread_num, tracefile, responsefile);
   m_threads.push_back(new_thread);
   m_num_threads_started++;
   m_num_threads_running++;

   if (spawn)
      new_thread->spawn();

   HooksManager::ThreadCreate args = {(thread_id_t)m_threads.size() - 1, app_id};
   m_context->getHooksManager()->callHooks(HookType::HOOK_THREAD_CREATE, (UInt64) &args);

   return (thread_id_t)m_threads.size() - 1;
}

app_id_t TraceManager::createApplication(SubsecondTime time, thread_id_t creator_thread_id)
{
   ScopedLock sl(m_lock);

   app_id_t app_id = m_num_apps++;
   m_app_info.push_back(app_info_t());
   m_num_apps_nonfinish++;

   newThread(app_id, true /*first*/, true /*init_fifo*/, true /*spawn*/, time, creator_thread_id);

   return app_id;
}

void TraceManager::signalStarted()
{
}

void TraceManager::signalDone(TraceThread *thread, SubsecondTime time, bool aborted)
{
   ScopedLock sl(m_lock);

   HooksManager::ThreadTime args = {thread->getThread()->getId(), time};
   m_context->getHooksManager()->callHooks(HookType::HOOK_THREAD_EXIT, (UInt64) &args);

   app_id_t app_id = thread->getAppId();
   m_app_info[app_id].num_threads--;
   m_num_threads_running--;

   if (m_app_info[app_id].num_threads == 0) {
      endApplication(thread, time);
   }

   // If the whole simulation is done, stop the monitor thread
   if (m_num_threads_running == 0 && (m_stop_with_first_app || m_num_apps_nonfinish == 0)) {
      m_context->getSimulator()->stop();
   }
}

void TraceManager::endApplication(TraceThread *thread, SubsecondTime time)
{
   // Internal version: assume we're already holding the lock

   app_id_t app_id = thread->getAppId();
   m_app_info[app_id].num_runs++;

   m_context->getHooksManager()->callHooks(HookType::HOOK_APPLICATION_EXIT, (UInt64)app_id);
   m_context->getStatsManager()->logEvent(StatsManager::EVENT_APP_EXIT, SubsecondTime::MaxTime(), INVALID_CORE_ID,
                                      INVALID_THREAD_ID, (UInt64)app_id, 0, "");

   if (m_app_info[app_id].num_runs == 1) {
      m_num_apps_nonfinish--;
   }

   if (m_app_restart) {
      // Create new initial thread for this application
      newThread(app_id, true /*first*/, false /*init_fifo*/, true /*spawn*/, time, INVALID_THREAD_ID);
   }
}

void TraceManager::endFrontEnd()
{
   ScopedLock sl(m_lock);
   for (std::vector<TraceThread *>::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
      (*it)->frontEndStop();
   }
}

void TraceManager::accessMemory(int core_id, Core::lock_signal_t lock_signal, Core::mem_op_t mem_op_type, IntPtr d_addr,
                                char *data_buffer, UInt32 data_size)
{
   // Stub: handle memory access from the frontend
}

UInt64 TraceManager::getProgressExpect()
{
   return 0;
}

UInt64 TraceManager::getProgressValue()
{
   return 0;
}

TraceManager::Monitor::Monitor(TraceManager *manager, UInt32 timeout)
    : m_thread(NULL), m_manager(manager), m_timeout(timeout)
{
}

TraceManager::Monitor::~Monitor()
{
   delete m_thread;
}

void TraceManager::Monitor::spawn()
{
   m_thread = _Thread::create(this);
   m_thread->run();
}

void TraceManager::Monitor::run()
{
   while (true) {
      sleep(m_timeout);
      // Check for timeout
   }
}
