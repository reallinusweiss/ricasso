#ifndef SRE13_BUTTON_CONTROLLER_H
#define SRE13_BUTTON_CONTROLLER_H

#include <stdint.h>
#include <stddef.h>
#include "lua.h"

#define SRE_BTN_MAX       128
#define SRE_BTN_ID_LEN    32
#define SRE_BTN_LABEL_LEN 64
#define SRE_OVERLAY_MAX   8

typedef struct {
    char     id[SRE_BTN_ID_LEN];       /* Lua string ID */
    char     label[SRE_BTN_LABEL_LEN]; /* Display text */
    float    x, y;                      /* Normalized position (0-1) */
    float    w, h;                      /* Normalized dimensions */
    float    alpha;                     /* Overall alpha 0-255 */
    float    scale_x, scale_y;         /* Scaling factors */
    int      text_color;               /* Packed ARGB */
    float    text_scale;               /* Text size multiplier */
    int      bg_alpha;                 /* Background alpha (0-255) */
    int      hidden;                   /* Per-button hidden flag */
    int      clickable;                /* Whether it accepts clicks */
    int      movable;                  /* Can be dragged */
    int      snapback;                 /* Returns to original pos on release */
    float    home_x, home_y;           /* Original position (for snapback) */
    int      padding_l, padding_t, padding_r, padding_b;
    int      alignment;                /* Text alignment / gravity */
    char     overlay_id[SRE_BTN_ID_LEN]; /* Belongs to overlay */
    int      confined;                  /* Confined to overlay bounds */
    /* State (written by host, read by SRE) */
    volatile int pressed;              /* Host writes: 1=down, 0=up */
    volatile int released;             /* Host writes: 1 on release */
    volatile int dragging;             /* Host writes: 1=dragging, 0=not */
    volatile float cur_x, cur_y;       /* Current position (after drag) */
    int      active;                   /* 1 = slot in use, 0 = free */
    int      dirty;                    /* 1 = needs visual update by host */
} SreBtnSlot;

typedef struct {
    char     id[SRE_BTN_ID_LEN];
    float    x, y;
    float    w, h;
    int      bg_color;
    int      bg_alpha;
    float    corner_radius;
    int      hidden;
    int      movable;
    int      pinchable;
    float    scale_factor;
    int      pinching;
    float    separators[8];
    int      separator_count;
    int      active;
    int      dirty;
} SreOverlaySlot;

extern volatile SreBtnSlot g_sre_buttons[SRE_BTN_MAX];
extern volatile SreOverlaySlot g_sre_overlays[SRE_OVERLAY_MAX];
extern volatile int g_sre_btn_count;
extern volatile int g_sre_btn_dirty;
extern volatile int g_sre_btn_delete_all;
extern volatile int g_sre_btn_globally_hidden;

void sre13_register_button_controller(lua_State *L);

#endif /* SRE13_BUTTON_CONTROLLER_H */
