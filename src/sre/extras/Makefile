# ============================================================================
# libsre-extras.so — optional SRE addon, ABI-aware (ARM64 guest
# module: Mini.MemoryAddress + Raijin signature FFI). Optional: loaded only
# when the file exists next to the SRE library. Version-dependent offsets are
# passed by the host through SreExtrasInit::abi and validated by
# sre_extras_abi_validate() — no hardcoded offsets remain.
#   make            -> ../../../bin/libs/libsre-extras.so
#   make clean
# ============================================================================

CC      := aarch64-linux-gnu-gcc
# This Makefile lives at src/sre/extras — the SRE tree root is one level up.
SRE_DIR := ..
OUT     := ../../../bin/libs/libsre-extras.so
BUILD   := build

# Layout: version code lives in sre/sre12 (sre_lua.h etc.), shared infra in
# sre/base (include/, lua/src, sre_lua_compat.h).
CFLAGS  := -shared -fPIC -O2 -nostdlib -fno-builtin -fno-stack-protector \
           -I$(SRE_DIR)/sre12 -I$(SRE_DIR)/base -I$(SRE_DIR)/base/include \
           -I$(SRE_DIR)/base/lua/src -Ilibffi \
           -include $(SRE_DIR)/base/sre_lua_compat.h

SRCS := sre_extras_init.c memory.c sre_ffi.c raijin_ffi.c \
        libffi/prep_cif.c libffi/types.c libffi/ffi.c libffi/sysv.S

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
OBJS := $(patsubst %.S,$(BUILD)/%.o,$(OBJS))

all: $(OUT)

$(OUT): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) -shared -fPIC -nostdlib -o $@ $(OBJS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(OUT)

.PHONY: all clean
