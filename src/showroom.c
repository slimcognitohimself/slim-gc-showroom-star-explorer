/* Showroom Star Explorer: a native OpenRAC mod for Ratchet & Clank: Going Commando, on every disc
 * OpenRAC supports: NTSC-U v1.01 ("ntsc"), NTSC-U v2.00 Greatest Hits ("ntsc2") and PAL ("pal",
 * SCES_516.07). The addresses and text ids are per disc (disc_addrs).
 *
 * 1. Showroom Silver is paint 17, the finished paint no Ship Shack row lists. The Ship Shack lists
 *    it, free (no skill points), right after Blargian Red: on Vendor_Preview the mod adds its entry to
 *    the list the game built, so the game's own preview, purchase and save code handle it. Its name
 *    is the unused text "Pokitaru Purple" (Blargian Red's id + 2), which Text_ById answers with
 *    "Showroom Silver"; its icon colour goes in the Ship Shack's paint colour table's spare row.
 *    The silver itself is assets/level_NN/textures/ship_paint_17.png.
 * 2. Blargian Red (paint 0) is the game's own, free (its skill-point requirement made 0).
 * 3. A new game starts with the showroom Star Explorer: the ship word gets Split Nose 0x1, Hi-Lift
 *    Wings 0x4, Triple Boost Acceleration Engine 0x10, Fusion Laser Cannons 0x40, Mega-Mine Launcher
 *    0x100, Multi-Torpedo Launcher 0x800 and paint 17, right after New Game's reset (the default
 *    save loaded, the loaded hull and paint marked stale with -1), so the flight to Aranos already
 *    shows it. The only other writer of the ship word is the Ship Shack's purchase.
 */
#include <stddef.h>
#include <stdint.h>

#ifndef OPENRAC_MOD_TARGET_MINOR /* the build sets it from the manifest's "api" */
#define OPENRAC_MOD_TARGET_MINOR 34
#endif
#include "openrac/openrac_mod.h"
#include "openrac/games/rac2.h"

OPENRAC_MOD_DEFINE("rac2-showroom-star-explorer", "0.5.0")

/* Per disc: matched in each disc's code against v1.01. Blargian Red's text id differs on each disc
 * (the Ship Shack's colour table, level 24: 0x30BF, 0x2F55, 0xFE2; Orxon Green is id + 1 and the
 * unused Pokitaru Purple id + 2). g_vendorUi is level 24's (its list builder clears 0x760 bytes there). */
typedef struct disc_addrs {
    uint32_t map;          /* s32 current level */
    uint32_t ship;         /* u32 ship word: parts, weapons, paint (bits 16-20) */
    uint32_t hull_loaded;  /* s32 hull model loaded (-1 none) */
    uint32_t paint_loaded; /* s32 paint loaded (-1 none) */
    uint32_t text_red;     /* Blargian Red's name */
    uint32_t vendor_ui;    /* g_vendorUi in level 24 */
} disc_addrs;

static const disc_addrs DISC_NTSC  = {0x1A79F0u, 0x1A7AF0u, 0x152C18u, 0x152C30u, 0x30BF, 0x22F0C0u};
static const disc_addrs DISC_NTSC2 = {0x1A79F8u, 0x1A7AF8u, 0x152C18u, 0x152C30u, 0x2F55, 0x22F040u};
static const disc_addrs DISC_PAL   = {0x1A7A78u, 0x1A7B78u, 0x152C98u, 0x152CB0u, 0x0FE2, 0x22F0C0u};
static const disc_addrs* g_disc = &DISC_NTSC;

#define TEXT_SILVER (g_disc->text_red + 2)
#define PAINT_MASK  0x1F0000u
#define SILVER      (17u << 16)
#define SHOWROOM_PARTS 0x955u /* nose 1, wings 4, 0x10, 0x40, 0x100, 0x800 */
#define LEVEL_SHIP_SHACK 24

/* g_vendorUi: the menu (2: the ship's parts and paints), the cursor, the list (0x1C-byte entries:
 * icon, price, mask, value, extra, name string, name id) and its count. */
#define UI_MENU   0x50u
#define UI_CURSOR 0x80u
#define UI_LIST   0x140u
#define UI_COUNT  0x740u
#define ENTRY     0x1Cu
#define LIST_MAX  ((UI_COUNT - UI_LIST) / ENTRY)

static const openrac_api* g_api;
static openrac_mod* g_mod;
static openrac_rac2_Text_ById g_text_original;
static openrac_rac2_Vendor_Preview g_preview_original;
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

