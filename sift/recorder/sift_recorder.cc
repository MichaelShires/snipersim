#include "log2.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <strings.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>
#include <vector>
// stat is not supported in Pin 3.0
// #include <sys/stat.h>
#include "intrabarrier_analysis.h"
#include "intrabarrier_mtng.h"
#include "pin.H"
#include <pthread.h>
#include <string.h>
#include <sys/syscall.h>

#if defined(SDE_INIT)
#include "sde-control.H"
#include "sde-init.H"
// #  include "sde-tracing.H"
#endif

#include "../../include/sim_api.h"
#include "emulation.h"
#include "globals.h"
#include "icountsniper.h"
#include "pinboost_debug.h"
#include "recorder_base.h"
#include "recorder_control.h"
#include "sift_assert.h"
#include "sift_writer.h"
#include "syscall_modeling.h"
#include "threads.h"
#include "trace_rtn.h"

using namespace CONTROLLER;

CONTROL_MANAGER *control_manager = NULL;
static CONTROLLER::CONTROL_MANAGER control("pinplay:");

VOID Fini(INT32 code, VOID *v)
{
   thread_data[0].output->Magic(SIM_CMD_ROI_END, 0, 0);
   for (unsigned int i = 0; i < max_num_threads; i++) {
      if (thread_data[i].output) {
         closeFile(i);
      }
   }
}
void initMtr()
{
   mtr_enabled = true;
   PinToolWarmup *warmup_tool = getWarmupTool();
   warmup_tool->activate();
}

VOID Detach(VOID *v)
{
}

BOOL followChild(CHILD_PROCESS childProcess, VOID *val)
{
   if (any_thread_in_detail) {
      fprintf(stderr, "EXECV ignored while in ROI\n");
      return false; // Cannot fork/execv after starting ROI
   }
   else
      return true;
}

VOID forkBefore(THREADID threadid, const CONTEXT *ctxt, VOID *v)
{
   if (thread_data[threadid].output) {
      child_app_id = thread_data[threadid].output->Fork();
   }
}

VOID forkAfterInChild(THREADID threadid, const CONTEXT *ctxt, VOID *v)
{
   // Forget about everything we inherited from the parent
   routines.clear();
   bzero(thread_data, max_num_threads * sizeof(*thread_data));
   // Assume identity of child process
   app_id = child_app_id;
   num_threads = 1;
   // Open new SIFT pipe for thread 0
   thread_data[0].bbv = new OnlineBbv(0);
   openFile(0);
}

bool assert_ignore()
{
   return false;
}

static VOID Handler(EVENT_TYPE ev, VOID *v, CONTEXT *ctxt, VOID *ip, THREADID tid, BOOL bcast)
{
   switch (ev) {
   case EVENT_START:
      if (KnobVerbose.Value())
         std::cerr << "[SIFT_RECORDER:" << app_id << ":" << tid << "] ROI Start" << std::endl;
      in_roi = true;
      setInstrumentationMode(Sift::ModeDetailed);
      if (tid == 0)
         thread_data[tid].output->Magic(SIM_CMD_ROI_START, 0, 0);
      break;
   case EVENT_STOP:
      if (KnobVerbose.Value())
         std::cerr << "[SIFT_RECORDER:" << app_id << ":" << tid << "] ROI Stop" << std::endl;
      in_roi = false;
      setInstrumentationMode(Sift::ModeIcount);
      if (tid == 0)
         thread_data[tid].output->Magic(SIM_CMD_ROI_END, 0, 0);
      break;
   default:
      break;
   }
}

