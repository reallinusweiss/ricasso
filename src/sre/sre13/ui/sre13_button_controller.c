#include "sre13_button_controller.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lauxlib.h"

volatile SreBtnSlot g_sre_buttons[SRE_BTN_MAX] = {{0}};
volatile SreOverlaySlot g_sre_overlays[SRE_OVERLAY_MAX] = {{0}};
volatile int g_sre_btn_count = 0;
volatile int g_sre_btn_dirty = 0;
volatile int g_sre_btn_delete_all = 0;
volatile int g_sre_btn_globally_hidden = 0;

static volatile SreBtnSlot* sre13_find_btn(const char *id) {
    if (!id || !*id) return NULL;
    for (int i = 0; i < SRE_BTN_MAX; i++) {
        if (g_sre_buttons[i].active && strcmp((const char*)g_sre_buttons[i].id, id) == 0) {
            return &g_sre_buttons[i];
        }
    }
    return NULL;
}

static volatile SreOverlaySlot* sre13_find_overlay(const char *id) {
    if (!id || !*id) return NULL;
    for (int i = 0; i < SRE_OVERLAY_MAX; i++) {
        if (g_sre_overlays[i].active && strcmp((const char*)g_sre_overlays[i].id, id) == 0) {
            return &g_sre_overlays[i];
        }
    }
    return NULL;
}

