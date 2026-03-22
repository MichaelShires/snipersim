#include "sift_formatters.h"
#include "zfstream.h"
#include <cstring>

namespace Sift
{

void SyscallRecord::write(vostream *output) const
{
   Record rec;
   rec.Other.zero = 0;
   rec.Other.type = RecOtherSyscallRequest;
   rec.Other.size = sizeof(uint16_t) + size;
   output->write(reinterpret_cast<char *>(&rec), sizeof(rec.Other));
   output->write(reinterpret_cast<const char *>(&syscall_number), sizeof(uint16_t));
   output->write(data, size);
}

void MagicRecord::write(vostream *output) const
{
   Record rec;
   rec.Other.zero = 0;
   rec.Other.type = RecOtherMagicInstruction;
   rec.Other.size = 3 * sizeof(uint64_t);
   output->write(reinterpret_cast<char *>(&rec), sizeof(rec.Other));
   output->write(reinterpret_cast<const char *>(&a), sizeof(uint64_t));
   output->write(reinterpret_cast<const char *>(&b), sizeof(uint64_t));
   output->write(reinterpret_cast<const char *>(&c), sizeof(uint64_t));
}

} // namespace Sift