static int in_ship_shack(void) {
    return (int32_t)rd32(g_disc->map) == LEVEL_SHIP_SHACK && g_api->loaded_level() == LEVEL_SHIP_SHACK;
}

/* --- the name ----------------------------------------------------------------------------- */

static int32_t text_by_id(int32_t id) {
    if (id == (int32_t)TEXT_SILVER) {
        if (!g_text_slot) g_text_slot = (uint32_t)g_text_original(OPENRAC_RAC2_TEXT_SLOT + 7);
        if (g_text_slot && g_text_slot < 0x2000000u) {
            uint32_t i;
            for (i = 0; i < sizeof NAME; i++) wr8(g_text_slot + i, (uint8_t)NAME[i]);
            return (int32_t)g_text_slot;
        }
    }
    return g_text_original(id);
}

/* --- the Ship Shack's list ------------------------------------------------------------------ */

/* Adds Showroom Silver's entry after Blargian Red's (or before the first paint), unless it is the
 * paint fitted (the game leaves that one out too) or already there. Entries after it move down
 * one, and so does the cursor when it is on or past it, so it stays on the same entry. */
static void add_silver(void) {
    uint32_t ui = g_disc->vendor_ui, list = ui + UI_LIST;
    uint32_t count = rd32(ui + UI_COUNT), i, k, at, from = 0xFFFFFFFFu, red = 0xFFFFFFFFu;
    uint32_t cursor;

    if ((rd32(g_disc->ship) & PAINT_MASK) == SILVER || count >= LIST_MAX) return;
    for (i = 0; i < count; i++) {
        uint32_t e = list + i * ENTRY;
        if (rd32(e + 8) != PAINT_MASK) continue;
        if (rd32(e + 12) == SILVER) return;
        if (from == 0xFFFFFFFFu) from = i;
        if (rd32(e + 12) == 0) red = i;
    }
    at = red != 0xFFFFFFFFu ? red + 1 : from != 0xFFFFFFFFu ? from : count;
    for (i = count; i > at; i--)
        for (k = 0; k < ENTRY; k += 4) wr32(list + i * ENTRY + k, rd32(list + (i - 1) * ENTRY + k));
    /* icon, price and extra as the other paints have them (5, the same for every paint row) */
    if (from != 0xFFFFFFFFu) {
        uint32_t t = list + (from < at ? from : from + 1) * ENTRY;
        wr32(list + at * ENTRY + 0, rd32(t + 0));
        wr32(list + at * ENTRY + 4, rd32(t + 4));
        wr32(list + at * ENTRY + 16, rd32(t + 16));
    } else {
        wr32(list + at * ENTRY + 0, 0);
        wr32(list + at * ENTRY + 4, 5);
        wr32(list + at * ENTRY + 16, 0);
    }
    wr32(list + at * ENTRY + 8, PAINT_MASK);
    wr32(list + at * ENTRY + 12, SILVER);
    wr32(list + at * ENTRY + 20, (uint32_t)text_by_id((int32_t)TEXT_SILVER));
    wr32(list + at * ENTRY + 24, TEXT_SILVER);
    wr32(ui + UI_COUNT, count + 1);
    cursor = rd32(ui + UI_CURSOR);
    if (cursor >= at && cursor < count) wr32(ui + UI_CURSOR, cursor + 1);
}

/* As the menu opens, as the cursor moves and after each purchase (the list just rebuilt): the
 * entry is in the list before the game reads the one under the cursor. */
static void vendor_preview(void) {
    if (in_ship_shack() && rd32(g_disc->vendor_ui + UI_MENU) == 2) add_silver();
    g_preview_original();
}

/* The shop's skill-point column (36 rows; row 19 is Blargian Red, needing 3). */
static const int32_t SKILL_ROWS[36] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 18,
                                       26, 3, 3, 3, 29, 15, 3, 10, 25, 29, 10, 15, 25, 15, 29, 3, 19, 19};

static void free_the_red(void) {
    uint32_t a;
    int found = 0;
    for (a = 0x100000u; a + sizeof SKILL_ROWS <= 0x2000000u; a += 4) {
        const int32_t* p = (const int32_t*)(g_api->memory + a);
        int k, match = 1;
        if (p[17] != 18 || p[18] != 26) continue;
        for (k = 0; k < 36 && match; k++) match = k == 19 ? p[k] == 3 || p[k] == 0 : p[k] == SKILL_ROWS[k];
        if (match) {
            wr32(a + 19 * 4, 0);
            found++;
        }
    }
    say(found ? "Blargian Red needs no skill points in the Ship Shack"
              : "the Ship Shack's paint table was not found");
}

