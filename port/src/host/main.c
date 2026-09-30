/* maniac-mansion-port: SDL2 host for the Stage 1 NES core.
 *
 * Runs the user's own Maniac Mansion (USA) ROM, verified by SHA-256.
 * Defaults are the original game as it ran on an NTSC console: 8:7 pixel
 * aspect at an integer scale, no filters, no enhancements. */
#include <SDL.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../core/cart.h"
#include "../core/nes.h"
#include "../core/trace.h"
#include "config.h"
#include "presentation.h"
#include "qol_input.h"
#include "mode.h"
#include "party.h"
#include "enhanced_save.h"
#include "devmenu.h"
#include "pads.h"
#include "chord.h"
#include "palette.h"
#include "png.h"

#ifndef MM_VERSION
#define MM_VERSION "dev"
#endif

#define NTSC_FPS (1789772.7272 * 3.0 / (341.0 * 262.0 - 0.5))

static FILE *log_file;

static void logline(const char *s)
{
    fprintf(stderr, "%s\n", s);
    if (log_file) {
        fprintf(log_file, "%s\n", s);
        fflush(log_file);
    }
}

static void logf_(const char *fmt, ...)
{
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    logline(b);
}

/* ---------------------------------------------------------------- files */

static bool write_atomic(const char *path, const void *data, size_t n)
{
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f)
        return false;
    bool ok = fwrite(data, 1, n, f) == n;
    ok = (fclose(f) == 0) && ok;
    return ok && rename(tmp, path) == 0;
}

static char *read_small(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;
    static char b[1024];
    size_t n = fread(b, 1, sizeof b - 1, f);
    fclose(f);
    b[n] = 0;
    char *nl = strchr(b, '\n');
    if (nl)
        *nl = 0;
    return b;
}

/* ------------------------------------------------------------------ ROM */

static const char *remembered_rom_file(void)
{
    static char p[1100];
    snprintf(p, sizeof p, "%s/rom-path", path_data_dir());
    return p;
}

static void remember_rom(const char *path)
{
    char *abs = realpath(path, NULL);
    if (!abs)
        return;
    path_mkdirs(path_data_dir());
    char line[1100];
    int n = snprintf(line, sizeof line, "%s\n", abs);
    write_atomic(remembered_rom_file(), line, (size_t)n);
    free(abs);
}

/* Candidates in order: --rom, MM_ROM / [game] rom, remembered, data dir. */
static bool find_rom(config_t *cfg, const char *cli_rom, cart_t *cart, char *err, size_t errlen, char *used,
                     size_t usedlen, cart_status_t *status, bool *explicit_out)
{
    const char *cands[4];
    int n = 0;
    if (cli_rom)
        cands[n++] = cli_rom;
    const char *c = getenv("MM_ROM");
    if (!c)
        c = config_str(cfg, "game", "rom", NULL);
    if (c && *c)
        cands[n++] = c;
    char *r = read_small(remembered_rom_file());
    static char remembered[1100], local[1100];
    if (r && *r) {
        snprintf(remembered, sizeof remembered, "%s", r);
        cands[n++] = remembered;
    }
    snprintf(local, sizeof local, "%s/rom.nes", path_data_dir());
    cands[n++] = local;

    err[0] = 0;
    *explicit_out = cli_rom || (c && *c);
    for (int i = 0; i < n; i++) {
        bool explicit_choice = i == 0 && *explicit_out;
        FILE *probe = fopen(cands[i], "rb");
        if (!probe && !explicit_choice)
            continue;
        if (probe)
            fclose(probe);
        if (cart_load_status(cart, cands[i], false, err, errlen, status)) {
            snprintf(used, usedlen, "%s", cands[i]);
            return true;
        }
        if (explicit_choice)
            return false; /* the user asked for this file: report why */
    }
    if (!err[0]) {
        snprintf(err, errlen, "no Maniac Mansion (USA) ROM found");
        *status = CART_ERR_MISSING;
    }
    return false;
}

/* ---------------------------------------------------------------- saves */

typedef struct {
    char path[1100], metadata_path[1120];
    uint8_t last[8192], metadata_last[PARTY_HISTORY_BYTES];
    size_t metadata_last_size;
    bool enhanced;
    uint32_t last_write_ms;
} save_t;

static bool save_open(save_t *s, nes_t *nes, mm_mode_t mode, party_state_t *party)
{
    char dir[1100], quick[1100];
    if (!mode_save_paths(path_data_dir(), mode, s->path, sizeof s->path, quick, sizeof quick)) {
        logline("error: save path too long");
        return false;
    }
    snprintf(dir, sizeof dir, "%s", s->path);
    *strrchr(dir, '/') = 0;
    path_mkdirs(dir);
    FILE *f = fopen(s->path, "rb");
    if (f) {
        size_t n = fread(nes->prgram, 1, sizeof nes->prgram, f);
        fclose(f);
        logf_("battery save: loaded %s (%zu bytes)", s->path, n);
    } else {
        logf_("battery save: none yet (%s)", s->path);
    }
    memcpy(s->last, nes->prgram, sizeof s->last);
    s->enhanced = mode == MM_MODE_ENHANCED;
    if (s->enhanced) {
        snprintf(s->metadata_path, sizeof s->metadata_path, "%s.party", s->path);
        uint8_t metadata[PARTY_HISTORY_BYTES]; size_t size = 0;
        if (enhanced_battery_metadata_read(s->metadata_path, nes->prgram, metadata, sizeof metadata, &size) &&
            party_restore_history(party, metadata, size)) {
            memcpy(s->metadata_last, metadata, size); s->metadata_last_size = size;
        }
    }
    return true;
}

static void save_flush(save_t *s, nes_t *nes, bool force, party_state_t *party)
{
    if (s->enhanced && (party->pending || party->executing)) return;
    uint32_t now = SDL_GetTicks();
    if (!force && now - s->last_write_ms < 2000) return;
    s->last_write_ms = now;
    bool changed = memcmp(s->last, nes->prgram, sizeof s->last) != 0;
    uint8_t metadata[PARTY_HISTORY_BYTES];
    size_t size = s->enhanced && party_history_encode(&party->history, metadata) ? sizeof metadata : 0;
    bool history_changed = s->enhanced && (size != s->metadata_last_size ||
                           (size && memcmp(metadata, s->metadata_last, size)));
    if (!changed && !history_changed) return;
    if (changed) {
        if (!write_atomic(s->path, nes->prgram, sizeof nes->prgram)) {
            logf_("battery save: cannot write %s: %s", s->path, strerror(errno));
            return;
        }
        memcpy(s->last, nes->prgram, sizeof s->last);
    }
    if (s->enhanced) {
        if (enhanced_battery_metadata_write(s->metadata_path, nes->prgram, metadata, size)) {
            if (size) memcpy(s->metadata_last, metadata, size);
            s->metadata_last_size = size;
        } else {
            /* This generated sidecar must not remain trusted with stale history. */
            unlink(s->metadata_path);
            s->metadata_last_size = 0;
            logline("enhanced party history: cannot persist; next load will require verified history");
        }
    }
}

/* ---------------------------------------------------------------- input */

typedef struct {
    SDL_Scancode key[8]; /* A B SELECT START UP DOWN LEFT RIGHT */
} keymap_t;

static const char *btn_names[8] = {"a", "b", "select", "start", "up", "down", "left", "right"};

static void keymap_load(keymap_t *km, config_t *cfg)
{
    static const char *defaults[8] = {"X", "Z", "Right Shift", "Return", "Up", "Down", "Left", "Right"};
    for (int i = 0; i < 8; i++) {
        const char *name = config_str(cfg, "input", btn_names[i], defaults[i]);
        SDL_Scancode sc = SDL_GetScancodeFromName(name);
        if (sc == SDL_SCANCODE_UNKNOWN) {
            logf_("config: unknown key '%s' for input.%s; using %s", name, btn_names[i], defaults[i]);
            sc = SDL_GetScancodeFromName(defaults[i]);
        }
        km->key[i] = sc;
    }
}

