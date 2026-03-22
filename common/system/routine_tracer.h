#ifndef __ROUTINE_TRACER_H
#define __ROUTINE_TRACER_H

#include "fixed_types.h"
#include "subsecond_time.h"
#include "lock.h"
#include "simulation_context.h"

#include <boost/functional/hash.hpp>
#include <deque>
#include <vector>

// From http://stackoverflow.com/questions/8027368/are-there-no-specializations-of-stdhash-for-standard-containers
namespace std
{
template <typename T> struct hash<std::deque<T>>
{
   size_t operator()(const std::deque<T> &a) const
   {
      return boost::hash_range(a.begin(), a.end());
   }
};
} // namespace std

class Thread;
class HooksManager;
class MagicServer;
class ThreadManager;
class ThreadStatsManager;
class StatsManager;
class CoreManager;
class Config;
class Simulator;
namespace config { class Config; }

typedef std::deque<IntPtr> CallStack;

class RoutineTracerThread
{
 public:
   RoutineTracerThread(Thread *thread, HooksManager *hooks_manager, MagicServer *magic_server);
   virtual ~RoutineTracerThread();

   void routineEnter(IntPtr eip, IntPtr esp, IntPtr returnEip);
   void routineExit(IntPtr eip, IntPtr esp);
   void routineAssert(IntPtr eip, IntPtr esp);

   const CallStack &getCallStack() const
   {
      return m_stack;
   }

 protected:
   Lock m_lock;
   Thread *m_thread;
   CallStack m_stack;
   IntPtr m_last_esp;
   HooksManager *m_hooks_manager;
   MagicServer *m_magic_server;

 private:
   bool unwindTo(IntPtr eip);

   void routineEnter_unlocked(IntPtr eip, IntPtr esp, IntPtr callEip);

   virtual void functionEnter(IntPtr eip, IntPtr callEip) = 0;
   virtual void functionExit(IntPtr eip) = 0;
   virtual void functionChildEnter(IntPtr eip, IntPtr eip_child) = 0;
   virtual void functionChildExit(IntPtr eip, IntPtr eip_child) = 0;

   void hookRoiBegin();
   void hookRoiEnd();
   static SInt64 __hook_roi_begin(UInt64 user, UInt64 arg)
   {
      ((RoutineTracerThread *)user)->hookRoiBegin();
      return 0;
   }
   static SInt64 __hook_roi_end(UInt64 user, UInt64 arg)
   {
      ((RoutineTracerThread *)user)->hookRoiEnd();
      return 0;
   }
};

class RoutineTracer
{
 public:
   class Routine
   {
    public:
      const IntPtr m_eip;
      char *m_name, *m_imgname, *m_filename, *m_location;
      IntPtr m_offset;
      int m_column, m_line;

      Routine(IntPtr eip, const char *name, const char *imgname, IntPtr offset, int column, int line,
              const char *filename);
      void updateLocation(const char *name, const char *imgname, IntPtr offset, int column, int line,
                          const char *filename);
   };

   static RoutineTracer *create(SimulationContext *context);

   RoutineTracer();
   virtual ~RoutineTracer();

   virtual void setThreadManager(ThreadManager *thread_manager) { m_thread_manager = thread_manager; }

   virtual void addRoutine(IntPtr eip, const char *name, const char *imgname, IntPtr offset, int column, int line,
                           const char *filename) = 0;
   virtual bool hasRoutine(IntPtr eip) = 0;
   virtual RoutineTracerThread *getThreadHandler(Thread *thread) = 0;

   virtual const Routine *getRoutineInfo(IntPtr eip)
   {
      return NULL;
   }

 protected:
   ThreadManager *m_thread_manager;
};

#endif // __ROUTINE_TRACER_H