/* The Ship Shack's paint icons are one grey picture tinted per paint from a table in the level's
 * code: 17 {text id, colour} pairs (Blargian Red, colour 0x00152056, then Orxon Green 0x0012461F),
 * one spare pair {0, 0} that ends it, then another table (first id 1). Silver's colour goes in the
 * spare pair; the lookup then reads on into the next table, whose small ids no paint has, and stops
 * at its own end. */
#define ICON_RED    0x00152056u
#define ICON_SILVER 0x007A7670u /* R 0x70, G 0x76, B 0x7A */

static int silver_icon(int quiet) {
    uint32_t a;
    int found = 0;
    for (a = 0x100000u; a + 18 * 8 + 4 <= 0x2000000u; a += 4) {
        const uint32_t* p = (const uint32_t*)(g_api->memory + a);
        if (p[0] != g_disc->text_red || p[1] != ICON_RED || p[2] != p[0] + 1 || p[3] != 0x0012461Fu) continue;
        if ((p[34] != 0 && p[34] != TEXT_SILVER) || p[36] != 1) continue;
        wr32(a + 17 * 8, TEXT_SILVER);
        wr32(a + 17 * 8 + 4, ICON_SILVER);
        found++;
    }
    if (found) say("Showroom Silver's Ship Shack icon is silver");
    else if (!quiet) say("the Ship Shack's paint icon colours were not found yet");
    return found;
}

/* --- each tick ---------------------------------------------------------------------------- */

static void check(void) {
    int32_t map = (int32_t)rd32(g_disc->map);
    int32_t paint = (int32_t)rd32(g_disc->paint_loaded);
    int32_t hull = (int32_t)rd32(g_disc->hull_loaded);
    uint32_t ship = rd32(g_disc->ship);

    /* New Game: the default save has just been loaded and the hull and paint marked stale. */
    if (map == 0 && paint == -1 && hull == -1) {
        if (!g_new_game_done && (ship & 0x1FFFFFu) == 0) {
            wr32(g_disc->ship, ship | SHOWROOM_PARTS | SILVER);
            say("new game: the showroom Star Explorer");
        }
        g_new_game_done = 1;
    } else if (paint != -1 || hull != -1) {
        g_new_game_done = 0;
    }

    if (map != g_last_map) {
        g_last_map = map;
        if (in_ship_shack()) free_the_red();
        g_icon_done = 0;
        g_icon_tries = 0;
    }
    /* The colour table is in the level's code: try until it is there (every second, at most a minute). */
    if (!g_icon_done && g_icon_tries < 3600 && in_ship_shack()) {
        if (g_icon_tries % 60 == 0) g_icon_done = silver_icon(g_icon_tries != 0);
        g_icon_tries++;
    }
}

static void on_frame(void* user) { (void)user; check(); }
static void on_tick(void* user, uint32_t tick) { (void)user; (void)tick; check(); }

static int same(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    g_api = api;
    g_mod = mod;
    if (api->api_major != 1 || api->api_minor < 34) {
        api->fail(mod, "needs OpenRAC's mod interface 1.34 or newer");
        return 1;
    }
    if (!api->game || !same(api->game, "rac2") || !api->version) {
        api->fail(mod, "only for Ratchet & Clank: Going Commando");
        return 1;
    }
    if (same(api->version, "ntsc")) g_disc = &DISC_NTSC;
    else if (same(api->version, "ntsc2")) g_disc = &DISC_NTSC2;
    else if (same(api->version, "pal")) g_disc = &DISC_PAL;
    else {
        api->fail(mod, "unknown Going Commando disc (needs \"ntsc\", \"ntsc2\" or \"pal\")");
        return 1;
    }
    say(g_disc == &DISC_PAL ? "PAL disc (SCES_516.07)"
        : g_disc == &DISC_NTSC2 ? "NTSC-U Greatest Hits disc (v2.00)" : "NTSC-U disc (v1.01)");
    if (OPENRAC_RAC2_HOOK(api, mod, Text_ById, text_by_id, &g_text_original) != OPENRAC_HOOK_OK)
        api->log(mod, OPENRAC_LOG_WARN, "no Text_ById hook point: Showroom Silver has no name");
    if (OPENRAC_RAC2_HOOK(api, mod, Vendor_Preview, vendor_preview, &g_preview_original) != OPENRAC_HOOK_OK)
        api->log(mod, OPENRAC_LOG_WARN, "no Vendor_Preview hook point: the Ship Shack doesn't list Showroom Silver");
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
