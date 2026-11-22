/* Showroom Star Explorer: a native OpenRAC mod for Ratchet & Clank: Going Commando
 * (NTSC-U v1.01).
 *
 * 1. Blargian Red becomes Showroom Silver: the shop's name for it reads "Showroom Silver"
 *    (text 0x30BF), and its skill-point requirement in the Ship Shack's table is made 0. The
 *    silver itself is an OpenRAC texture pack (the port draws the paint, a texture the game
 *    uploads to the GS, from the pack's picture of the same hash).
 * 2. A new game starts with the showroom Star Explorer: the ship word (u32 0x1A7AF0, the shop's
 *    rows) gets Split Nose 0x1, Hi-Lift Wings 0x4, Triple Boost Acceleration Engine 0x10,
 *    Fusion Laser Cannons 0x40, Mega-Mine Launcher 0x100 and Multi-Torpedo Launcher 0x800,
 *    with paint 0 (now Showroom Silver), right after New Game's reset (func_00298E08 loads the
 *    default save, then marks the loaded hull and paint stale with -1), so the in-engine flight
 *    to Aranos already shows it.
 */
#include <stddef.h>
#include <stdint.h>

#ifndef OPENRAC_MOD_TARGET_MINOR /* the build sets it from the manifest\'s "api" */
#define OPENRAC_MOD_TARGET_MINOR 6
#endif
#include "openrac/openrac_mod.h"
#include "openrac/games/rac2.h"

OPENRAC_MOD_DEFINE("rac2-showroom-star-explorer", "0.3.4")

#define A_MAP          0x1A79F0u /* s32 current level */
#define A_SHIP         0x1A7AF0u /* u32 ship word: parts, weapons, paint (bits 16-20) */
#define A_HULL_LOADED  0x152C18u /* s32 hull model loaded (-1 none) */
#define A_PAINT_LOADED 0x152C30u /* s32 paint loaded (-1 none) */

#define SHOWROOM_PARTS 0x955u    /* nose 1, wings 4, 0x10, 0x40, 0x100, 0x800 */
#define TEXT_BLARGIAN_RED 0x30BF
#define LEVEL_SHIP_SHACK 24

static const openrac_api* g_api;
static openrac_mod* g_mod;
static openrac_rac2_Text_ById g_text_original;
static uint32_t g_text_slot; /* the port's text slot holding the name */

static int32_t g_last_map = -2;
static int g_new_game_done;
static int g_icon_done;
static unsigned g_icon_tries;

static const char NAME[] = "Showroom Silver";

static uint32_t rd32(uint32_t a) { return *(volatile uint32_t*)(g_api->memory + a); }
static void wr8(uint32_t a, uint8_t v) { *(volatile uint8_t*)(g_api->memory + a) = v; }
static void wr32(uint32_t a, uint32_t v) { *(volatile uint32_t*)(g_api->memory + a) = v; }
static void say(const char* s) { g_api->log(g_mod, OPENRAC_LOG_INFO, s); }

/* --- the name ----------------------------------------------------------------------------- */

static int32_t text_by_id(int32_t id) {
    if (id == TEXT_BLARGIAN_RED) {
        if (!g_text_slot) g_text_slot = (uint32_t)g_text_original(OPENRAC_RAC2_TEXT_SLOT + 7);
        if (g_text_slot && g_text_slot < 0x2000000u) {
            uint32_t i;
            for (i = 0; i < sizeof NAME; i++) wr8(g_text_slot + i, (uint8_t)NAME[i]);
            return (int32_t)g_text_slot;
        }
    }
    return g_text_original(id);
}

/* --- the Ship Shack ---------------------------------------------------------------------- */

/* The shop's skill-point column (36 rows; row 19 is Blargian Red, needing 3). */
static const int32_t SKILL_ROWS[36] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18,
                                       26, 3, 3, 3, 29, 15, 3, 10, 25, 29, 10, 15, 25, 15, 29, 3, 19, 19};

/* The Ship Shack's paint icons are one grey picture tinted by a colour per paint: a table in the
 * level's code of {text id, colour} pairs (Blargian Red 0x30BF: 0x00152056, then Orxon Green 0x30C0:
 * 0x0012461F). Its colour becomes silver. */
#define ICON_RED    0x00152056u
#define ICON_SILVER 0x007A7670u /* R 0x70, G 0x76, B 0x7A */

