#include "clock_skew_minimization_object.h"
#include "barrier_sync_client.h"
#include "barrier_sync_server.h"
#include "config.hpp"
#include "log.h"
#include "simulator.h"
#include "simulation_context.h"

ClockSkewMinimizationObject::Scheme ClockSkewMinimizationObject::parseScheme(String scheme)
{
   if (scheme == "none")
      return NONE;
   else if (scheme == "barrier")
      return BARRIER;
   else {
      config::Error("Unrecognized clock skew minimization scheme: %s", scheme.c_str());
   }
}

ClockSkewMinimizationClient *ClockSkewMinimizationClient::create(Core *core, SimulationContext *context)
{
   Scheme scheme = context->getConfig()->getClockSkewMinimizationScheme();

   switch (scheme) {
   case BARRIER:
      return new BarrierSyncClient(core);

   default:
      LOG_PRINT_ERROR("Unrecognized scheme: %u", scheme);
      return (ClockSkewMinimizationClient *)NULL;
   }
}

ClockSkewMinimizationManager *ClockSkewMinimizationManager::create(SimulationContext *context)
{
   Scheme scheme = context->getConfig()->getClockSkewMinimizationScheme();

   switch (scheme) {
   case BARRIER:
      return (ClockSkewMinimizationManager *)NULL;

   default:
      LOG_PRINT_ERROR("Unrecognized scheme: %u", scheme);
      return (ClockSkewMinimizationManager *)NULL;
   }
}

ClockSkewMinimizationServer *ClockSkewMinimizationServer::create(SimulationContext *context)
{
   Scheme scheme = context->getConfig()->getClockSkewMinimizationScheme();

   switch (scheme) {
   case BARRIER:
      return new BarrierSyncServer(context);

   default:
      LOG_PRINT_ERROR("Unrecognized scheme: %u", scheme);
      return (ClockSkewMinimizationServer *)NULL;
   }
}

SubsecondTime ClockSkewMinimizationServer::getGlobalTime(bool upper_bound)
{
   LOG_PRINT_ERROR("This clock skew minimization server does not support getGlobalTime");
}
