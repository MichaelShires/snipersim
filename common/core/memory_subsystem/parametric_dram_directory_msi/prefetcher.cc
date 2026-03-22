#include "prefetcher.h"
#include "a53prefetcher.h"
#include "config.hpp"
#include "ghb_prefetcher.h"
#include "log.h"
#include "simple_prefetcher.h"
#include "simulator.h"

Prefetcher *Prefetcher::createPrefetcher(String type, String configName, core_id_t core_id, UInt32 shared_cores)
{
   if (type == "none")
      return NULL;
   else if (type == "simple")
      return new SimplePrefetcher(configName, core_id, shared_cores);
   else if (type == "ghb")
      return new GhbPrefetcher(configName, core_id);
   else if (type == "a53prefetcher")
      return new A53Prefetcher(configName, core_id);

   LOG_PRINT_ERROR("Invalid prefetcher type %s", type.c_str());
}