static void strncpy_safe(volatile char *dst, const char *src, size_t maxlen) {
    size_t i = 0;
    while (src && src[i] && i < maxlen - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* ButtonController.New(id, label, x, y, w, h) */
static int l_btn_new(lua_State *L) {
    int top = lua_gettop(L);
    const char *id = luaL_checkstring(L, 1);
    const char *label = (top >= 2) ? luaL_optstring(L, 2, "") : "";
    float x = (top >= 3) ? (float)luaL_optnumber(L, 3, 0.5) : 0.5f;
    float y = (top >= 4) ? (float)luaL_optnumber(L, 4, 0.5) : 0.5f;
    float w = (top >= 5) ? (float)luaL_optnumber(L, 5, 0.15) : 0.15f;
    float h = (top >= 6) ? (float)luaL_optnumber(L, 6, 0.08) : 0.08f;

    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (!btn) {
        for (int i = 0; i < SRE_BTN_MAX; i++) {
            if (!g_sre_buttons[i].active) {
                btn = &g_sre_buttons[i];
                break;
            }
        }
    }
    if (!btn) {
        lua_pushnil(L);
        return 1;
    }

    strncpy_safe(btn->id, id, SRE_BTN_ID_LEN);
    strncpy_safe(btn->label, label, SRE_BTN_LABEL_LEN);
    btn->x = x;
    btn->y = y;
    btn->home_x = x;
    btn->home_y = y;
    btn->cur_x = x;
    btn->cur_y = y;
    btn->w = w;
    btn->h = h;
    btn->alpha = 255.0f;
    btn->scale_x = 1.0f;
    btn->scale_y = 1.0f;
    btn->text_color = 0xFFFFFFFF;
    btn->text_scale = 1.0f;
    btn->bg_alpha = 180;
    btn->hidden = 0;
    btn->clickable = 1;
    btn->movable = 0;
    btn->snapback = 0;
    btn->overlay_id[0] = '\0';
    btn->confined = 0;
    btn->pressed = 0;
    btn->released = 0;
    btn->dragging = 0;
    btn->active = 1;
    btn->dirty = 1;
    g_sre_btn_dirty = 1;

    lua_pushboolean(L, 1);
    return 1;
}

/* ButtonController.Delete(id) */
static int l_btn_delete(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        btn->active = 0;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.DeleteAll() */
static int l_btn_delete_all(lua_State *L) {
    for (int i = 0; i < SRE_BTN_MAX; i++) {
        g_sre_buttons[i].active = 0;
    }
    g_sre_btn_delete_all = 1;
    g_sre_btn_dirty = 1;
    return 0;
}

/* ButtonController.SetHidden(id, bool) */
static int l_btn_set_hidden(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    int hidden = lua_toboolean(L, 2);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        btn->hidden = hidden;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.SetHiddenAll(bool) */
static int l_btn_set_hidden_all(lua_State *L) {
    g_sre_btn_globally_hidden = lua_toboolean(L, 1);
    g_sre_btn_dirty = 1;
    return 0;
}

/* ButtonController.IsPressed(id) */
static int l_btn_is_pressed(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn && btn->active && !btn->hidden) {
        lua_pushboolean(L, btn->pressed);
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

/* ButtonController.IsDragging(id) */
static int l_btn_is_dragging(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn && btn->active && !btn->hidden) {
        lua_pushboolean(L, btn->dragging);
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

/* ButtonController.Exists(id) */
static int l_btn_exists(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    lua_pushboolean(L, sre13_find_btn(id) != NULL);
    return 1;
}

/* ButtonController.SetText(id, text) */
static int l_btn_set_text(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    const char *text = luaL_checkstring(L, 2);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        strncpy_safe(btn->label, text, SRE_BTN_LABEL_LEN);
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.SetPosition(id, x, y) */
static int l_btn_set_position(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        btn->x = x;
        btn->y = y;
        btn->cur_x = x;
        btn->cur_y = y;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.GetPosition(id) */
static int l_btn_get_position(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        lua_pushnumber(L, btn->cur_x);
        lua_pushnumber(L, btn->cur_y);
        return 2;
    }
    lua_pushnil(L);
    return 1;
}

/* ButtonController.SetAlpha(id, alpha) */
static int l_btn_set_alpha(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    float a = (float)luaL_checknumber(L, 2);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        btn->alpha = a;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.SetScaling(id, sx [, sy]) */
static int l_btn_set_scaling(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    float sx = (float)luaL_checknumber(L, 2);
    float sy = (lua_gettop(L) >= 3) ? (float)luaL_checknumber(L, 3) : sx;
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        btn->scale_x = sx;
        btn->scale_y = sy;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.SetDimensions(id, w, h) */
static int l_btn_set_dimensions(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    float w = (float)luaL_checknumber(L, 2);
    float h = (float)luaL_checknumber(L, 3);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        btn->w = w;
        btn->h = h;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.MakeMovable(id, snapback) */
static int l_btn_make_movable(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    int snapback = (lua_gettop(L) >= 2) ? lua_toboolean(L, 2) : 0;
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        btn->movable = 1;
        btn->snapback = snapback;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.SetClickable(id, bool) */
static int l_btn_set_clickable(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    int clk = lua_toboolean(L, 2);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (btn) {
        btn->clickable = clk;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.SetTextColor(id, r, g, b, a) or (id, packed) */
static int l_btn_set_text_color(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    volatile SreBtnSlot *btn = sre13_find_btn(id);
    if (!btn) return 0;
    if (lua_gettop(L) >= 5) {
        int r = (int)luaL_checkinteger(L, 2);
        int g = (int)luaL_checkinteger(L, 3);
        int b = (int)luaL_checkinteger(L, 4);
        int a = (int)luaL_optinteger(L, 5, 255);
        btn->text_color = ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    } else {
        btn->text_color = (int)luaL_checkinteger(L, 2);
    }
    btn->dirty = 1;
    g_sre_btn_dirty = 1;
    return 0;
}

/* ButtonController.NewOverlay(id, x, y, w, h) */
static int l_ovr_new(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    float x = (float)luaL_optnumber(L, 2, 0.5);
    float y = (float)luaL_optnumber(L, 3, 0.5);
    float w = (float)luaL_optnumber(L, 4, 0.4);
    float h = (float)luaL_optnumber(L, 5, 0.4);

    volatile SreOverlaySlot *ovr = sre13_find_overlay(id);
    if (!ovr) {
        for (int i = 0; i < SRE_OVERLAY_MAX; i++) {
            if (!g_sre_overlays[i].active) {
                ovr = &g_sre_overlays[i];
                break;
            }
        }
    }
    if (!ovr) return 0;

    strncpy_safe(ovr->id, id, SRE_BTN_ID_LEN);
    ovr->x = x;
    ovr->y = y;
    ovr->w = w;
    ovr->h = h;
    ovr->bg_color = 0x222222;
    ovr->bg_alpha = 200;
    ovr->corner_radius = 8.0f;
    ovr->hidden = 0;
    ovr->movable = 1;
    ovr->pinchable = 1;
    ovr->scale_factor = 1.0f;
    ovr->pinching = 0;
    ovr->separator_count = 0;
    ovr->active = 1;
    ovr->dirty = 1;
    g_sre_btn_dirty = 1;
    return 0;
}

/* ButtonController.RemoveOverlay(id) */
static int l_ovr_remove(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    volatile SreOverlaySlot *ovr = sre13_find_overlay(id);
    if (ovr) {
        ovr->active = 0;
        ovr->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.OverlayAddButton(overlayId, btnId) */
static int l_ovr_add_button(lua_State *L) {
    const char *ovr_id = luaL_checkstring(L, 1);
    const char *btn_id = luaL_checkstring(L, 2);
    volatile SreBtnSlot *btn = sre13_find_btn(btn_id);
    if (btn) {
        strncpy_safe(btn->overlay_id, ovr_id, SRE_BTN_ID_LEN);
        btn->confined = 1;
        btn->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.SetOverlayHidden(id, hidden) */
static int l_ovr_set_hidden(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    int hidden = lua_toboolean(L, 2);
    volatile SreOverlaySlot *ovr = sre13_find_overlay(id);
    if (ovr) {
        ovr->hidden = hidden;
        ovr->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.SetOverlayPosition(id, x, y) */
static int l_ovr_set_position(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    volatile SreOverlaySlot *ovr = sre13_find_overlay(id);
    if (ovr) {
        ovr->x = x;
        ovr->y = y;
        ovr->dirty = 1;
        g_sre_btn_dirty = 1;
    }
    return 0;
}

/* ButtonController.GetOverlayPosition(id) */
static int l_ovr_get_position(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    volatile SreOverlaySlot *ovr = sre13_find_overlay(id);
    if (ovr) {
        lua_pushnumber(L, ovr->x);
        lua_pushnumber(L, ovr->y);
        return 2;
    }
    lua_pushnil(L);
    return 1;
}

/* ButtonController.GetOverlayScaleFactor(id) */
static int l_ovr_get_scale(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    volatile SreOverlaySlot *ovr = sre13_find_overlay(id);
    if (ovr) {
        lua_pushnumber(L, ovr->scale_factor);
        return 1;
    }
    lua_pushnumber(L, 1.0);
    return 1;
}

static const luaL_Reg s_btn_ctrl_methods[] = {
    { "New",                     l_btn_new },
    { "addButton",               l_btn_new },
    { "Delete",                  l_btn_delete },
    { "removeButton",            l_btn_delete },
    { "DeleteAll",               l_btn_delete_all },
    { "removeAll",               l_btn_delete_all },
    { "SetHidden",               l_btn_set_hidden },
    { "setHidden",               l_btn_set_hidden },
    { "SetHiddenAll",            l_btn_set_hidden_all },
    { "IsPressed",               l_btn_is_pressed },
    { "isPressed",               l_btn_is_pressed },
    { "IsDragging",              l_btn_is_dragging },
    { "isDragging",              l_btn_is_dragging },
    { "Exists",                  l_btn_exists },
    { "SetText",                 l_btn_set_text },
    { "setText",                 l_btn_set_text },
    { "SetPosition",             l_btn_set_position },
    { "setPosition",             l_btn_set_position },
    { "GetPosition",             l_btn_get_position },
    { "getPosition",             l_btn_get_position },
    { "SetAlpha",                l_btn_set_alpha },
    { "setAlpha",                l_btn_set_alpha },
    { "SetScaling",              l_btn_set_scaling },
    { "setScaling",              l_btn_set_scaling },
    { "SetDimensions",           l_btn_set_dimensions },
    { "MakeMovable",             l_btn_make_movable },
    { "makeMovable",             l_btn_make_movable },
    { "SetClickable",            l_btn_set_clickable },
    { "setClickable",            l_btn_set_clickable },
    { "SetTextColor",            l_btn_set_text_color },
    { "setTextColor",            l_btn_set_text_color },
    { "NewOverlay",              l_ovr_new },
    { "newOverlay",              l_ovr_new },
    { "RemoveOverlay",           l_ovr_remove },
    { "removeOverlay",           l_ovr_remove },
    { "OverlayAddButton",        l_ovr_add_button },
    { "SetOverlayHidden",        l_ovr_set_hidden },
    { "setOverlayHidden",        l_ovr_set_hidden },
    { "SetOverlayPosition",      l_ovr_set_position },
    { "GetOverlayPosition",      l_ovr_get_position },
    { "GetOverlayScaleFactor",   l_ovr_get_scale },
    { NULL, NULL }
};

void sre13_register_button_controller(lua_State *L) {
    if (!L) return;
    int top = lua_gettop(L);

    lua_getglobal(L, "ButtonController");
    int exists = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (exists) {
        lua_settop(L, top);
        return;
    }

    luaL_register(L, "ButtonController", s_btn_ctrl_methods);
    /* Alias Button and OverlayController to ButtonController */
    lua_getglobal(L, "ButtonController");
    lua_setglobal(L, "Button");
    lua_getglobal(L, "ButtonController");
    lua_setglobal(L, "OverlayController");

    /* Ensure stack top is completely restored to what it was */
    lua_settop(L, top);
}
