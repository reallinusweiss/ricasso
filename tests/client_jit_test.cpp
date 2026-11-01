#include "dynarmic/interface/A64/a64.h"
#include "dynarmic/interface/A64/config.h"
#include "dynarmic/interface/halt_reason.h"
#include "dynarmic/interface/exclusive_monitor.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

struct Memory : Dynarmic::A64::UserCallbacks {
    std::array<unsigned char,4096> data{};
    Dynarmic::A64::Jit* jit{};
    uint64_t ticks = 1000;
    template<class T> T read(uint64_t a) { if (a > data.size()-sizeof(T)) throw std::runtime_error("read bounds"); T v; std::memcpy(&v,data.data()+a,sizeof(v)); return v; }
    template<class T> void write(uint64_t a,T v) { if (a > data.size()-sizeof(T)) throw std::runtime_error("write bounds"); std::memcpy(data.data()+a,&v,sizeof(v)); }
    uint8_t MemoryRead8(uint64_t a) override { return read<uint8_t>(a); }
    uint16_t MemoryRead16(uint64_t a) override { return read<uint16_t>(a); }
    uint32_t MemoryRead32(uint64_t a) override { return read<uint32_t>(a); }
    uint64_t MemoryRead64(uint64_t a) override { return read<uint64_t>(a); }
    Dynarmic::A64::Vector MemoryRead128(uint64_t a) override { return read<Dynarmic::A64::Vector>(a); }
    void MemoryWrite8(uint64_t a,uint8_t v) override { write(a,v); }
    void MemoryWrite16(uint64_t a,uint16_t v) override { write(a,v); }
    void MemoryWrite32(uint64_t a,uint32_t v) override { write(a,v); }
    void MemoryWrite64(uint64_t a,uint64_t v) override { write(a,v); }
    void MemoryWrite128(uint64_t a,Dynarmic::A64::Vector v) override { write(a,v); }
    bool MemoryWriteExclusive32(uint64_t a,uint32_t value,uint32_t expected) override {
        if (read<uint32_t>(a)!=expected) return false;
        write(a,value); return true;
    }
    void InterpreterFallback(uint64_t,size_t) override { throw std::runtime_error("unexpected interpreter fallback"); }
    void ExceptionRaised(uint64_t,Dynarmic::A64::Exception) override { throw std::runtime_error("unexpected guest exception"); }
    void CallSVC(uint32_t n) override { if (n!=1) throw std::runtime_error("SVC"); jit->HaltExecution(Dynarmic::HaltReason::UserDefined1); }
    void AddTicks(uint64_t n) override { ticks = n < ticks ? ticks-n : 0; }
    uint64_t GetTicksRemaining() override { return ticks; }
    uint64_t GetCNTPCT() override { return 0; }
};
int main() {
    Memory m;
    // MOV X0,#40; ADD X0,X0,#2; STR X0,[X1]; SVC #1.
    const uint32_t code[] = {0xd2800500,0x91000800,0xf9000020,0xd4000021};
    std::memcpy(m.data.data(),code,sizeof(code));
    Dynarmic::ExclusiveMonitor monitor(1);
    Dynarmic::A64::UserConfig cfg; cfg.callbacks=&m; cfg.code_cache_size=16*1024*1024;
    cfg.global_monitor=&monitor; cfg.processor_id=0;
    Dynarmic::A64::Jit jit(cfg); m.jit=&jit; jit.SetPC(0); jit.SetRegister(1,256);
    jit.Run();
    if (jit.GetRegister(0)!=42 || m.read<uint64_t>(256)!=42) return 1;
    // Invalidate compiled code and prove the next execution sees the patch.
    m.write<uint32_t>(0,0xd2800640); // MOV X0,#50
    jit.InvalidateCacheRange(0,sizeof(code)); jit.ClearHalt(Dynarmic::HaltReason::UserDefined1);
    m.ticks=1000; jit.SetPC(0); jit.Run();
    if (jit.GetRegister(0)!=52 || m.read<uint64_t>(256)!=52) return 2;
    // LDAXR W0,[X1]; ADD W2,W0,#1; STLXR W3,W2,[X1]; CBNZ W3,retry; SVC #1.
    // Guest atomics must work intact, without rewriting store-exclusive opcodes.
    const uint32_t atomic_code[]={0x885ffc20,0x11000402,0x8803fc22,0x35ffffa3,0xd4000021};
    std::memcpy(m.data.data(),atomic_code,sizeof(atomic_code));
    m.write<uint32_t>(256,99);
    jit.InvalidateCacheRange(0,sizeof(atomic_code)); jit.ClearHalt(Dynarmic::HaltReason::UserDefined1);
    m.ticks=1000; jit.SetPC(0); jit.Run();
    if (jit.GetRegister(0)!=99 || m.read<uint32_t>(256)!=100 || jit.GetRegister(3)!=0) return 3;
    // CLREX; STLXR must fail and leave memory unchanged when no reservation exists.
    m.write<uint32_t>(0,0xd5033f5f); m.write<uint32_t>(4,0x8803fc22); m.write<uint32_t>(8,0xd4000021);
    jit.InvalidateCacheRange(0,12); jit.ClearHalt(Dynarmic::HaltReason::UserDefined1);
    m.ticks=1000; jit.SetPC(0); jit.SetRegister(2,200); jit.Run();
    if (jit.GetRegister(3)!=1 || m.read<uint32_t>(256)!=100) return 4;
    std::cout << "A64 JIT: arithmetic, memory, SVC, cache invalidation, atomic retry and reservation failure passed\n";
}
