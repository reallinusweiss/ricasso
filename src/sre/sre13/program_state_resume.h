#ifndef SRE13_PROGRAM_STATE_RESUME_H
#define SRE13_PROGRAM_STATE_RESUME_H

/* Verified ARM64 1.4.13 ProgramState offsets. No timer values are changed. */
static inline int sre13_resume_program(void* self, void* lua_state, int nargs,
                                      int (*resume)(void*, int)) {
    *(int*)((char*)self + 0x50) = 0;
    int result = resume(lua_state, nargs);
    if (result != 1) /* LUA_YIELD: the coroutine remains active. */
        *((unsigned char*)self + 0x5b) = 1;
    return result;
}
#endif