typedef struct {
    SDL_GameController *gc[PADS_MAX];
    SDL_Joystick *js[PADS_MAX]; /* raw fallback: set (and gc NULL) for pads without a mapping */
    int32_t id[PADS_MAX];
    int n;
    pad_set_t set;
    bool back_now, start_now;   /* Back/Start OR-ed over every attached pad this frame (chord input) */
    uint32_t trace_window_ms;
    int trace_lines;
} pads_t;

static uint8_t pad_buttons(SDL_GameController *gc)
{
    uint8_t b = 0;
    const int T = 16000;
    int lx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
    int ly = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A)) b |= PAD_A;
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_B)) b |= PAD_B;
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_X)) b |= PAD_B;
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK)) b |= PAD_SELECT;
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_START)) b |= PAD_START;
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP) || ly < -T) b |= PAD_UP;
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ly > T) b |= PAD_DOWN;
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || lx < -T) b |= PAD_LEFT;
    if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx > T) b |= PAD_RIGHT;
    /* Opposing directions cannot be pressed together on a real pad. */
    if ((b & (PAD_UP | PAD_DOWN)) == (PAD_UP | PAD_DOWN)) b &= ~(PAD_UP | PAD_DOWN);
    if ((b & (PAD_LEFT | PAD_RIGHT)) == (PAD_LEFT | PAD_RIGHT)) b &= ~(PAD_LEFT | PAD_RIGHT);
    return b;
}

/* Raw-joystick read (pads without an SDL controller mapping): xpad layout. */
static uint8_t raw_buttons(SDL_Joystick *js, bool *back, bool *start, bool *l3, bool *r3)
{
    uint32_t bt = 0;
    int nb = SDL_JoystickNumButtons(js);
    for (int i = 0; i < nb && i < 32; i++)
        if (SDL_JoystickGetButton(js, i))
            bt |= 1u << i;
    uint8_t hat = SDL_JoystickNumHats(js) > 0 ? SDL_JoystickGetHat(js, 0) : 0;
    int ax = SDL_JoystickNumAxes(js) > 0 ? SDL_JoystickGetAxis(js, 0) : 0;
    int ay = SDL_JoystickNumAxes(js) > 1 ? SDL_JoystickGetAxis(js, 1) : 0;
    return pads_raw_layout(bt, hat, ax, ay, back, start, l3, r3);
}

static void pads_open(pads_t *p, int device_index)
{
    if (p->n == PADS_MAX)
        return;
    SDL_GameController *gc = NULL;
    SDL_Joystick *js = NULL;
    if (SDL_IsGameController(device_index)) {
        gc = SDL_GameControllerOpen(device_index);
        if (!gc)
            return;
        js = SDL_GameControllerGetJoystick(gc);
    } else {
        js = SDL_JoystickOpen(device_index); /* raw fallback */
        if (!js)
            return;
        if (SDL_JoystickNumButtons(js) < 8) { /* not a gamepad-shaped device */
            SDL_JoystickClose(js);
            return;
        }
    }
    int32_t id = SDL_JoystickInstanceID(js);
    for (int i = 0; i < p->n; i++)
        if (p->id[i] == id) {
            if (gc) SDL_GameControllerClose(gc); else SDL_JoystickClose(js);
            return;
        }
    p->gc[p->n] = gc;
    p->js[p->n] = gc ? NULL : js;
    p->id[p->n++] = id;
    const char *name = gc ? SDL_GameControllerName(gc) : SDL_JoystickName(js);
    uint16_t vid = SDL_JoystickGetVendor(js), pid = SDL_JoystickGetProduct(js);
    bool held = gc ? pad_buttons(gc) != 0 : raw_buttons(js, NULL, NULL, NULL, NULL) != 0;
    pads_added(&p->set, id, vid, pid, name, held);
    logf_("[DEV_MENU] pad connected: %s vid:pid=%04x:%04x instance=%d%s%s", name ? name : "?", vid, pid, (int)id,
          pads_is_steam_virtual(vid, pid, name) ? " (steam virtual)" : "", gc ? "" : " (raw joystick, xpad layout)");
}

static void pads_close(pads_t *p, int32_t id)
{
    for (int i = 0; i < p->n; i++)
        if (p->id[i] == id) {
            if (p->gc[i]) SDL_GameControllerClose(p->gc[i]); else if (p->js[i]) SDL_JoystickClose(p->js[i]);
            p->gc[i] = p->gc[--p->n];
            p->js[i] = p->js[p->n];
            p->id[i] = p->id[p->n];
            pads_removed(&p->set, id);
            logf_("[DEV_MENU] pad removed: instance=%d", (int)id);
            return;
        }
}

/* Game pad state from the active controller (§4a selection). */
static uint8_t pads_poll(pads_t *p, uint8_t *hot)
{
    uint8_t out = 0;
    *hot = 0;
    uint32_t now = SDL_GetTicks();
    if (now - p->trace_window_ms >= 1000) {
        p->trace_window_ms = now;
        p->trace_lines = 0;
    }
    p->back_now = p->start_now = false;
    for (int i = 0; i < p->n; i++) {
        uint8_t b;
        bool l3, r3, back, start;
        if (p->gc[i]) {
            b = pad_buttons(p->gc[i]);
            l3 = SDL_GameControllerGetButton(p->gc[i], SDL_CONTROLLER_BUTTON_LEFTSTICK);
            r3 = SDL_GameControllerGetButton(p->gc[i], SDL_CONTROLLER_BUTTON_RIGHTSTICK);
            back = SDL_GameControllerGetButton(p->gc[i], SDL_CONTROLLER_BUTTON_BACK);
            start = SDL_GameControllerGetButton(p->gc[i], SDL_CONTROLLER_BUTTON_START);
        } else {
            b = raw_buttons(p->js[i], &back, &start, &l3, &r3);
        }
        bool non_neutral = b || l3 || r3;
        if (non_neutral && p->trace_lines < 3) {
            p->trace_lines++;
            logf_("[DEV_MENU] input instance=%d buttons=%02x l3=%d r3=%d", (int)p->id[i], b, l3, r3);
        }
        /* The Back+Start chord is a host gesture: read it from EVERY attached pad,
         * outside the active-pad selection and its neutral gating, so a mirrored
         * Steam/InputPlumber copy that appears mid-press, or a pad whose first press
         * is still being adjudicated, still opens the menu. Game input stays with
         * the active pad only. */
        p->back_now |= back;
        p->start_now |= start;
        if (pads_input(&p->set, p->id[i], non_neutral)) {
            out = b;
            *hot = (uint8_t)((l3 ? 1 : 0) | (r3 ? 2 : 0));
        }
    }
    return out;
}

static uint8_t boot_pad_poll(void *userdata)
{
    uint8_t hot;
    return pads_poll(userdata, &hot);
}

static void boot_device_event(void *userdata, const SDL_Event *event)
{
    pads_t *pads = userdata;
    if (event->type == SDL_CONTROLLERDEVICEADDED || event->type == SDL_JOYDEVICEADDED)
        pads_open(pads, event->type == SDL_CONTROLLERDEVICEADDED ? event->cdevice.which : event->jdevice.which);
    else if (event->type == SDL_CONTROLLERDEVICEREMOVED || event->type == SDL_JOYDEVICEREMOVED)
        pads_close(pads, event->type == SDL_CONTROLLERDEVICEREMOVED ? event->cdevice.which : event->jdevice.which);
}

/* ---------------------------------------------------------------- video */

