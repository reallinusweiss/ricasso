#define LOG_TAG "SRE13Hook"
#include "hook.h"
#include "log.h"

sre_hook_installer_t g_sre_hook_installers[SRE_HOOK_INSTALLER_CAP] = {0};
int g_sre_hook_installer_count = 0;

dl_resolver_t g_dl_resolvers[DL_RESOLVER_CAP] = {0};
int g_dl_resolver_count = 0;

void sre_register_hook_installer(sre_hook_installer_t fn) {
    if (g_sre_hook_installer_count >= SRE_HOOK_INSTALLER_CAP) {
        LOGE("Hook installer cap reached!");
        return;
    }
    g_sre_hook_installers[g_sre_hook_installer_count++] = fn;
}

void dl_register_resolver(dl_resolver_t fn) {
    if (g_dl_resolver_count >= DL_RESOLVER_CAP) {
        LOGE("DL resolver cap reached!");
        return;
    }
    g_dl_resolvers[g_dl_resolver_count++] = fn;
}

void dl_resolve_all(void) {
    LOGI("Resolving %d dynamic symbols...", g_dl_resolver_count);
    for (int i = 0; i < g_dl_resolver_count; i++) {
        if (g_dl_resolvers[i]) {
            g_dl_resolvers[i]();
        }
    }
}

void init_hooks(void) {
    LOGI("Installing %d registered ProHooks...", g_sre_hook_installer_count);
    for (int i = 0; i < g_sre_hook_installer_count; i++) {
        if (g_sre_hook_installers[i]) {
            g_sre_hook_installers[i]();
        }
    }
    dl_resolve_all();
}

void *swordigo_dlsym(const char *symbol) {
    return (void*)srehost_get_symbol(symbol);
}
