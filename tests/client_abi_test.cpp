#include "platform/guest_libc64.h"
#include <cerrno>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <cstring>
#ifndef _WIN32
#include <unistd.h>
#else
#define write _write
#define close _close
#endif
static void require(bool ok) { if (!ok) throw std::runtime_error("guest LP64/host ABI regression"); }
int main() {
    char* end=nullptr;
    require(guest64::strtol("4294967296x",&end,10)==INT64_C(4294967296) && *end=='x');
    require(guest64::strtol("-4294967297",&end,10)==-INT64_C(4294967297) && !*end);
    require(guest64::strtoul("18446744073709551615",&end,10)==UINT64_MAX && !*end);
    errno=0;
    require(guest64::strtol("9223372036854775808",&end,10)==INT64_MAX && errno==ERANGE);
    FILE* file=std::fopen("client-abi-position.tmp","w+b"); require(file!=nullptr);
    const int64_t beyond4gb=INT64_C(0x100000123);
    require(guest64::fseek(file,beyond4gb,SEEK_SET)==0 && guest64::ftell(file)==beyond4gb);
    require(guest64::fseek(file,-8,SEEK_CUR)==0 && guest64::ftell(file)==beyond4gb-8);
    // No data written at that offset: this creates no multi-GB file.
    std::fclose(file); std::remove("client-abi-position.tmp");
    const char* saved="client-abi-save.tmp";
    std::remove(saved);
    int fd=guest64::open(saved,1|0100|0200,0600); // guest WRONLY|CREAT|EXCL
    require(fd>=0);
    const char payload[]="save\n\032";
    require(write(fd,payload,sizeof(payload))==sizeof(payload)); close(fd);
    // Must still exist after close, with unchanged LF and Ctrl-Z bytes.
    file=std::fopen(saved,"rb"); require(file!=nullptr);
    char actual[sizeof(payload)]{};
    require(std::fread(actual,1,sizeof(actual),file)==sizeof(payload)); std::fclose(file);
    require(std::memcmp(actual,payload,sizeof(payload))==0);
    fd=guest64::open(saved,1|02000,0600); require(fd>=0); // guest APPEND
    require(write(fd,"!",1)==1); close(fd);
    file=std::fopen(saved,"rb"); require(file!=nullptr);
    require(guest64::fseek(file,0,SEEK_END)==0 && guest64::ftell(file)==sizeof(payload)+1);
    std::fclose(file); std::remove(saved);
    std::cout << "guest LP64: signed/unsigned conversions, overflow, 64-bit file offsets passed\n";
    std::cout << "guest open flags: persistent binary create/exclusive/append passed\n";
}
