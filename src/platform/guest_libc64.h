#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif

// Android AArch64 is LP64. Windows x64 is LLP64: host long/fseek/ftell
// cannot represent guest long/off_t, even though both pointer sizes are 64 bits.
namespace guest64 {
inline bool open_writes(int flags) { return (flags & (1 | 2 | 0100)) != 0; }
inline int open(const char* path, int flags, int mode) {
#ifdef _WIN32
    // Android/Linux asm-generic flags are not UCRT flags. In particular,
    // guest O_CREAT (0100) means _O_TEMPORARY on Windows and would delete saves!
    int host=(flags & 3) | _O_BINARY;
    if (flags & 0100) host |= _O_CREAT;
    if (flags & 0200) host |= _O_EXCL;
    if (flags & 01000) host |= _O_TRUNC;
    if (flags & 02000) host |= _O_APPEND;
    if (flags & 02000000) host |= _O_NOINHERIT;
    return _open(path,host,mode);
#else
    return ::open(path,flags,mode);
#endif
}
inline int64_t strtol(const char* text, char** end, int base) {
    return std::strtoll(text,end,base);
}
inline uint64_t strtoul(const char* text, char** end, int base) {
    return std::strtoull(text,end,base);
}
inline int fseek(FILE* stream, int64_t offset, int whence) {
#ifdef _WIN32
    return _fseeki64(stream,offset,whence);
#else
    return fseeko(stream,offset,whence);
#endif
}
inline int64_t ftell(FILE* stream) {
#ifdef _WIN32
    return _ftelli64(stream);
#else
    return ftello(stream);
#endif
}
}
