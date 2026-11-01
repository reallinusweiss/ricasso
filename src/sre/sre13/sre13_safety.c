/*
 * sre13_safety.c — Safety overrides and crash prevention for 1.4.13
 *
 * Death-screen freeze fix (mirrors SRE v1.4.12 src/sre/sre_gui_native.c):
 *
 *   Swordigo 1.4.13 dies → GameOverViewController is presented. 4.5 s later
 *   its Update() wants to show the "game_over" interstitial via
 *   OnlineController::ShowInterstitialAd("game_over", 60, 0.4). On desktop
 *   there is no ad SDK, so that callback that normally triggers
 *   GameOverViewDidContinue() never fires → permanent death-screen freeze.
 *
 *   Fix: hook the ad request (ShowAdMaybe symbol AND the inlined copy inside
 *   GameOverViewController::Update) to raise the engine's one-shot flag at
 *   this+0x50 and queue the controller. After updateApplication returns, the
 *   host calls sre_gameover_respawn_tick() (host already polls
 *   g_sre_gameover_respawn_controller every frame), which validates the
 *   controller's root view (GUIViewController::view px at +0x18, installed
 *   by LoadView) and invokes GameOverViewDidContinue — the engine's real
 *   respawn path (pushes a fresh GameViewController at the checkpoint).
 *
 *   Host wiring (main.cpp, shared by both ABIs):
 *     - resolves g_sre_GameOverVC_DidContinue -> _ZN5Caver22GameOverViewController23GameOverViewDidContinueEPNS_12GameOverViewE
 *     - calls sre_gameover_respawn_tick when g_sre_gameover_respawn_controller != 0
 */

#include "sre13.h"

/* Engine's GameOverViewDidContinue — filled by the host from libswordigo.so's
 * dynsym (mangled name above) at SRE load time. */
pfn_GameOverVC_DidContinue g_sre_GameOverVC_DidContinue = 0;

/* Controller whose deferred respawn is pending host-side servicing. */
volatile uint64_t g_sre_gameover_respawn_controller = 0;

/* Re-entry guard so a DidContinue tail (which re-enters the ad request in
 * 1.4.13) or an in-frame re-queue cannot recurse while we are draining. */
static volatile int s_gameover_respawn_running = 0;

/* No-op iOS audio interruption resumption */
void* sre_AudioSystem_EndAudioInterruptionIfNecessary(void* self) {
    return self;
}

/* Stack canary mismatch safety handler — return cleanly without aborting.
 * __stack_chk_fail is called right before the function epilogue
 * (BL __stack_chk_fail; epilogue ...), so returning to LR drops straight
 * into the epilogue with the full stack frame intact. */
void sre_stack_chk_fail(void) {
    return;
}

/* GameOverViewController::ShowAdMaybe — desktop respawn instead of ads.
 *
 * The engine calls this when the death screen wants to show the interstitial
 * ad (and 1.4.13 also embeds an identical body inside Update/DidContinue).
 * Original semantics: if (!this[+0x50]) { this[+0x50] = 1; ShowInterstitialAd("game_over"); }
 * We keep the same one-shot flag so GameOverViewController::Update stops
 * re-requesting the ad every frame, then queue the controller for the host
 * to service with GameOverViewDidContinue after updateApplication returns.
 */
void sre_GameOverViewController_ShowAdMaybe(void* self) {
    if (!self || s_gameover_respawn_running) return;

    /* Match the engine's one-shot flag (this + 0x50) so the inlined ad block
     * in Update()/DidContinue() is skipped and the request stays one-shot. */
    *((volatile uint8_t*)self + 0x50) = 1;
    g_sre_gameover_respawn_controller = (uint64_t)(uintptr_t)self;
}

/* Called as a separate guest invocation by the host AFTER the full
 * updateApplication call chain has returned (never from inside the frame —
 * DidContinue starts a scene/VC transition that must not re-enter the
 * engine mid-frame).
 *
 * 1.4.13 GameOverViewDidContinue immediately writes view+0x100, so the view
 * must be non-NULL. GUIViewController::view shared_ptr px sits at +0x18
 * (LoadView installs it). If it is not valid yet, clear the one-shot flag so
 * Update's timer gate can request again next frame. */
void sre_gameover_respawn_tick(void) {
    uint64_t controller = g_sre_gameover_respawn_controller;
    if (!controller || s_gameover_respawn_running ||
        !g_sre_GameOverVC_DidContinue) return;

    g_sre_gameover_respawn_controller = 0;
    s_gameover_respawn_running = 1;

    uint64_t view = *(uint64_t*)(uintptr_t)(controller + 0x18);
    if (view >= 0x10000ULL && view < 0x0000800000000000ULL && (view & 7) == 0) {
        g_sre_GameOverVC_DidContinue((void*)(uintptr_t)controller,
                                     (void*)(uintptr_t)view);
    } else {
        /* LoadView not finished / view torn down — allow Update to request
         * the respawn again once the view becomes valid. */
        *((volatile uint8_t*)(uintptr_t)controller + 0x50) = 0;
    }

    s_gameover_respawn_running = 0;
}
