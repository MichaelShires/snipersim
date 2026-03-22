#ifndef __SIFT_FORMAT_H
#define __SIFT_FORMAT_H

// Sniper Instruction Trace File Format
//
// ia32 and intel64, little-endian

#include "sift.h"

#if defined(PIN_CRT)
#include <stdint.h>
#else
#include <cstdint>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <sys/types.h>
#endif

namespace Sift
{

const uint32_t MagicNumber = 0x54464953; // "SIFT"
const uint64_t PAGE_SIZE_SIFT = 4096;
const uint32_t ICACHE_SIZE = 0x1000;
const uint64_t ICACHE_OFFSET_MASK = ICACHE_SIZE - 1;
const uint64_t ICACHE_PAGE_MASK = ~ICACHE_OFFSET_MASK;

#define NUM_PAPI_COUNTERS 6

struct Header
{
   uint32_t magic;
   uint32_t size;    //< Size of extra header, in bytes
   uint64_t options; //< Bit field of Option* options

   uint8_t reserved[];
};

enum Mode
{
   ModeUnknown = 0,
   ModeDetailed,
   ModeCacheOnly,
   ModeIcount,
   ModeNoSimulation,
   ModeMemory,
   ModeStop,
};

enum Option
{
   OptionNone = 0,
   OptionIcount = (1 << 0),
   OptionResponseFiles = (1 << 1),
   CompressionZlib = (1 << 2),
   ArchIA32 = (1 << 3),
   PhysicalAddress = (1 << 4),
   IcacheVariable = (1 << 5),
};

struct Record
{
   union
   {
      struct
      {
         uint8_t size : 4; // 1-15
         uint8_t num_addresses : 2;
         uint8_t is_branch : 1;
         uint8_t taken : 1;
         uint64_t addresses[];
      } Instruction;
      struct
      {
         uint8_t type : 4; // 0
         uint8_t size : 4; // 1-15
         uint8_t num_addresses : 2;
         uint8_t is_branch : 1;
         uint8_t taken : 1;
         uint8_t is_predicate : 1;
         uint8_t executed : 1;
         uint64_t addr;
         uint64_t addresses[];
      } InstructionExt;
      struct
      {
         uint8_t zero; // 0
         uint8_t type;
         uint32_t size;
         uint8_t data[];
      } Other;
   };
};

typedef enum
{
   RecOtherNone = 0,
   RecOtherSync = 1,
   RecOtherSyscall = 2,
   RecOtherNewThread = 3,
   RecOtherMemoryCount = 4,
   RecOtherLogicalId = 5,
   RecOtherMemoryOp = 6,
   RecOtherFork = 7,
   RecOtherEmuRequest = 8,
   RecOtherEmuReply = 9,
   RecOtherInstructionCount = 10,
   RecOtherCacheEfficiencyEvict = 11,
   RecOtherMagic = 12,
   RecOtherRoutineChange = 13,
   RecOtherRoutineAnnounce = 14,
   RecOtherISAChange = 15,
   RecOtherPapiDevice = 16,
   RecOtherPapiConfig = 17,
   RecOtherPapiEvent = 18,
   RecOtherPapiCounter = 19,
   RecOtherChecksum = 20,
   RecOtherMemoryResponse = 21,
   RecOtherLogical2Physical = 22,
   RecOtherIcache = 23,
   RecOtherIcacheVariable = 24,
   RecOtherSyncResponse = 25,
   RecOtherCacheOnly = 26,
   RecOtherOutput = 27,
   RecOtherSyscallRequest = 28,
   RecOtherNewThreadResponse = 29,
   RecOtherJoin = 30,
   RecOtherJoinResponse = 31,
   RecOtherForkResponse = 32,
   RecOtherMagicInstruction = 33,
   RecOtherMagicInstructionResponse = 34,
   RecOtherEmu = 35,
   RecOtherEmuResponse = 36,
   RecOtherMemoryRequest = 37,
   RecOtherSyscallResponse = 38,
   RecOtherEnd = 39,
   RecOtherShutdown = 40,
} RecOtherType;

struct MemoryCount
{
   uint64_t instructions;
   uint64_t reads;
   uint64_t writes;
};

struct MemoryOp
{
   uint8_t type; // MemoryOpType
   uint64_t address;
   uint32_t size;
};

typedef enum
{
   EmuTypeRdtsc = 1,
   EmuTypeCpuid = 2,
   EmuTypeGetProcInfo = 3,
   EmuTypeGetTime = 4,
   EmuTypePAPIstart = 5,
   EmuTypePAPIread = 6,
   EmuTypeSetThreadInfo = 7,
} EmuType;

struct EmuRequest
{
   uint8_t type; // EmuType
   union
   {
      struct
      {
         uint32_t eax, ecx;
      } cpuid;
      struct
      {
         uint32_t eventset;
      } papi;
      struct
      {
         uint64_t tid;
      } setthreadinfo;
   };
};

struct EmuReply
{
   union
   {
      struct
      {
         uint64_t cycles;
      } rdtsc;
      struct
      {
         uint32_t eax, ebx, ecx, edx;
      } cpuid;
      struct
      {
         uint64_t procid, nprocs, emunprocs;
      } getprocinfo;
      struct
      {
         uint64_t time_ns;
      } gettime;
      struct
      {
         uint64_t values[NUM_PAPI_COUNTERS];
      } papi;
   };
};

typedef enum
{
   CacheOnlyMemUnknown = 0,
   CacheOnlyMemRead,
   CacheOnlyMemReadEx,
   CacheOnlyMemWrite,
   CacheOnlyMemIcache,
   CacheOnlyBranchTaken,
   CacheOnlyBranchNotTaken,
} CacheOnlyType;

// Determine record type based on first uint8_t
inline bool IsInstructionSimple(uint8_t byte)
{
   return byte > 0;
}

}; // namespace Sift

#endif // __SIFT_FORMAT_H
