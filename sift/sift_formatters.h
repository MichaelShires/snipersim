#ifndef SIFT_FORMATTERS_H
#define SIFT_FORMATTERS_H

#include "sift_format.h"
#include <cstdint>
#include <vector>

class vostream;

namespace Sift
{

class RecordFormatter
{
 public:
   virtual ~RecordFormatter()
   {
   }
   virtual void write(vostream *output) const = 0;
};

class SyscallRecord : public RecordFormatter
{
   uint16_t syscall_number;
   const char *data;
   uint32_t size;

 public:
   SyscallRecord(uint16_t num, const char *d, uint32_t s) : syscall_number(num), data(d), size(s)
   {
   }

   void write(vostream *output) const override;
};

class MagicRecord : public RecordFormatter
{
   uint64_t a, b, c;

 public:
   MagicRecord(uint64_t _a, uint64_t _b, uint64_t _c) : a(_a), b(_b), c(_c)
   {
   }

   void write(vostream *output) const override;
};

} // namespace Sift

#endif
