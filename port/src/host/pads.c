#include "pads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* §4a step 1: entries to remove from the inherited ignore list. */
static bool unhide(uint16_t vid, uint16_t pid, bool any_pid)
{
    if (vid == 0x17EF) /* every Lenovo Legion entry */
        return true;
    if (vid != 0x28DE || any_pid)
        return false;
    return pid == 0x1205 || pid == 0x1206 || (pid >= 0x12F0 && pid <= 0x12FF);
}

char *pads_filter_ignore_list(const char *list)
{
    size_t n = strlen(list);
    char *out = malloc(n + 1), *o = out;
    char *copy = strdup(list);
    bool first = true;
    for (char *save = NULL, *tok = strtok_r(copy, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ')
            tok++;
        unsigned vid = 0, pid = 0;
        char pidtxt[16] = {0};
        bool keep = true;
        if (sscanf(tok, "%x/%15s", &vid, pidtxt) == 2) {
            bool any = pidtxt[0] == '*';
            if (!any)
                pid = (unsigned)strtoul(pidtxt, NULL, 16);
            keep = !unhide((uint16_t)vid, (uint16_t)pid, any);
        }
        if (keep) {
            if (!first)
                *o++ = ',';
            size_t l = strlen(tok);
            memcpy(o, tok, l);
            o += l;
            first = false;
        }
    }
    *o = 0;
    free(copy);
    return out;
}

void pads_fix_environment(void (*log)(const char *line))
{
    static const char *vars[] = {"SDL_GAMECONTROLLER_IGNORE_DEVICES", "SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT"};
    char line[1024];
    for (int i = 0; i < 2; i++) {
        const char *v = getenv(vars[i]);
        if (!v)
            continue;
        char *f = pads_filter_ignore_list(v);
        snprintf(line, sizeof line, "[DEV_MENU] env %s before='%s' after='%s'", vars[i], v, f);
        if (log)
            log(line);
        setenv(vars[i], f, 1);
        free(f);
    }
    /* gamescope drops focus; keep receiving pad events regardless */
    setenv("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1", 1);
    /* SDL >= 2.28 hides Steam's virtual gamepad (28de:11ff) unless this hint is set;
     * Steam sets it for games IT launches, the Banshee launcher does not. Under Game
     * Mode the physical pad's device node is root-only, so without this the port
     * saw no pad at all (Legion Go, 2026-09-30). */
    setenv("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", "1", 1);
    if (log)
        log("[DEV_MENU] env SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1 (Steam virtual pad visible)");
}

bool pads_is_steam_virtual(uint16_t vid, uint16_t pid, const char *name)
{
    if (vid == 0x28DE && pid == 0x11FF)
        return true;
    return name && strcasestr(name, "Steam Virtual");
}

void pads_init(pad_set_t *s, void (*log)(const char *line))
{
    memset(s, 0, sizeof *s);
    s->active = -1;
    s->log = log;
}

static int find(const pad_set_t *s, int32_t instance)
{
    for (int i = 0; i < s->n; i++)
        if (s->dev[i].instance == instance)
            return i;
    return -1;
}

const pad_dev_t *pads_get(const pad_set_t *s, int32_t instance)
{
    int i = find(s, instance);
    return i < 0 ? NULL : &s->dev[i];
}

static void choose(pad_set_t *s, int idx, const char *reason)
{
    pad_dev_t *d = &s->dev[idx];
    if (s->active == d->instance)
        return;
    s->active = d->instance;
    if (s->log) {
        char line[256];
        snprintf(line, sizeof line, "[DEV_MENU] pad: %s vid:pid=%04x:%04x instance=%d reason=%s", d->name, d->vid,
                 d->pid, (int)d->instance, reason);
        s->log(line);
    }
}

void pads_added(pad_set_t *s, int32_t instance, uint16_t vid, uint16_t pid, const char *name, bool held)
{
    int i = find(s, instance);
    if (i < 0) {
        if (s->n == PADS_MAX)
            return;
        i = s->n++;
    }
    pad_dev_t *d = &s->dev[i];
    memset(d, 0, sizeof *d);
    d->instance = instance;
    d->vid = vid;
    d->pid = pid;
    d->is_virtual = pads_is_steam_virtual(vid, pid, name);
    d->need_neutral = held;
    snprintf(d->name, sizeof d->name, "%s", name ? name : "?");
}

void pads_removed(pad_set_t *s, int32_t instance)
{
    int i = find(s, instance);
    if (i < 0)
        return;
    s->dev[i] = s->dev[--s->n];
    if (s->active == instance) {
        s->active = -1;
        if (s->log)
            s->log("[DEV_MENU] pad: active pad removed; waiting for input");
    }
}

bool pads_input(pad_set_t *s, int32_t instance, bool non_neutral)
{
    int i = find(s, instance);
    if (i < 0)
        return false;
    pad_dev_t *d = &s->dev[i];
    if (d->need_neutral) {
        if (!non_neutral)
            d->need_neutral = false;
        return false;
    }
    if (non_neutral && !d->had_input) {
        /* First input from this pad: re-evaluate. */
        d->had_input = true;
        int a = find(s, s->active);
        if (!d->is_virtual)
            choose(s, i, "physical-active");
        else if (a < 0 || (s->dev[a].is_virtual)) {
            bool physical_live = false;
            for (int k = 0; k < s->n; k++)
                if (!s->dev[k].is_virtual && s->dev[k].had_input)
                    physical_live = true;
            if (!physical_live)
                choose(s, i, "virtual-only");
        }
    } else if (non_neutral && s->active < 0 && !d->is_virtual) {
        choose(s, i, "physical-active");
    } else if (non_neutral && s->active < 0) {
        choose(s, i, "virtual-only");
    }
    return s->active == instance;
}

uint8_t pads_raw_layout(uint32_t buttons, uint8_t hat, int ax, int ay, bool *back, bool *start, bool *l3, bool *r3)
{
    enum { A = 1, B = 2, SELECT = 4, START = 8, UP = 16, DOWN = 32, LEFT = 64, RIGHT = 128 };
    const int T = 16000;
    uint8_t b = 0;
    if (buttons & (1u << PADS_RAW_A)) b |= A;
    if (buttons & ((1u << PADS_RAW_B) | (1u << PADS_RAW_X))) b |= B;
    if (buttons & (1u << PADS_RAW_BACK)) b |= SELECT;
    if (buttons & (1u << PADS_RAW_START)) b |= START;
    if ((hat & 1) || ay < -T) b |= UP;
    if ((hat & 4) || ay > T) b |= DOWN;
    if ((hat & 8) || ax < -T) b |= LEFT;
    if ((hat & 2) || ax > T) b |= RIGHT;
    if ((b & (UP | DOWN)) == (UP | DOWN)) b &= (uint8_t)~(UP | DOWN);
    if ((b & (LEFT | RIGHT)) == (LEFT | RIGHT)) b &= (uint8_t)~(LEFT | RIGHT);
    if (back) *back = (buttons >> PADS_RAW_BACK) & 1;
    if (start) *start = (buttons >> PADS_RAW_START) & 1;
    if (l3) *l3 = (buttons >> PADS_RAW_L3) & 1;
    if (r3) *r3 = (buttons >> PADS_RAW_R3) & 1;
    return b;
}
