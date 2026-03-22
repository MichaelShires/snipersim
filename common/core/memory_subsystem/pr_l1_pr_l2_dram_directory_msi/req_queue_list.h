#pragma once

#include <map>
#include <queue>

#include "req_queue_list_template.h"
#include "shmem_req.h"

namespace PrL1PrL2DramDirectoryMSI
{
typedef ReqQueueListTemplate<ShmemReq> ReqQueueList;
}
