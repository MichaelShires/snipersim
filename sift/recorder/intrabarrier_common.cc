
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs
 *  and could serve as the starting point for developing your first PIN tool
 */
// #include "globals.h"
#include "cond.h"
#include "control_manager.H"
#include "lock.h"
#include "pin.H"
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>
#define OMP_BEGIN "GOMP_parallel_start"
#define OMP_END "GOMP_parallel_end"
#include "bbv_count.h"
#include "globals.h"
#include "hooks_manager.h"
#include "recorder_control.h"
#include "sift/sift_format.h"
#include "sift_assert.h"
#include "sim_api.h"
// #include "threads.h"
#include <cstdint>
#include <set>
#include <sstream>
#include <utility>
// #include "threads.h"
#include "bbv_count_cluster.h"
#include "cond.h"
#include "log2.h"
#include "mtng.h"
#include "to_json.h"
#include "tool_warmup.h"
#include <set>
// #define tuple pair
#include "tuple_hash.h"
using namespace std;
// #define MARKER_INSERT
#include <deque>
#include <functional>
#include <iostream>
#include <queue>
// #include <tuple>
#include "to_json.h"
#include "trietree.h"
#include <cmath>
#include <utility>

uint64_t combinehash(std::tuple<uint64_t, uint32_t> key1, std::tuple<uint64_t, uint32_t> key2)
{
   uint64_t seed = 0;
//
#define hash(data) seed ^= std::hash<uint64_t>()(data) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
   hash(key1.first) hash((uint64_t)key1.second) hash(key2.first) hash((uint64_t)key2.second)
#undef hash
       return seed;
}
uint64_t combinehash(uint64_t key1, uint64_t key2)
{
   uint64_t seed = 0;
//
#define hash(data) seed ^= std::hash<uint64_t>()(data) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
   hash(key1) hash(key2)
#undef hash
       return seed;
}