typedef struct {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *frame;  /* 256x240, nearest */
    SDL_Texture *scaled; /* integer prescale, then linear to the window */
    int prescale;
    bool par87;
    bool graphics_sample;
    qol_native_t *qol;
    devmenu_t *qol_overlay;
    int crop; /* lines hidden at top and bottom */
    char window_shot[1024]; /* pending capture of the next presented frame */
    FILE *record;           /* --record-video: raw RGB24 of every presented frame */
    FILE *record_idx;       /* <video>.idx: game frame number of each video frame */
    long game_frame;
    const char *caption_top, *caption_bottom;
    uint32_t lut[512];
} video_t;

static void video_lut(video_t *v)
{
    for (int i = 0; i < 512; i++)
        v->lut[i] = 0xFF000000u | nes_rgb((uint16_t)i);
}

static void video_present(video_t *v, const uint16_t *fb, devmenu_t *dm)
{
    void *pixels;
    int pitch;
    if (SDL_LockTexture(v->frame, NULL, &pixels, &pitch) == 0) {
        for (int y = 0; y < NES_H; y++) {
            uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
            const uint16_t *src = fb + y * NES_W;
            for (int x = 0; x < NES_W; x++)
                row[x] = v->graphics_sample ? presentation_pixel(v->lut[src[x] & 0x1FF], true, y)
                                          : v->lut[src[x] & 0x1FF];
        }
        SDL_UnlockTexture(v->frame);
    }
    int ww, wh;
    SDL_GetRendererOutputSize(v->ren, &ww, &wh);
    int vis_h = NES_H - 2 * v->crop;
    double par = v->par87 ? 8.0 / 7.0 : 1.0;
    int scale = wh / vis_h;
    while (scale > 1 && (int)(NES_W * par * scale + 0.5) > ww)
        scale--;
    if (scale < 1)
        scale = 1;
    SDL_Rect dst;
    dst.h = vis_h * scale;
    dst.w = (int)(NES_W * par * scale + 0.5);
    if (dst.w > ww) { /* window smaller than 1x: fit */
        dst.w = ww;
        dst.h = (int)(ww / (NES_W * par) * vis_h);
    }
    dst.x = (ww - dst.w) / 2;
    dst.y = (wh - dst.h) / 2;
    SDL_Rect src = {0, v->crop, NES_W, vis_h};

    SDL_SetRenderDrawColor(v->ren, 0, 0, 0, 255);
    SDL_RenderClear(v->ren);
    if (v->scaled && v->par87) {
        /* Sharp 8:7: nearest integer prescale, linear only for the
         * fractional horizontal stretch. */
        SDL_SetRenderTarget(v->ren, v->scaled);
        SDL_Rect pre = {0, 0, NES_W * v->prescale, vis_h * v->prescale};
        SDL_RenderCopy(v->ren, v->frame, &src, &pre);
        SDL_SetRenderTarget(v->ren, NULL);
        SDL_RenderCopy(v->ren, v->scaled, &pre, &dst);
    } else {
        SDL_RenderCopy(v->ren, v->frame, &src, &dst);
    }
    if (v->qol && v->qol_overlay)
        devmenu_qol_draw(v->qol_overlay, v->ren, dst, v->crop,
                         v->qol->policy.chooser, qol_verb_name((qol_verb_t)v->qol->policy.verb_index),
                         v->qol->highlight, v->qol->selected.x, v->qol->selected.y);
    if (dm)
        devmenu_draw(dm, v->ren, dst); /* host overlay on the presented copy */
    if (dm && (v->caption_top || v->caption_bottom)) { /* recordings: in the letterbox bars */
        int sc = dst.y >= 40 ? 3 : 2;
        devmenu_caption(dm, v->ren, (SDL_Rect){0, 0, ww, dst.y}, sc, v->caption_top);
        devmenu_caption(dm, v->ren, (SDL_Rect){0, dst.y + dst.h, ww, wh - dst.y - dst.h}, 2, v->caption_bottom);
    }
    if (v->record) {
        static uint8_t *rgb;
        static size_t cap;
        size_t need = (size_t)ww * wh * 3;
        if (need > cap) {
            rgb = realloc(rgb, need);
            cap = need;
        }
        if (SDL_RenderReadPixels(v->ren, NULL, SDL_PIXELFORMAT_RGB24, rgb, ww * 3) == 0) {
            fwrite(rgb, 1, need, v->record);
            if (v->record_idx) /* video frame -> game frame, for editing */
                fprintf(v->record_idx, "%ld\n", v->game_frame);
        }
    }
    if (v->window_shot[0]) { /* tests: capture exactly what is presented */
        int w, h;
        SDL_GetRendererOutputSize(v->ren, &w, &h);
        uint8_t *rgb = malloc((size_t)w * h * 3);
        if (rgb && SDL_RenderReadPixels(v->ren, NULL, SDL_PIXELFORMAT_RGB24, rgb, w * 3) == 0)
            logf_(png_write_rgb(v->window_shot, rgb, w, h) ? "window shot: %s" : "window shot failed: %s", v->window_shot);
        free(rgb);
        v->window_shot[0] = 0;
    }
    SDL_RenderPresent(v->ren);
}

/* ------------------------------------------------------------- helpers */

