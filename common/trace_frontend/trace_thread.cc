#include "trace_thread.h"
#include "branch_predictor.h"
#include "config.hpp"
#include "core.h"
#include "core_manager.h"
#include "dvfs_manager.h"
#include "dynamic_instruction.h"
#include "instruction.h"
#include "instruction_decoder_wlib.h"
#include "magic_client.h"
#include "performance_model.h"
#include "rng.h"
#include "routine_tracer.h"
#include "sim_api.h"
#include "simulator.h"
#include "syscall_model.h"
#include "thread.h"
#include "thread_manager.h"
#include "trace_manager.h"
#include "sampling_manager.h"
#include "fastforward_performance_manager.h"
#include "simulation_context.h"

#include "stats.h"

#include <sys/syscall.h>
#include <unistd.h>

#include <x86_decoder.h> // TODO remove when the decode function in microop perf model is adapted

int TraceThread::m_isa = 0;

TraceThread::TraceThread(TraceManager *manager, SimulationContext *context, app_id_t app_id, uint32_t thread_num, String tracefile, String responsefile)
    : m__thread(NULL), m_thread(context->getThreadManager()->getThreadFromID(thread_num)), m_time_start(SubsecondTime::Zero()),
      m_trace(tracefile.c_str(), responsefile.c_str(), thread_num), m_trace_has_pa(false),
      m_address_randomization(context->getConfigFile()->getBool("traceinput/address_randomization")),
      m_appid_from_coreid(context->getConfigFile()->getString("scheduler/type") == "sequential" ? true : false), m_stop(false),
      m_bbv_base(0), m_bbv_count(0), m_bbv_last(0), m_bbv_end(false), m_output_leftover_size(0), m_tracefile(tracefile),
      m_responsefile(responsefile), m_app_id(app_id), m_blocked(false), m_cleanup(false), m_started(false),
      m_stopped(false), m_context(context), m_manager(manager)
{

   m_trace.setHandleInstructionCountFunc(TraceThread::__handleInstructionCountFunc, this);
   m_trace.setHandleCacheOnlyFunc(TraceThread::__handleCacheOnlyFunc, this);
   if (m_context->getConfigFile()->getBool("traceinput/mirror_output"))
      m_trace.setHandleOutputFunc(TraceThread::__handleOutputFunc, this);
   m_trace.setHandleSyscallFunc(TraceThread::__handleSyscallFunc, this);
   m_trace.setHandleNewThreadFunc(TraceThread::__handleNewThreadFunc, this);
   m_trace.setHandleJoinFunc(TraceThread::__handleJoinFunc, this);
   m_trace.setHandleMagicFunc(TraceThread::__handleMagicFunc, this);
   m_trace.setHandleEmuFunc(TraceThread::__handleEmuFunc, this);
   m_trace.setHandleForkFunc(TraceThread::__handleForkFunc, this);
   if (m_context->getRoutineTracer())
      m_trace.setHandleRoutineFunc(TraceThread::__handleRoutineChangeFunc, TraceThread::__handleRoutineAnnounceFunc,
                                   this);

   if (m_address_randomization) {
      // Fisher-Yates shuffle, simultaneously initializing array to m_address_randomization_table[i] = i
      // See http://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#The_.22inside-out.22_algorithm
      // By using the app_id as a random seed, we get an app_id-specific pseudo-random permutation of 0..255
      UInt64 state = rng_seed(app_id);
      m_address_randomization_table[0] = 0;
      for (unsigned int i = 1; i < 256; ++i) {
         uint8_t j = rng_next(state) % (i + 1);
         m_address_randomization_table[i] = m_address_randomization_table[j];
         m_address_randomization_table[j] = i;
      }
   }

   m_thread->setVa2paFunc(_va2pa, (UInt64)this);
}

TraceThread::~TraceThread()
{
   delete m__thread;
   if (m_cleanup) {
      unlink(m_tracefile.c_str());
      unlink(m_responsefile.c_str());
   }
   for (std::unordered_map<IntPtr, const dl::DecodedInst *>::iterator i = m_decoder_cache.begin();
        i != m_decoder_cache.end(); ++i)
   {
      delete (*i).second;
   }
}

void TraceThread::spawn()
{
   m__thread = _Thread::create(this);
   m__thread->run();
}

void TraceThread::run()
{
   m_context->getCoreManager()->initializeThread(m_thread->getId());
   m_context->getThreadManager()->onThreadStart(m_thread->getId(), m_time_start);

   while (!m_stop) {
      Sift::Instruction inst;
      if (!m_trace.Read(inst))
         break;
   }

   m_context->getThreadManager()->onThreadExit(m_thread->getId());
   m_context->getCoreManager()->terminateThread();
   m_manager->signalDone(this, getCurrentTime(), m_stop);
}

Sift::Mode TraceThread::handleInstructionCountFunc(uint32_t icount)
{
   // Stub
   return Sift::ModeUnknown;
}

void TraceThread::handleCacheOnlyFunc(uint8_t icount, Sift::CacheOnlyType type, uint64_t eip, uint64_t address)
{
   // Stub
}

void TraceThread::handleOutputFunc(uint8_t fd, const uint8_t *data, uint32_t size)
{
   // Stub
}

uint64_t TraceThread::handleSyscallFunc(uint16_t syscall_number, const uint8_t *data, uint32_t size)
{
   // Stub
   return 0;
}

int32_t TraceThread::handleNewThreadFunc()
{
   // Stub
   return 0;
}

int32_t TraceThread::handleForkFunc()
{
   // Stub
   return 0;
}

int32_t TraceThread::handleJoinFunc(int32_t thread)
{
   // Stub
   return 0;
}

uint64_t TraceThread::handleMagicFunc(uint64_t a, uint64_t b, uint64_t c)
{
   // Stub
   return 0;
}

bool TraceThread::handleEmuFunc(Sift::EmuType type, Sift::EmuRequest &req, Sift::EmuReply &res)
{
   // Stub
   return false;
}

void TraceThread::handleRoutineChangeFunc(Sift::RoutineOpType event, uint64_t eip, uint64_t esp, uint64_t callEip)
{
   // Stub
}

void TraceThread::handleRoutineAnnounceFunc(uint64_t eip, const char *name, const char *imgname, uint64_t offset, uint32_t line,
                                  uint32_t column, const char *filename)
{
   // Stub
}

SubsecondTime TraceThread::getCurrentTime() const
{
   return SubsecondTime::Zero();
}

void TraceThread::frontEndStop()
{
   // Stub
}

UInt64 TraceThread::getProgressExpect()
{
   return 0;
}

UInt64 TraceThread::getProgressValue()
{
   return 0;
}

void TraceThread::handleAccessMemory(Core::lock_signal_t lock_signal, Core::mem_op_t mem_op_type, IntPtr d_addr,
                           char *data_buffer, UInt32 data_size)
{
   // Stub
}

uint64_t TraceThread::va2pa(uint64_t va, bool *noMapping)
{
   return va;
}
