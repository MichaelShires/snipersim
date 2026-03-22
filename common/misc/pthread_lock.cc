#include "pthread_lock.h"

PthreadLock::PthreadLock()
{
   pthread_mutex_init(&_mutx, NULL);
}

PthreadLock::~PthreadLock()
{
   pthread_mutex_destroy(&_mutx);
}

void PthreadLock::acquire()
{
   pthread_mutex_lock(&_mutx);
}

void PthreadLock::release()
{
   pthread_mutex_unlock(&_mutx);
}

PthreadRecursiveLock::PthreadRecursiveLock()
{
   pthread_mutexattr_t attr;
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(&_mutx, &attr);
   pthread_mutexattr_destroy(&attr);
}

PthreadRecursiveLock::~PthreadRecursiveLock()
{
   pthread_mutex_destroy(&_mutx);
}

void PthreadRecursiveLock::acquire()
{
   pthread_mutex_lock(&_mutx);
}

void PthreadRecursiveLock::release()
{
   pthread_mutex_unlock(&_mutx);
}

__attribute__((weak)) LockImplementation *LockCreator_Default::create()
{
   return new PthreadLock();
}

__attribute__((weak)) LockImplementation *LockCreator_RwLock::create()
{
   return new PthreadLock();
}

__attribute__((weak)) LockImplementation *LockCreator_Spinlock::create()
{
   return new PthreadLock();
}

__attribute__((weak)) LockImplementation *LockCreator_Recursive::create()
{
   return new PthreadRecursiveLock();
}