static void take_screenshot(const uint16_t *fb, devmenu_t *dm)
{
    char dir[1100], p[1200];
    snprintf(dir, sizeof dir, "%s/screenshots", path_state_dir());
    path_mkdirs(dir);
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(p, sizeof p, "%s/mm-%04d%02d%02d-%02d%02d%02d.png", dir, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    bool ok = png_write_nes(p, fb);
    logf_(ok ? "screenshot: %s" : "screenshot failed: %s", p);
    if (dm)
        devmenu_toast(dm, ok ? "SCREENSHOT SAVED" : "SCREENSHOT FAILED");
}

static const char *quick_slot(mm_mode_t mode)
{
    static char p[1100];
    char dir[1100], battery[1100];
    if (!mode_save_paths(path_data_dir(), mode, battery, sizeof battery, p, sizeof p)) {
        p[0] = 0;
        return p;
    }
    snprintf(dir, sizeof dir, "%s", p);
    *strrchr(dir, '/') = 0;
    path_mkdirs(dir);
    return p;
}

/* Quick save/load are whole-machine snapshots in a host-side slot; the
 * game's own battery save is untouched. */
static void quick_save(nes_t *nes, const uint8_t id[8], devmenu_t *dm, mm_mode_t mode,
                       party_state_t *party)
{
    if (mode == MM_MODE_ENHANCED && (party->pending || party->executing)) {
        if (dm) devmenu_toast(dm, "WAIT FOR TEAMMATE SWAP");
        return;
    }
    bool ok;
    if (mode == MM_MODE_ENHANCED) {
        uint8_t metadata[PARTY_HISTORY_BYTES];
        size_t size = party_history_encode(&party->history, metadata) ? sizeof metadata : 0;
        ok = enhanced_snapshot_save(quick_slot(mode), id, nes, metadata, size);
    } else {
        static uint8_t buf[sizeof(nes_t) + 64];
        nes_state_save(nes, buf, id);
        ok = write_atomic(quick_slot(mode), buf, nes_state_size());
    }
    logf_("[DEV_MENU] quick save %s: %s", ok ? "done" : "FAILED", quick_slot(mode));
    if (dm) devmenu_toast(dm, ok ? "QUICK SAVE DONE" : "QUICK SAVE FAILED");
}

static void quick_load(nes_t *nes, const uint8_t id[8], devmenu_t *dm, mm_mode_t mode,
                       party_state_t *party)
{
    if (mode == MM_MODE_ENHANCED && (party->pending || party->executing)) {
        if (dm) devmenu_toast(dm, "WAIT FOR TEAMMATE SWAP");
        return;
    }
    const char *why = NULL;
    if (mode == MM_MODE_ENHANCED) {
        uint8_t metadata[PARTY_HISTORY_BYTES]; size_t size = 0;
        enhanced_load_result_t result = enhanced_snapshot_load(quick_slot(mode), id, nes,
                                                               metadata, sizeof metadata, &size);
        if (result == ENHANCED_LOAD_FAILED) why = "QUICK SAVE INVALID OR MISSING";
        else {
            party_destroy(party); party_init(party);
            if (result == ENHANCED_LOAD_VERIFIED) party_restore_history(party, metadata, size);
            /* Legacy/missing history remains unknown; it must never issue new items. */
        }
    } else {
        static uint8_t buf[sizeof(nes_t) + 64];
        FILE *f = fopen(quick_slot(mode), "rb");
        size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
        if (f) fclose(f);
        why = !f ? "NO QUICK SAVE YET" : nes_state_load(nes, buf, n, id) ? NULL : "QUICK SAVE IS FROM ANOTHER MODE/BUILD";
    }
    logf_("[DEV_MENU] quick load %s", why ? why : "done");
    if (dm) devmenu_toast(dm, "%s", why ? why : "QUICK LOAD DONE");
}

/* Enhanced dispatcher owns the core callback and composes the optional native
 * input adapter with the party observer/transaction. Original never binds it. */
typedef struct {
    void (*input_hook)(nes_t *, void *);
    void *input_userdata;
    party_state_t *party;
} enhancement_dispatch_t;
static void enhancement_dispatch(nes_t *nes, void *userdata)
{
    enhancement_dispatch_t *dispatch = userdata;
    if (dispatch->input_hook) dispatch->input_hook(nes, dispatch->input_userdata);
    party_exec(nes, dispatch->party);
}

/* Harness-only scripted dev keys (spec §9): MM_DEV_KEYS="frame:KEY,...",
 * KEY = F8 UP DOWN OK BACK F1 F10 F11 F12. Inert unless set. */
typedef struct {
    long frame;
    char key[8];
} devkey_t;

static int parse_devkeys(devkey_t *out, int max)
{
    const char *s = getenv("MM_DEV_KEYS");
    int n = 0;
    while (s && *s && n < max) {
        long fr;
        char k[8];
        int used = 0;
        if (sscanf(s, "%ld:%7[A-Z0-9]%n", &fr, k, &used) != 2)
            break;
        out[n].frame = fr;
        snprintf(out[n].key, sizeof out[n].key, "%s", k);
        n++;
        s += used;
        if (*s == ',')
            s++;
    }
    return n;
}

/* ----------------------------------------------------------------- main */

static void usage(void)
{
    printf("maniac-mansion-port %s\n"
           "usage: maniac-mansion-port [--rom FILE] [--mode original|enhanced] [--config FILE] [--set section.key=value]... [--check-rom]\n"
           "\n"
           "Runs your own Maniac Mansion (USA) NES ROM (checked by SHA-256). The ROM is\n"
           "looked for in: --rom, $MM_ROM, [game] rom in config.ini, the last ROM used,\n"
           "then %s/rom.nes.\n"
           "\n"
           "Keys: arrows = D-pad, X = A, Z = B, Enter = Start, Right Shift = Select,\n"
           "      F6 = fast-forward on/off, Backspace (hold) = fast-forward, F10 = screenshot,\n"
           "      Alt+Enter = fullscreen, Esc = quit, F8 = dev menu. Controllers: A = A, B/X = B,\n"
           "      Back = Select, Start = Start, L3 = fast-forward on/off, R3 (hold) = fast-forward.\n",
           MM_VERSION, path_data_dir());
}

static void fatal_box(const char *title, const char *msg)
{
    logf_("error: %s", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg, NULL);
}

int main(int argc, char **argv)
{
    const char *cli_rom = NULL, *cfg_path = NULL, *trace_path = NULL, *shot_path = NULL, *hash_path = NULL;
    long max_frames = -1, shot_frame = -1;
    struct { long frame; char path[1024]; } wshots[16];
    const char *rec_video = NULL, *rec_audio = NULL, *captions_path = NULL;
    int nwshots = 0;
    bool check_only = false;
    mm_mode_t mode = MM_MODE_ORIGINAL;
    bool mode_explicit = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc)
            cli_rom = argv[++i];
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
            if (!mode_parse(argv[++i], &mode)) {
                fprintf(stderr, "error: --mode requires original or enhanced\n");
                return 2;
            }
            mode_explicit = true;
        } else if (!strcmp(argv[i], "--config") && i + 1 < argc)
            cfg_path = argv[++i];
        else if (!strcmp(argv[i], "--set") && i + 1 < argc)
            i++; /* applied after the config file is loaded */
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage();
            return 0;
        } else if (!strcmp(argv[i], "--trace") && i + 1 < argc) {
            trace_path = argv[++i]; /* replay a .mmin (tests, demos) */
        } else if (!strcmp(argv[i], "--hashes") && i + 1 < argc) {
            hash_path = argv[++i]; /* per-frame mmfb1 hashes (tests) */
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            max_frames = strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
            char *colon; /* FRAME:PATH */
            shot_frame = strtol(argv[++i], &colon, 10);
            shot_path = *colon == ':' ? colon + 1 : NULL;
        } else if (!strcmp(argv[i], "--window-shot") && i + 1 < argc && nwshots < 16) {
            char *colon; /* FRAME:PATH, the presented window including overlays */
            wshots[nwshots].frame = strtol(argv[++i], &colon, 10);
            if (*colon == ':')
                snprintf(wshots[nwshots++].path, sizeof wshots[0].path, "%s", colon + 1);
        } else if (!strcmp(argv[i], "--record-video") && i + 1 < argc) {
            rec_video = argv[++i]; /* raw RGB24 frames, one per presented frame (demos) */
        } else if (!strcmp(argv[i], "--record-audio") && i + 1 < argc) {
            rec_audio = argv[++i]; /* 48 kHz mono s16 WAV at game time */
        } else if (!strcmp(argv[i], "--captions") && i + 1 < argc) {
            captions_path = argv[++i]; /* "FRAME top|bottom TEXT" lines */
        } else if (!strcmp(argv[i], "--check-rom")) {
            check_only = true;
        } else if (!strcmp(argv[i], "--version")) {
            printf("maniac-mansion-port %s\n", MM_VERSION);
            return 0;
        } else if (argv[i][0] != '-' && !cli_rom) {
            cli_rom = argv[i]; /* allow a bare ROM path (drag onto launcher) */
        } else {
            fprintf(stderr, "unknown option '%s' (see --help)\n", argv[i]);
            return 2;
        }
    }

    path_mkdirs(path_state_dir());
    char logp[1100];
    snprintf(logp, sizeof logp, "%s/port.log", path_state_dir());
    log_file = fopen(logp, "w");
    logf_("maniac-mansion-port %s", MM_VERSION);

    char err[768];
    config_t *cfg = config_load(cfg_path, err, sizeof err);
    if (!cfg) {
        fprintf(stderr, "error: %s\n", err);
        return 2;
    }
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--set") && i + 1 < argc) {
            char kv[600];
            snprintf(kv, sizeof kv, "%s", argv[++i]);
            char *eq = strchr(kv, '=');
            if (!eq) {
                fprintf(stderr, "error: --set expects section.key=value, got '%s'\n", kv);
                return 2;
            }
            *eq = 0;
            config_set(cfg, kv, eq + 1);
        }

    cart_t cart;
    char used[1100];
    cart_status_t rom_status = CART_OK;
    bool rom_explicit = false;
    bool have_rom = find_rom(cfg, cli_rom, &cart, err, sizeof err, used, sizeof used, &rom_status, &rom_explicit);
    if (check_only) {
        /* Machine-readable result for launchers; no window, no SDL. */
        if (have_rom) {
            printf("MM_ROM_OK path=%s sha256=%s\n", used, cart.sha256);
            return 0;
        }
        printf("MM_ROM_ERROR code=%d %s\n", rom_status, err);
        return rom_status;
    }

    /* §4a: un-hide physical pads before any SDL init. */
    pads_fix_environment(logline);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#ifdef SDL_HINT_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD, "1"); /* see pads_fix_environment */
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "error: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    video_t v = {0};
    int scale = config_int(cfg, "video", "scale", 3);
    v.par87 = strcmp(config_str(cfg, "video", "aspect", "8:7"), "square") != 0;
    v.crop = config_int(cfg, "video", "overscan_crop", 0);
    if (v.crop < 0 || v.crop > 16)
        v.crop = 0;
    bool fullscreen = config_bool(cfg, "video", "fullscreen", false);
    bool vsync = config_bool(cfg, "video", "vsync", true);
    int win_w = (int)(NES_W * (v.par87 ? 8.0 / 7.0 : 1.0) * scale + 0.5), win_h = (NES_H - 2 * v.crop) * scale;
    const char *wsize = config_str(cfg, "video", "window", NULL); /* e.g. 1920x1080 */
    if (wsize)
        sscanf(wsize, "%dx%d", &win_w, &win_h);
    v.win = SDL_CreateWindow("Maniac Mansion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h,
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                                 (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!v.win) {
        fprintf(stderr, "error: cannot open a window: %s\n", SDL_GetError());
        return 1;
    }
    v.ren = SDL_CreateRenderer(v.win, -1, SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!v.ren)
        v.ren = SDL_CreateRenderer(v.win, -1, 0);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    v.frame = SDL_CreateTexture(v.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NES_W, NES_H);
    v.prescale = 4;
    v.scaled = SDL_CreateTexture(v.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, NES_W * v.prescale,
                                 NES_H * v.prescale);
    if (v.scaled)
        SDL_SetTextureScaleMode(v.scaled, SDL_ScaleModeLinear);
    video_lut(&v);

    /* ROM: verified by hash. A ROM the user named that fails the check is
     * reported and the port exits with its error class; with no ROM at
     * all, the window waits for one to be dropped on it. */
    if (!have_rom && rom_explicit) {
        char msg[1200];
        snprintf(msg, sizeof msg, "%s.\n\nThis port runs your own Maniac Mansion (USA) NES ROM.", err);
        fprintf(stderr, "MM_ROM_ERROR code=%d %s\n", rom_status, err);
        fatal_box("Maniac Mansion: ROM problem", msg);
        return rom_status;
    }
    if (!have_rom) {
        fprintf(stderr, "MM_ROM_ERROR code=%d %s\n", rom_status, err);
        char msg[1400];
        snprintf(msg, sizeof msg,
                 "%s.\n\nThis port needs your own Maniac Mansion (USA) NES ROM (262,160 bytes, No-Intro).\n"
                 "Drop the .nes file onto the game window, or start with:\n"
                 "  maniac-mansion-port --rom \"/path/to/Maniac Mansion (USA).nes\"\n"
                 "It is remembered for next time.",
                 err);
        fatal_box("Maniac Mansion: ROM needed", msg);
        SDL_SetWindowTitle(v.win, "Maniac Mansion: drop your Maniac Mansion (USA) .nes ROM on this window");
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
        while (!have_rom) {
            SDL_Event e;
            if (!SDL_WaitEvent(&e))
                continue;
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                return CART_ERR_MISSING;
            if (e.type == SDL_DROPFILE) {
                if (cart_load(&cart, e.drop.file, false, err, sizeof err)) {
                    snprintf(used, sizeof used, "%s", e.drop.file);
                    have_rom = true;
                } else {
                    fatal_box("Maniac Mansion: wrong file", err);
                }
                SDL_free(e.drop.file);
            }
            SDL_SetRenderDrawColor(v.ren, 16, 16, 24, 255);
            SDL_RenderClear(v.ren);
            SDL_RenderPresent(v.ren);
        }
        SDL_SetWindowTitle(v.win, "Maniac Mansion");
    }
    keymap_t km;
    keymap_load(&km, cfg);
    pads_t pads = {0};
    pads_init(&pads.set, logline);
    for (int i = 0; i < SDL_NumJoysticks(); i++) pads_open(&pads, i);
    /* Batch/harness and dummy-driver runs must never wait for foreground
     * input. Neither mode nor its last selection is persisted. */
    bool harness = max_frames >= 0 || trace_path || hash_path || rec_video || rec_audio ||
                   shot_path || nwshots || getenv("MM_DEV_KEYS") ||
                   (SDL_GetCurrentVideoDriver() && (!strcmp(SDL_GetCurrentVideoDriver(), "dummy") ||
                                                   !strcmp(SDL_GetCurrentVideoDriver(), "offscreen")));
    if (!mode_explicit && !harness &&
        !mode_choose(v.ren, km.key, boot_pad_poll, boot_device_event, &pads, &mode)) {
        for (int i = pads.n - 1; i >= 0; i--) pads_close(&pads, pads.id[i]);
        cart_free(&cart);
        config_free(cfg);
        SDL_Quit();
        return 0; /* No core power or save open when the chooser is closed. */
    }
    bool enhanced = mode == MM_MODE_ENHANCED;
    logf_("[MODE] %s; runtime=full retail 6502 NES emulation; cartridge bytes unchanged; enhancement hooks %s",
          mode_name(mode), enhanced ? "opt-in" : "OFF");
    remember_rom(used);
    logf_("rom: %s (sha256 %s) verified: Maniac Mansion (USA)", used, cart.sha256);

    static nes_t nes;
    int rate = config_int(cfg, "audio", "rate", 48000);
    nes_audio_config(&nes, rate);
    nes_init(&nes, cart.prg, cart.prg_size);
    nes_power(&nes, 0x00);
    party_state_t party; party_init(&party);
    save_t save = {0};
    if (!save_open(&save, &nes, mode, &party)) return 1;

    /* audio */
    bool audio_on = config_bool(cfg, "audio", "enabled", true);
    int volume = config_int(cfg, "audio", "volume", 100);
    SDL_AudioDeviceID adev = 0;
    if (audio_on) {
        SDL_AudioSpec want = {0}, have;
        want.freq = rate;
        want.format = AUDIO_S16SYS;
        want.channels = 1;
        want.samples = 512;
        adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (!adev)
            logf_("audio: unavailable (%s); continuing without sound", SDL_GetError());
        else
            SDL_PauseAudioDevice(adev, 0);
    }
    const double base_cps = 1789773.0 / rate;
    const uint32_t target_queue = (uint32_t)(rate / 60 * 3) * 2; /* ~3 frames, bytes */

    trace_t trace = {0};
    if (trace_path && !trace_load(&trace, trace_path, err, sizeof err)) {
        fatal_box("Maniac Mansion", err);
        return 2;
    }
    long frame_no = 0;
    uint64_t emu_ticks = 0;
    bool running = true;
    FILE *hash_out = hash_path ? fopen(hash_path, "w") : NULL;
    uint64_t freq = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter();
    double acc = 0, frame_time = 1.0 / NTSC_FPS;
    static int16_t abuf[8192];
    uint8_t prev_hot = 0;

    /* dev menu: on by default in the preview releases (owner policy);
     * [dev] dev_menu = off or MM_DEV_MENU=0 hides it, MM_CHEATS=0 disables
     * it entirely. Only verified rows are ever shown. */
    const char *cheats_env = getenv("MM_CHEATS");
    const char *dev_env = getenv("MM_DEV_MENU");
    bool dev_on = !(cheats_env && !strcmp(cheats_env, "0")) &&
                  (dev_env ? strcmp(dev_env, "0") != 0 : config_bool(cfg, "dev", "dev_menu", true));
    dm_state_t dms = {0};
    dms.enhanced_mode = enhanced;
    dms.kid_switching = enhanced && config_bool(cfg, "enhancements", "kid_switching", false);
    dms.party_in = 3; /* Bernard; Dave is excluded from both replacement lists. */
    bool saved_kids = dms.kid_switching, party_intent = false, party_waiting = false;
    unsigned party_outgoing = 0, party_incoming = 0;
    enhancement_dispatch_t dispatch = {.party = &party};
    party_update(&party, enhanced, dms.kid_switching, false);
    dms.hold_walk = enhanced && config_bool(cfg, "enhancements", "hold_walk", false);
    dms.object_snap = enhanced && config_bool(cfg, "enhancements", "object_snap", false);
    dms.quick_verbs = enhanced && config_bool(cfg, "enhancements", "quick_verbs", false);
    bool saved_hold = dms.hold_walk, saved_snap = dms.object_snap, saved_verbs = dms.quick_verbs;
    qol_native_t qol;
    qol_native_init(&qol);
    v.qol = &qol;
    v.qol_overlay = devmenu_create(&dms, NULL);
    bool focus_active = true;
    int32_t qol_pad_owner = pads.set.active;
    dms.smooth_display = enhanced && config_bool(cfg, "enhancements", "smooth_display", false);
    dms.graphics_sample = enhanced && config_bool(cfg, "enhancements", "graphics_sample", false);
    logf_("[MODE] effective enhancements hold_walk=%d object_snap=%d quick_verbs=%d smooth_display=%d graphics_sample=%d kid_switching=%d",
          dms.hold_walk, dms.object_snap, dms.quick_verbs, dms.smooth_display, dms.graphics_sample, dms.kid_switching);
    bool saved_smooth = dms.smooth_display, saved_graphics = dms.graphics_sample;
    v.graphics_sample = dms.graphics_sample;
    dms.ff_speed = 1;
    dms.aspect = v.par87 ? 0 : 1;
    dms.crop = v.crop ? 1 : 0;
    devmenu_t *dm = dev_on ? devmenu_create(&dms, logline) : NULL;
    logf_("[DEV_MENU] %s", dev_on ? "enabled (F8, or Back+Start on the pad: together, or one within 300 ms of the other)" : "off");
    uint8_t prg_id[8];
    for (int i = 0; i < 8; i++) {
        unsigned b;
        sscanf(cart.prg_sha256 + i * 2, "%2x", &b);
        prg_id[i] = (uint8_t)b;
    }
    mode_snapshot_id(prg_id, mode, prg_id);
    devkey_t devkeys[64];
    int ndevkeys = dev_on ? parse_devkeys(devkeys, 64) : 0, devkey_i = 0;
    /* recordings: exact game time, one video frame per presented frame */
    bool recording = rec_video != NULL;
    FILE *wav = rec_audio ? fopen(rec_audio, "wb") : NULL;
    uint32_t wav_samples = 0;
    if (recording) {
        v.record = fopen(rec_video, "wb");
        if (!v.record) {
            fatal_box("Maniac Mansion", "cannot open --record-video output");
            return 1;
        }
        char idx[1100];
        snprintf(idx, sizeof idx, "%s.idx", rec_video);
        v.record_idx = strncmp(rec_video, "/dev/", 5) ? fopen(idx, "w") : NULL;
        if (!v.record_idx && getenv("MM_RECORD_IDX"))
            v.record_idx = fopen(getenv("MM_RECORD_IDX"), "w");
        logf_("recording: video %s, audio %s (%dx%d)", rec_video, rec_audio ? rec_audio : "-", win_w, win_h);
    }
    if (wav)
        fwrite((uint8_t[44]){0}, 1, 44, wav);
    struct { long frame; int bottom; char text[120]; } caps[128];
    int ncaps = 0, cap_i = 0;
    if (captions_path) {
        FILE *cf = fopen(captions_path, "r");
        char line[256];
        while (cf && ncaps < 128 && fgets(line, sizeof line, cf)) {
            long fr;
            char where[16];
            int used = 0;
            if (line[0] == '#' || sscanf(line, "%ld %15s %n", &fr, where, &used) < 2)
                continue;
            line[strcspn(line, "\n")] = 0;
            caps[ncaps].frame = fr;
            caps[ncaps].bottom = !strcmp(where, "bottom");
            snprintf(caps[ncaps].text, sizeof caps[0].text, "%s", line + used);
            ncaps++;
        }
        if (cf)
            fclose(cf);
    }
    static char cap_top[120], cap_bottom[120];
    bool drain = false;          /* after the menu closes: wait for neutral */
    chord_t chord;               /* Back+Start gesture (chord.h); swallows both while a chord is possible */
    chord_init(&chord);
    chord_timed_t enhanced_chord;
    chord_timed_init(&enhanced_chord);
    bool timed_chord_on = false;
    bool chord_active = false;   /* menu toggled by this gesture: pad nav waits for release */
    uint8_t prev_nav = 0;        /* pad nav edges: 1 up 2 down 4 ok 8 back */

    while (running) {
        SDL_Event e;
        bool menu = dm && devmenu_is_open(dm);
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = false; break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    focus_active = false;
                    if (dms.hold_walk || dms.object_snap || dms.quick_verbs) drain = true;
                }
                else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    focus_active = true;
                    if (dms.hold_walk || dms.object_snap || dms.quick_verbs) drain = true;
                }
                break;
            case SDL_CONTROLLERDEVICEADDED: pads_open(&pads, e.cdevice.which); break;
            case SDL_CONTROLLERDEVICEREMOVED: pads_close(&pads, e.cdevice.which); break;
            case SDL_JOYDEVICEADDED: /* pads without a mapping: raw fallback (mapped ones arrive above too; dedup'd by id) */
                if (!SDL_IsGameController(e.jdevice.which)) pads_open(&pads, e.jdevice.which);
                break;
            case SDL_JOYDEVICEREMOVED: pads_close(&pads, e.jdevice.which); break;
            case SDL_KEYDOWN: {
                if (e.key.repeat)
                    break;
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(v.win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                } else if (menu) {
                    /* menu open: it consumes navigation; port hotkeys are suppressed */
                    if (k == SDLK_UP) devmenu_nav(dm, DM_UP);
                    else if (k == SDLK_DOWN) devmenu_nav(dm, DM_DOWN);
                    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) devmenu_nav(dm, DM_OK);
                    else if (k == SDLK_ESCAPE) devmenu_nav(dm, DM_BACK);
                    else if (k == SDLK_F8) devmenu_toggle(dm);
                    else if (k == SDLK_F1) dms.show_help = !dms.show_help;
                } else if (k == SDLK_ESCAPE) {
                    running = false;
                } else if (k == SDLK_F6) {
                    dms.ff_on = !dms.ff_on;
                } else if (k == SDLK_F10) {
                    take_screenshot(nes.ppu.fb, dm);
                } else if (dm) {
                    if (k == SDLK_F8) devmenu_toggle(dm);
                    else if (k == SDLK_F1) dms.show_help = !dms.show_help;
                    else if (k == SDLK_F7) {
                        dms.ff_speed = (e.key.keysym.mod & KMOD_SHIFT) ? 1 : (dms.ff_speed + 1) % 4;
                        devmenu_toast(dm, "FAST-FORWARD SPEED: %s",
                                      (const char *[]){"2X", "4X", "8X", "MAX"}[dms.ff_speed]);
                    } else if (k == SDLK_F11) dms.req_quick_save = true;
                    else if (k == SDLK_F12) dms.req_quick_load = true;
                }
                if (dm && !devmenu_is_open(dm) && menu)
                    drain = true;
            } break;
            }
        }

        /* scripted dev keys (tests only) */
        while (devkey_i < ndevkeys && devkeys[devkey_i].frame <= frame_no) {
            const char *k = devkeys[devkey_i++].key;
            logf_("[DEV_MENU] scripted key %s at frame %ld", k, frame_no);
            bool was = devmenu_is_open(dm);
            if (!strcmp(k, "F8")) devmenu_toggle(dm);
            else if (!strcmp(k, "UP")) devmenu_nav(dm, DM_UP);
            else if (!strcmp(k, "DOWN")) devmenu_nav(dm, DM_DOWN);
            else if (!strcmp(k, "OK")) devmenu_nav(dm, DM_OK);
            else if (!strcmp(k, "BACK")) devmenu_nav(dm, DM_BACK);
            else if (!strcmp(k, "F1")) dms.show_help = !dms.show_help;
            else if (!strcmp(k, "F10")) take_screenshot(nes.ppu.fb, dm);
            else if (!strcmp(k, "F11")) dms.req_quick_save = true;
            else if (!strcmp(k, "F12")) dms.req_quick_load = true;
            if (was && !devmenu_is_open(dm))
                drain = true;
        }

        /* input */
        const uint8_t *ks = SDL_GetKeyboardState(NULL);
        uint8_t kb = 0;
        for (int i = 0; i < 8; i++)
            if (ks[km.key[i]])
                kb |= (uint8_t)(1 << i);
        if ((kb & (PAD_UP | PAD_DOWN)) == (PAD_UP | PAD_DOWN)) kb &= ~(PAD_UP | PAD_DOWN);
        if ((kb & (PAD_LEFT | PAD_RIGHT)) == (PAD_LEFT | PAD_RIGHT)) kb &= ~(PAD_LEFT | PAD_RIGHT);
        uint8_t hot;
        uint8_t gp = pads_poll(&pads, &hot);
        if (qol_pad_owner != pads.set.active) {
            if (qol_pad_owner >= 0 && (dms.hold_walk || dms.object_snap || dms.quick_verbs)) {
                qol_native_init(&qol);
                drain = true;
            }
            qol_pad_owner = pads.set.active;
        }
        menu = dm && devmenu_is_open(dm);

        if (dm) {
            /* Back+Start on the same pad opens/closes the menu. With the
             * opt-in on, both are held back from the game for a short grace
             * window so the combo never also reaches the game. */
            bool fire = false;
            bool use_timed = dms.hold_walk || dms.object_snap || dms.quick_verbs ||
                             dms.smooth_display || dms.graphics_sample || dms.kid_switching;
            if (use_timed != timed_chord_on) {
                chord_init(&chord); chord_timed_init(&enhanced_chord);
                chord.fired = enhanced_chord.fired = pads.back_now || pads.start_now;
                timed_chord_on = use_timed;
            }
            uint8_t pass = use_timed
                ? chord_timed_step(&enhanced_chord, pads.back_now, pads.start_now, PAD_SELECT, PAD_START,
                                   (uint64_t)((double)SDL_GetPerformanceCounter() * 1000.0 / (double)freq), &fire)
                : chord_step(&chord, pads.back_now, pads.start_now, PAD_SELECT, PAD_START, &fire);
            if (fire) {
                bool was = devmenu_is_open(dm);
                devmenu_toggle(dm);
                if (was)
                    drain = true;
                menu = devmenu_is_open(dm);
                chord_active = true;
                logf_("[DEV_MENU] Back+Start chord: menu %s", menu ? "open" : "closed");
            }
            if (chord_active && !pads.back_now && !pads.start_now)
                chord_active = false;
            gp = (uint8_t)((gp & ~(PAD_SELECT | PAD_START)) | pass);
            /* pad navigation: edges only, no auto-repeat */
            uint8_t nav = 0;
            if (menu && !chord_active) {
                if (gp & PAD_UP) nav |= 1;
                if (gp & PAD_DOWN) nav |= 2;
                if (gp & PAD_A) nav |= 4;
                if (gp & PAD_B) nav |= 8;
                uint8_t edge = nav & ~prev_nav;
                if (edge & 1) devmenu_nav(dm, DM_UP);
                if (edge & 2) devmenu_nav(dm, DM_DOWN);
                if (edge & 4) devmenu_nav(dm, DM_OK);
                if (edge & 8) {
                    devmenu_nav(dm, DM_BACK);
                    if (!devmenu_is_open(dm))
                        drain = true;
                }
            }
            prev_nav = nav;
            menu = devmenu_is_open(dm);
            /* one-shot requests from the menu or F-keys */
            if (dms.req_screenshot) {
                dms.req_screenshot = false;
                take_screenshot(nes.ppu.fb, dm);
            }
            if (dms.req_quick_save) {
                dms.req_quick_save = false;
                quick_save(&nes, prg_id, dm, mode, &party);
            }
            if (dms.req_quick_load) {
                dms.req_quick_load = false;
                quick_load(&nes, prg_id, dm, mode, &party);
                qol_native_init(&qol);
                if (dms.hold_walk || dms.object_snap || dms.quick_verbs) drain = true;
            }
            /* Original mode never enables/persists enhancement choices,
             * even if an older menu or scripted input requests a toggle. */
            if (!enhanced) {
                dms.hold_walk = dms.object_snap = dms.quick_verbs = false;
                dms.smooth_display = dms.graphics_sample = false;
            }
            if (dms.req_party_swap) {
                static const unsigned actors[6] = {1, 2, 4, 5, 6, 7};
                dms.req_party_swap = false;
                if (enhanced && dms.kid_switching && !party.pending && !party.executing) {
                    party_outgoing = actors[dms.party_out]; party_incoming = actors[dms.party_in];
                    party_intent = true;
                    if (devmenu_is_open(dm)) devmenu_toggle(dm);
                    menu = false; drain = true;
                    devmenu_toast(dm, "RELEASE INPUT TO SWAP");
                }
            }
            if (saved_kids != dms.kid_switching) {
                saved_kids = dms.kid_switching;
                if (!config_save_bool(cfg, "enhancements", "kid_switching", saved_kids))
                    devmenu_toast(dm, "TEAMMATE SETTING SAVE FAILED");
            }
            if (saved_hold != dms.hold_walk) {
                saved_hold = dms.hold_walk;
                if (!config_save_bool(cfg, "enhancements", "hold_walk", saved_hold))
                    devmenu_toast(dm, "WALK SETTING SAVE FAILED");
            }
            if (saved_snap != dms.object_snap) {
                saved_snap = dms.object_snap;
                if (!config_save_bool(cfg, "enhancements", "object_snap", saved_snap))
                    devmenu_toast(dm, "SNAP SETTING SAVE FAILED");
            }
            if (saved_verbs != dms.quick_verbs) {
                saved_verbs = dms.quick_verbs;
                if (!config_save_bool(cfg, "enhancements", "quick_verbs", saved_verbs))
                    devmenu_toast(dm, "VERB SETTING SAVE FAILED");
            }
            if (saved_smooth != dms.smooth_display) {
                saved_smooth = dms.smooth_display;
                if (!config_save_bool(cfg, "enhancements", "smooth_display", saved_smooth))
                    devmenu_toast(dm, "DISPLAY SETTING SAVE FAILED");
            }
            if (saved_graphics != dms.graphics_sample) {
                saved_graphics = dms.graphics_sample;
                if (!config_save_bool(cfg, "enhancements", "graphics_sample", saved_graphics))
                    devmenu_toast(dm, "GRAPHICS SETTING SAVE FAILED");
            }
            v.graphics_sample = dms.graphics_sample;
            v.par87 = dms.aspect == 0;
            v.crop = dms.crop ? 8 : 0;
        }
        if ((hot & 1) && !(prev_hot & 1) && !menu)
            dms.ff_on = !dms.ff_on; /* L3 */
        prev_hot = hot;
        bool ff = !menu && (dms.ff_on || ks[SDL_SCANCODE_BACKSPACE] || (hot & 2));

        uint8_t game_in = kb | gp;
        if (menu)
            game_in = 0; /* the menu consumes input; the game keeps running */
        if (drain) {
            if (game_in)
                game_in = 0; /* held from the menu: must return to neutral first */
            else
                drain = false;
        }
        if (enhanced) {
            if (nes.exec_hook == enhancement_dispatch && nes.exec_hook_ud == &dispatch) {
                nes.exec_hook = dispatch.input_hook; nes.exec_hook_ud = dispatch.input_userdata;
            }
            bool neutral = !menu && !drain && !chord_active && focus_active && !(kb | gp);
            party_update(&party, true, dms.kid_switching && !trace_path, neutral);
            if (party_intent && neutral) {
                party_intent = false;
                party_result_t result = party_request(&nes, &party, party_outgoing, party_incoming);
                party_waiting = result == PARTY_OK;
                logf_("[PARTY] requested %s -> %s: %s", party_actor_name(party_outgoing),
                      party_actor_name(party_incoming), party_result_name(result));
                if (!party_waiting && dm) devmenu_toast(dm, "%s", party_result_short(result));
            }
            if (party_waiting && !party.pending && !party.executing) {
                party_waiting = false; drain = true;
                logf_("[PARTY] result: %s", party_result_name(party.result));
                if (dm) devmenu_toast(dm, "%s", party.result == PARTY_OK ? "TEAMMATE REPLACED" : party_result_short(party.result));
                qol_native_init(&qol);
            }
            if (party.pending || party.executing) game_in = 0;
            qol_native_update(&nes, &qol, (qol_options_t){dms.hold_walk, dms.object_snap, dms.quick_verbs},
                              game_in, !menu && !drain && !trace_path && focus_active && !chord_active &&
                              !party.pending && !party.executing);
            dispatch.input_hook = nes.exec_hook; dispatch.input_userdata = nes.exec_hook_ud;
            nes.exec_hook = enhancement_dispatch; nes.exec_hook_ud = &dispatch;
        }
        if (trace_path) {
            uint8_t tp[2];
            trace_at(&trace, (uint32_t)frame_no, tp);
            nes_set_pad(&nes, 0, tp[0]);
            nes_set_pad(&nes, 1, tp[1]);
        } else {
            nes_set_pad(&nes, 0, game_in);
        }

        while (cap_i < ncaps && caps[cap_i].frame <= frame_no) {
            snprintf(caps[cap_i].bottom ? cap_bottom : cap_top, 120, "%s", caps[cap_i].text);
            cap_i++;
        }
        v.caption_top = cap_top[0] ? cap_top : NULL;
        v.caption_bottom = cap_bottom[0] ? cap_bottom : NULL;

        /* timing */
        uint64_t now = SDL_GetPerformanceCounter();
        acc += (double)(now - last) / (double)freq;
        last = now;
        if (acc > 0.25)
            acc = frame_time; /* after a stall, don't try to catch up */
        int to_run = 0;
        uint64_t budget = 0;
        if (recording) {
            /* no wall clock: one presented frame per loop, ff frames per present */
            int sp = dm_ff_speeds[dms.ff_speed];
            to_run = ff ? (sp ? sp : 8) : 1;
        } else if (ff) {
            int sp = dm_ff_speeds[dms.ff_speed];
            to_run = sp ? sp : 64;
            budget = sp ? 0 : now + freq / 70; /* max: whatever fits in ~14 ms */
            acc = 0;
        } else {
            while (acc >= frame_time && to_run < 3) {
                acc -= frame_time;
                to_run++;
            }
        }
        for (int f = 0; f < to_run; f++) {
            if (trace_path && f > 0) {
                uint8_t tp[2]; /* traces advance one frame at a time */
                trace_at(&trace, (uint32_t)frame_no, tp);
                nes_set_pad(&nes, 0, tp[0]);
                nes_set_pad(&nes, 1, tp[1]);
            }
            uint64_t t0 = SDL_GetPerformanceCounter();
            nes_run_frame(&nes);
            emu_ticks += SDL_GetPerformanceCounter() - t0;
            if (frame_no == shot_frame && shot_path)
                logf_(png_write_nes(shot_path, nes.ppu.fb) ? "screenshot: %s" : "screenshot failed: %s", shot_path);
            if (hash_out) {
                uint64_t h = 0xcbf29ce484222325ULL;
                for (int k = 0; k < NES_W * NES_H; k++) {
                    h ^= nes.ppu.fb[k];
                    h *= 0x100000001b3ULL;
                }
                fprintf(hash_out, "%ld %016llx\n", frame_no, (unsigned long long)h);
            }
            for (int w = 0; w < nwshots; w++)
                if (wshots[w].frame == frame_no)
                    snprintf(v.window_shot, sizeof v.window_shot, "%s", wshots[w].path);
            frame_no++;
            size_t n = nes_audio_take(&nes, abuf, sizeof abuf / sizeof abuf[0]);
            if (wav && f == to_run - 1) {
                /* one frame of audio per video frame; silent while fast-forwarding */
                if (ff)
                    memset(abuf, 0, n * sizeof abuf[0]);
                fwrite(abuf, sizeof abuf[0], n, wav);
                wav_samples += (uint32_t)n;
            }
            if (adev && !ff) {
                if (volume != 100)
                    for (size_t k = 0; k < n; k++)
                        abuf[k] = (int16_t)(abuf[k] * volume / 100);
                SDL_QueueAudio(adev, abuf, (uint32_t)(n * sizeof abuf[0]));
            }
            if (max_frames >= 0 && frame_no >= max_frames) {
                running = false;
                break;
            }
            if (budget && SDL_GetPerformanceCounter() > budget)
                break;
        }
        if (adev) {
            /* Keep ~3 frames queued by nudging the resample ratio (max 0.5%). */
            uint32_t q = SDL_GetQueuedAudioSize(adev);
            if (ff)
                SDL_ClearQueuedAudio(adev);
            double err_ratio = ((double)q - target_queue) / (double)target_queue;
            if (err_ratio > 1) err_ratio = 1;
            if (err_ratio < -1) err_ratio = -1;
            nes.apu.cycles_per_sample = base_cps * (1.0 + 0.005 * err_ratio);
        }
        v.game_frame = frame_no;
        if (to_run)
            video_present(&v, nes.ppu.fb, dm);
        if (dms.smooth_display && !recording && !ff) {
            double elapsed = (double)(SDL_GetPerformanceCounter() - last) / (double)freq;
            SDL_Delay(presentation_wait_ms(acc, elapsed, frame_time));
        } else if (!to_run)
            SDL_Delay(1);
        save_flush(&save, &nes, false, &party);
    }

    if (v.record)
        fclose(v.record);
    if (v.record_idx)
        fclose(v.record_idx);
    if (wav) { /* 48 kHz mono s16 */
        uint32_t data = wav_samples * 2, riff = 36 + data, r = (uint32_t)rate, br = r * 2, fmt = 16;
        uint16_t pcm = 1, ch = 1, al = 2, bits = 16;
        rewind(wav);
        fwrite("RIFF", 1, 4, wav); fwrite(&riff, 4, 1, wav); fwrite("WAVEfmt ", 1, 8, wav);
        fwrite(&fmt, 4, 1, wav); fwrite(&pcm, 2, 1, wav); fwrite(&ch, 2, 1, wav); fwrite(&r, 4, 1, wav);
        fwrite(&br, 4, 1, wav); fwrite(&al, 2, 1, wav); fwrite(&bits, 2, 1, wav);
        fwrite("data", 1, 4, wav); fwrite(&data, 4, 1, wav);
        fclose(wav);
    }
    if (enhanced && (party.pending || party.executing)) party_cancel(&nes, &party);
    save_flush(&save, &nes, true, &party);
    if (frame_no)
        logf_("frames: %ld, core %.3f ms/frame (%.0f fps capable)", frame_no,
              1000.0 * (double)emu_ticks / (double)freq / (double)frame_no,
              (double)frame_no * (double)freq / (double)(emu_ticks ? emu_ticks : 1));
    logf_("exit");
    if (hash_out)
        fclose(hash_out);
    party_destroy(&party);
    trace_free(&trace);
    devmenu_free(dm);
    devmenu_free(v.qol_overlay);
    if (adev)
        SDL_CloseAudioDevice(adev);
    SDL_Quit();
    cart_free(&cart);
    config_free(cfg);
    return 0;
}
