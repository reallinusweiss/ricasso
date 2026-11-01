/*
 * sre_base.c — shared Swordigo Runtime Engine base entry (guest-side, ARM64).
 *
 * The base is SOURCE-level shared infra only: this directory (lua/,
 * luasocket/, luafilesystem/, raknet/, toml-c/, include/, sre_lua_compat.h,
 * sre_setjmp.*, sre_base.c) is compiled INTO each version library
 * (libsre12.so for Swordigo 1.4.12, libsre13.so for Swordigo 1.4.13).
 * There is deliberately NO standalone libsre.so runtime module — only
 * libsre12.so / libsre13.so are loaded (plus the optional libsre-extras.so
 * addon, 1.4.12 only for now).
 *
 * This TU is the base's stable entry point. Keep everything in here free of
 * any engine-version offset/symbol so it can be compiled for every ABI.
 */

/* sre_base_abi_id — engine ABI this base instance is serving, written by the
 * host after load (0 = unset, 12 = Swordigo 1.4.12, 13 = Swordigo 1.4.13). */
unsigned long long sre_base_abi_id = 0;

/* sre_base_tag — human-readable base version for logs/forensics. */
const char* sre_base_tag(void) {
    return "sre-base 1.0 (shared infra, pre-split)";
}

/* sre_base_init — host calls this right after the base module is loaded and
 * relocated. Reserved for a future host bridge ABI pointer table.
 * Returns 0 on success. */
int sre_base_init(unsigned long long swordigo_base, unsigned long long reserved) {
    (void)swordigo_base;
    (void)reserved;
    return 0;
}