int main(int argc, char **argv)
{
#if defined(SDE_INIT)
   sde_pin_init(argc, argv);
   sde_init();
#else
   if (PIN_Init(argc, argv)) {
      std::cerr << "Error, invalid parameters" << std::endl;
      std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;
      exit(1);
   }
#endif
   PIN_InitSymbols();

   if (KnobMaxThreads.Value() > 0) {
      max_num_threads = KnobMaxThreads.Value();
   }
   init_global_bbv();
   size_t thread_data_size = max_num_threads * sizeof(*thread_data);
   if (posix_memalign((void **)&thread_data, LINE_SIZE_BYTES, thread_data_size) != 0) {
      std::cerr << "Error, posix_memalign() failed" << std::endl;
      exit(1);
   }
   bzero(thread_data, thread_data_size);

   PIN_InitLock(&access_memory_lock);
   PIN_InitLock(&new_threadid_lock);

   app_id = KnobSiftAppId.Value();
   blocksize = KnobBlocksize.Value();
   fast_forward_target = KnobFastForwardTarget.Value();
   detailed_target = KnobDetailedTarget.Value();

   if (KnobEmulateSyscalls.Value() || (!KnobUseROI.Value() && !KnobMPIImplicitROI.Value())) {
      if (app_id < 0)
         findMyAppId();
   }
   if (fast_forward_target == 0 && !KnobUseROI.Value() && !KnobMPIImplicitROI.Value()) {
      // Start in detailed if there is no fast-forwarding or warming requested
      in_roi = true;
      setInstrumentationMode(Sift::ModeDetailed);
      openFile(0);
   }
   else if (KnobEmulateSyscalls.Value()) {
      openFile(0);
   }

   if (thread_data[0].output)
      thread_data[0].output->Magic(SIM_CMD_ROI_START, 0, 0);
   // When attaching with --pid, there could be a number of threads already running.
   // Manually call NewThread() because the normal method to start new thread pipes (SYS_clone)
   // will already have happened
   if (PIN_GetInitialThreadCount() > 1) {
      sift_assert(thread_data[PIN_ThreadId()].output);
      for (UINT32 i = 1; i < PIN_GetInitialThreadCount(); i++) {
         thread_data[PIN_ThreadId()].output->NewThread();
      }
   }

#ifdef PINPLAY
   if (KnobReplayer.Value()) {
      if (KnobEmulateSyscalls.Value()) {
         std::cerr << "Error, emulating syscalls is not compatible with PinPlay replaying." << std::endl;
         exit(1);
      }

#if defined(SDE_INIT)
      // This is a replay-only tool (for now)
      // p_pinplay_engine = sde_tracing_get_pinplay_engine();
      p_pinplay_engine = &pp_pinplay_engine;
      p_pinplay_engine->Activate(argc, argv, false /*logger*/, KnobReplayer.Value());
#else
      p_pinplay_engine = &pp_pinplay_engine;
      p_pinplay_engine->Activate(argc, argv, false /*logger*/, KnobReplayer.Value());
#endif
   }
#else
   if (KnobReplayer.Value()) {
      std::cerr << "Error, PinPlay support not compiled in. Please use a compatible pin kit when compiling."
                << std::endl;
      exit(1);
   }
#endif

#if defined(SDE_INIT)
   control_manager = SDE_CONTROLLER::sde_controller_get();
#else
   control.Activate();
   control_manager = &control;
#endif
   if (control_manager && KnobVerbose.Value())
      control_manager->RegisterHandler(Handler, 0, FALSE);

   icount.Activate();
   if (KnobEmulateSyscalls.Value()) {
      if (!KnobUseResponseFiles.Value()) {
         std::cerr << "Error, Response files are required when using syscall emulation." << std::endl;
         exit(1);
      }

      initSyscallModeling();
   }

   initThreads();
   initRecorderControl();
   initRecorderBase();
   initEmulation();

   if (KnobRoutineTracing.Value())
      initRoutineTracer();

   PIN_AddFiniFunction(Fini, 0);
   PIN_AddDetachFunction(Detach, 0);

   PIN_AddFollowChildProcessFunction(followChild, 0);
   if (KnobEmulateSyscalls.Value()) {
      PIN_AddForkFunction(FPOINT_BEFORE, forkBefore, 0);
      PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, forkAfterInChild, 0);
   }

   pinboost_register("SIFT_RECORDER", KnobDebug.Value());

   int64_t pacsim_version = KnobPacSimEnable.Value();
   mtr_enabled = false;
   if (pacsim_version) {
      std::cout << "[PacSim]: Pacsim is Enabled\n";
      intrabarrier_mtng::activate(false);
      // initMtr();
   }

   PIN_StartProgram();

   return 0;
}
