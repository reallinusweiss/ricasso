#include "sre/sre13/program_state_resume.h"
#include <array>
#include <iostream>
#include <cstring>

struct Script {
    alignas(8) std::array<unsigned char, 0x60> state{};
    int calls = 0;
    int result = 1;
    float next_wait = 0;
    bool stale_wait_seen = false;
    int& wait_mode() { return *reinterpret_cast<int*>(state.data()+0x50); }
    float& remaining() { return *reinterpret_cast<float*>(state.data()+0x54); }
    bool completed() const { return state[0x5b] != 0; }
    static int resume(void* context, int) {
        auto& s = *static_cast<Script*>(context);
        s.stale_wait_seen |= s.wait_mode()!=0;
        ++s.calls;
        if (s.next_wait > 0) { s.wait_mode()=1; s.remaining()=s.next_wait; }
        return s.result;
    }
    int run() { return sre13_resume_program(state.data(),this,0,resume); }
    void update(float dt) {
        if (!completed() && wait_mode()==1) {
            remaining()-=dt;
            if (remaining()<0) run();
        }
    }
};
int main() {
    // A timer may finish one coroutine and release its parent death/cutscene.
    Script death; death.next_wait=0.5f; death.run(); death.next_wait=0; death.result=0;
    for (int i=0;i<4;++i) death.update(0.1f);
    if (death.calls!=1 || death.completed()) return 1;
    for (int i=0;i<3;++i) death.update(0.1f);
    if (death.calls!=2 || !death.completed() || death.wait_mode()!=0 || death.stale_wait_seen) return 2;
    // Explicit Resume cancels an old timer; it cannot fire the old wait again.
    Script manual; manual.wait_mode()=1; manual.remaining()=0.01f; manual.run();
    manual.update(0.1f);
    if (manual.calls!=1 || manual.completed() || manual.wait_mode()!=0 || manual.stale_wait_seen) return 3;
    // Repeated AI waits keep their newly scheduled interval, without early fire.
    Script ai; ai.next_wait=0.5f; ai.run();
    for(int i=0;i<4;++i) ai.update(0.1f);
    if(ai.calls!=1) return 4;
    ai.update(0.2f);
    if(ai.calls!=2 || ai.completed() || ai.wait_mode()!=1 || ai.remaining()!=0.5f || ai.stale_wait_seen) return 5;
    // An errored coroutine must be retired like a successfully finished one.
    Script error; error.result=2;
    if(error.run()!=2 || !error.completed()) return 6;
    std::cout << "ProgramState: timed completion, parent release, explicit resume, repeated AI waits and error retirement passed\n";
}