static int silver_the_icon(int quiet) {
    uint32_t a;
    int found = 0;
    for (a = 0x100000u; a + 16 <= 0x2000000u; a += 4) {
        const uint32_t* p = (const uint32_t*)(g_api->memory + a);
        if (p[0] != TEXT_BLARGIAN_RED || p[2] != 0x30C0u || p[3] != 0x0012461Fu) continue;
        if (p[1] != ICON_RED && p[1] != ICON_SILVER) continue;
        wr32(a + 4, ICON_SILVER);
        found++;
    }
    if (found) say("Showroom Silver's Ship Shack icon is silver");
    else if (!quiet) say("the Ship Shack's paint icon colours were not found yet");
    return found;
}

static void free_the_paint(void) {
    uint32_t a;
    int found = 0;
    for (a = 0x100000u; a + sizeof SKILL_ROWS <= 0x2000000u; a += 4) {
        const int32_t* p = (const int32_t*)(g_api->memory + a);
        int k, match = 1;
        if (p[17] != 18 || p[18] != 26) continue;
        for (k = 0; k < 36 && match; k++) {
            int32_t want = SKILL_ROWS[k];
            if (k == 19) match = p[k] == 3 || p[k] == 0;
            else match = p[k] == want;
        }
        if (match) {
            wr32(a + 19 * 4, 0);
            found++;
        }
    }
    say(found ? "Showroom Silver needs no skill points in the Ship Shack"
              : "the Ship Shack's paint table was not found");
}

/* --- each tick ---------------------------------------------------------------------------- */

static void check(void) {
    int32_t map = (int32_t)rd32(A_MAP);
    int32_t paint = (int32_t)rd32(A_PAINT_LOADED);
    int32_t hull = (int32_t)rd32(A_HULL_LOADED);
    uint32_t ship = rd32(A_SHIP);

    /* New Game: the default save has just been loaded and the hull and paint marked stale. */
    if (map == 0 && paint == -1 && hull == -1) {
        if (!g_new_game_done && (ship & 0x1FFFFFu) == 0) {
            wr32(A_SHIP, ship | SHOWROOM_PARTS);
            say("new game: the showroom Star Explorer");
        }
        g_new_game_done = 1;
    } else if (paint != -1 || hull != -1) {
        g_new_game_done = 0;
    }

    if (map != g_last_map) {
        g_last_map = map;
        if (map == LEVEL_SHIP_SHACK && g_api->loaded_level() == LEVEL_SHIP_SHACK) {
            free_the_paint();
        }
        g_icon_done = 0;
        g_icon_tries = 0;
    }
    /* The table is in the level's code: try until it is there (every second, at most a minute). */
    if (map == LEVEL_SHIP_SHACK && !g_icon_done && g_icon_tries < 3600 && g_api->loaded_level() == LEVEL_SHIP_SHACK) {
        if (g_icon_tries % 60 == 0) g_icon_done = silver_the_icon(g_icon_tries != 0);
        g_icon_tries++;
    }
}

static void on_frame(void* user) { (void)user; check(); }
static void on_tick(void* user, uint32_t tick) { (void)user; (void)tick; check(); }

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    g_api = api;
    g_mod = mod;
    if (api->api_major != 1 || api->api_minor < 6) {
        api->fail(mod, "needs OpenRAC's mod interface 1.6 or newer");
        return 1;
    }
    if (!api->game || api->game[0] != 'r' || api->game[3] != '2' ||
        !api->version || api->version[0] != 'n' || api->version[4] != 0) {
        api->fail(mod, "only for Going Commando NTSC-U v1.01 (\"ntsc\")");
        return 1;
    }
    if (OPENRAC_RAC2_HOOK(api, mod, Text_ById, text_by_id, &g_text_original) != OPENRAC_HOOK_OK)
        api->log(mod, OPENRAC_LOG_WARN, "no Text_ById hook point: the paint keeps its old name");
    api->on_frame(mod, on_frame, 0);
    api->on_tick_boundary(mod, on_tick, 0);
    return 0;
}

/* Only for a build without the C runtime (clang -ffreestanding, lld-link /noentry /nodefaultlib);
 * the SDK's CMake build links the runtime, which has its own. */
#ifdef OPENRAC_FREESTANDING
int __stdcall _DllMainCRTStartup(void* h, unsigned reason, void* p) {
    (void)h; (void)reason; (void)p;
    return 1;
}
#endif
