#include "qol_input.h"
#include "../core/nes.h"
#include <limits.h>
#include <string.h>

#define HOLD_MS 250u
const char *qol_verb_name(qol_verb_t verb)
{
    static const char *const names[] = { "Walk to", "Push", "Pull", "Give", "Open", "Close",
        "Pick up", "Use", "Read", "Turn on", "Turn off", "New kid" };
    return (unsigned)verb < QOL_COUNT ? names[verb] : "";
}
void qol_input_reset(qol_input_t *s) { memset(s, 0, sizeof(*s)); }
static bool safe(qol_context_t c)
{ return c.gameplay && c.input_enabled && !c.menu && !c.cutscene && !c.chord; }
qol_result_t qol_input_step(qol_input_t *s, qol_options_t o, qol_context_t c,
                           uint8_t pad, uint64_t now)
{
    qol_result_t r = { .pad = pad };
    /* OFF is exact pass-through even in a menu: host owns menu consumption. */
    if (!o.hold_walk && !o.object_snap && !o.quick_verbs) {
        qol_input_reset(s);
        return r;
    }
    if (!safe(c) || c.epoch != s->epoch) {
        r.chooser_closed = s->chooser;
        qol_input_reset(s);
        s->epoch = c.epoch;
        s->blocked = pad != 0;
        r.pad = 0;
        return r;
    }
    if (s->blocked) {
        if (!pad) s->blocked = false;
        r.pad = 0;
        return r;
    }
    uint8_t edge = (uint8_t)(pad & ~s->previous);
    if (o.quick_verbs && !c.sentence_pending) {
        if (edge & PAD_SELECT) {
            s->select_pending = true;
            s->select_since = now;
        }
        if (s->select_pending && (pad & PAD_START)) {
            /* Host chord adjudication must run first. Never commit a tap on a
             * Start combination; require neutral after cancellation. */
            s->select_pending = false;
            s->chooser = false;
            s->blocked = true;
            r.pad = 0;
        } else if (s->select_pending) {
            r.pad &= (uint8_t)~PAD_SELECT;
            if (!(pad & PAD_SELECT)) {
                r.pad |= PAD_SELECT; /* one native Select pulse on short release */
                s->select_pending = false;
            } else if (now - s->select_since >= HOLD_MS) {
                s->select_pending = false;
                s->chooser = true;
                r.chooser_open = true;
            }
        }
    } else if (s->select_pending || s->chooser) {
        s->select_pending = s->chooser = false;
        s->blocked = pad != 0;
        r.chooser_closed = true;
        r.pad = 0;
    }
    if (s->chooser) {
        r.pad = 0;
        if (edge & (PAD_UP | PAD_LEFT)) s->verb_index = (s->verb_index + QOL_COUNT - 1) % QOL_COUNT;
        else if (edge & (PAD_DOWN | PAD_RIGHT)) s->verb_index = (s->verb_index + 1) % QOL_COUNT;
        if (edge & (PAD_A | PAD_B)) {
            r.select_verb = (edge & PAD_A) != 0;
            r.verb = (qol_verb_t)s->verb_index;
            r.chooser_closed = true;
            s->chooser = false;
            s->blocked = true;
        }
    }
    bool can_walk = o.hold_walk && c.walk_verb && c.free_walk && !c.sentence_pending &&
                    !s->chooser && !s->select_pending && !s->blocked && !(pad & (PAD_SELECT | PAD_START | PAD_B));
    if ((edge & PAD_A) && can_walk) {
        s->holding_walk = true;
        s->a_since = now;
        s->walk_x = c.cursor_x; s->walk_y = c.cursor_y;
    }
    if (!(pad & PAD_A) || !can_walk) s->holding_walk = false;
    if (s->holding_walk && now - s->a_since >= HOLD_MS &&
        (s->walk_x != c.cursor_x || s->walk_y != c.cursor_y)) {
        r.walk_update = true;
        r.walk_x = s->walk_x = c.cursor_x;
        r.walk_y = s->walk_y = c.cursor_y;
    }
    s->previous = pad;
    return r;
}
bool qol_snap(const qol_target_t *targets, size_t n, uint32_t current,
              int x, int y, int left, int top, int right, int bottom,
              qol_direction_t dir, qol_target_t *selected)
{
    if (!targets || !selected || dir > QOL_RIGHT || left >= right || top >= bottom) return false;
    bool found = false;
    uint64_t best = UINT64_MAX;
    for (size_t i = 0; i < n; ++i) {
        const qol_target_t *t = &targets[i];
        if (!t->visible || !t->interactive || !t->hit_visible || t->id == current ||
            t->x < left || t->x >= right || t->y < top || t->y >= bottom) continue;
        /* Clamp to realistic screen coordinates to prevent hostile provider
         * overflow. Viewport coordinates are allowed a small scroll margin. */
        if (t->x < -4096 || t->x > 4096 || t->y < -4096 || t->y > 4096 ||
            x < -4096 || x > 4096 || y < -4096 || y > 4096) continue;
        int64_t dx = (int64_t)t->x - x, dy = (int64_t)t->y - y;
        int64_t forward = dir == QOL_UP ? -dy : dir == QOL_DOWN ? dy : dir == QOL_LEFT ? -dx : dx;
        int64_t side = dir == QOL_UP || dir == QOL_DOWN ? dx : dy;
        if (forward <= 0) continue;
        uint64_t score = (uint64_t)(forward * forward + 4 * side * side);
        if (!found || score < best || (score == best && t->id < selected->id)) {
            *selected = *t; best = score; found = true;
        }
    }
    return found;
}

static unsigned guest(const nes_t *n, unsigned address)
{ return n->prgram[address - 0x6000]; }
bool qol_native_ready(const nes_t *n)
{
    return n && guest(n, 0x63c6) != 0 && (guest(n, 0x6b0a) & 0x10) &&
        !guest(n, 0x6b08) && !guest(n, 0x6b09) &&
        !guest(n, 0x6d3c) && !guest(n, 0x6b0b) && !guest(n, 0x710d) && (!guest(n, 0x72d8) || (n->cpu.pc == 0x913f && n->mmc1.prg_lo == n->prg)) && !guest(n, 0x72bf);
}
static bool object_visible(const nes_t *n, unsigned i, unsigned count)
{
    if (guest(n, 0x65e7 + i) & 0x20) return false;
    for (unsigned depth = 0; depth <= count; ++depth) {
        unsigned required = guest(n, 0x6538 + i);
        unsigned parent = guest(n, 0x6515 + i);
        if (!parent) return true;
        if (parent > count || (guest(n, 0x65e7 + parent) & 0x80) != required) return false;
        i = parent;
    }
    return false; /* corrupt/cyclic parents fail closed */
}
static unsigned hit_slot(const nes_t *n, unsigned count, unsigned x, unsigned y)
{
    for (unsigned i = 1; i <= count; ++i) {
        unsigned lx = guest(n, 0x63da + i), ly = guest(n, 0x63fd + i);
        unsigned rx = (lx + guest(n, 0x6420 + i)) & 255u;
        unsigned by = (ly + guest(n, 0x6443 + i)) & 255u;
        if (object_visible(n, i, count) && x >= lx && x < rx && y >= ly && y < by) return i;
    }
    return 0;
}
size_t qol_native_targets(const nes_t *n, qol_target_t *out, size_t cap)
{
    if (!out || !cap || !qol_native_ready(n)) return 0;
    unsigned count = guest(n, 0x63d9), left = guest(n, 0x6c31);
    if (count > 34) return 0;
    size_t used = 0;
    for (unsigned i = 1; i <= count && used < cap; ++i) {
        if (!object_visible(n, i, count)) continue;
        int best_x = 0, best_y = 0;
        unsigned distance = UINT_MAX;
        int lx = (int)guest(n, 0x63da + i), ly = (int)guest(n, 0x63fd + i);
        int w = (int)guest(n, 0x6420 + i), h = (int)guest(n, 0x6443 + i);
        if (!w || !h) continue;
        /* Search only visible screen points and verify native overlap priority.
         * Native point transform gives object_y=floor((screen_y-35)/8). */
        for (int sy = 39; sy < 175; sy += 8) {
            unsigned oy = (unsigned)(((sy + 1) / 2 - 18) / 4);
            if (oy < (unsigned)ly || oy >= (unsigned)(ly + h)) continue;
            for (int sx = 20; sx < 240; sx += 8) {
                unsigned ox = ((unsigned)(sx / 8) + left) & 255u;
                if (ox < (unsigned)lx || ox >= (unsigned)(lx + w) || hit_slot(n, count, ox, oy) != i) continue;
                int relative_x = ((lx - (int)left + 128) & 255) - 128;
                int dx = sx - (relative_x * 8 + w * 4);
                int dy = sy - (ly * 8 + 35 + h * 4);
                unsigned d = (unsigned)(dx * dx + dy * dy);
                if (d < distance) { distance = d; best_x = sx; best_y = sy; }
            }
        }
        if (distance == UINT_MAX) continue;
        out[used++] = (qol_target_t){ guest(n, 0x64f2 + i) | (guest(n, 0x64cf + i) << 8),
            best_x, best_y, true, true, true };
    }
    return used;
}
bool qol_native_cursor(nes_t *n, int x, int y)
{
    if (!qol_native_ready(n) || x < 16 || x > 239 || y < 35 || y > 222) return false;
    /* Clear native movement momentum/fractions so a released directional edge
     * doesn't move the snapped pointer away on the following native poll. */
    for (unsigned a = 0x710f; a <= 0x7115; ++a) n->prgram[a - 0x6000] = 0;
    n->prgram[0x7117 - 0x6000] = 0;
    n->prgram[0x7116 - 0x6000] = (uint8_t)x;
    n->prgram[0x7118 - 0x6000] = (uint8_t)y;
    return true;
}
bool qol_native_verb_cursor(nes_t *n, unsigned id)
{
    if (!qol_native_ready(n) || !(guest(n, 0x6b0a) & 0x80) || !id || id > 255) return false;
    for (unsigned i = 0; i < 28; ++i) {
        if (guest(n, 0x6ace + i) != id) continue;
        unsigned top = guest(n, 0x6a5e + i), bottom = guest(n, 0x6a7a + i);
        unsigned left = guest(n, 0x6a96 + i), right = guest(n, 0x6ab2 + i);
        if (left >= right || top >= bottom) continue;
        /* UI may contain inventory/kid cells with same numeric id; restrict
         * candidate to native verb panel (screen rows 22..25). */
        if (top < 22 || bottom > 26) continue;
        int x = (int)(left + right) * 4 + 8;
        int y = (int)(top + bottom) * 4 + 2;
        return qol_native_cursor(n, x, y);
    }
    return false;
}

void qol_native_init(qol_native_t *s) { memset(s, 0, sizeof(*s)); }
static void native_hook(nes_t *n, void *opaque)
{
    qol_native_t *s = opaque;
    if (n->cpu.pc != 0x913f || n->mmc1.prg_lo != n->prg) return;
    uint64_t now = n->master / 21477u; /* emulated NTSC master-clock milliseconds */
    uint8_t old = s->policy.previous;
    unsigned verb = guest(n, 0x611a);
    unsigned scene_x = (guest(n,0x7116) / 8 + guest(n,0x6c31)) & 255u;
    unsigned scene_y = ((guest(n,0x7118) + 1) / 2 - 18) / 4;
    unsigned count_now = guest(n,0x63d9);
    bool scene = guest(n,0x7118) < 175;
    bool empty_walk = scene && count_now <= 34 && !hit_slot(n,count_now,scene_x,scene_y);
    qol_context_t c = {
        .gameplay = qol_native_ready(n), .input_enabled = s->permitted,
        .walk_verb = verb == 13,
        .free_walk = empty_walk,
        .sentence_pending = (verb == 3 || verb == 11) &&
            (guest(n, 0x611b) || guest(n, 0x601b)) && guest(n, 0x611d) &&
            !guest(n,0x611c) && !guest(n,0x601c),
        .epoch = (guest(n, 0x63c6) << 8) | guest(n, 0x6100),
        .cursor_x = (int)guest(n, 0x7116), .cursor_y = (int)guest(n, 0x7118)
    };
    if (s->restore_cursor && c.gameplay && s->permitted) {
        qol_native_cursor(n, s->saved_x, s->saved_y);
        s->restore_cursor = false;
        c.cursor_x = s->saved_x; c.cursor_y = s->saved_y;
    }
    if (c.epoch != s->policy.epoch) s->selected_id = 0;
    if (s->highlight) {
        unsigned count = guest(n, 0x63d9);
        unsigned slot = count <= 34 ? hit_slot(n, count,
            ((unsigned)(s->selected.x / 8) + guest(n,0x6c31)) & 255u,
            (unsigned)(((s->selected.y + 1) / 2 - 18) / 4)) : 0;
        unsigned id = slot ? guest(n,0x64f2+slot) | (guest(n,0x64cf+slot)<<8) : 0;
        if (id != s->selected_id || c.cursor_x != s->selected.x || c.cursor_y != s->selected.y)
            s->highlight = false;
        if (!s->highlight) s->selected_id = 0;
    }
    qol_result_t r = qol_input_step(&s->policy, s->options, c, s->input, now);
    if (!c.gameplay || !s->permitted || (s->policy.blocked && !r.chooser_closed)) {
        s->highlight = false;
        s->selected_id = 0;
        s->restore_cursor = false;
        /* Native menus/cutscenes keep their original controls. Only a held
         * gesture returning to verified field play is drained below. */
        if (!c.gameplay || !s->permitted) return;
        n->ram[2] = 0;
        n->prgram[0x710e - 0x6000] = 0;
        n->ram[0x2f] = 0;
        return;
    }
    if (s->options.quick_verbs) {
        /* $2F is a native B/Select/Start edge group in guest bit order. */
        if (s->policy.select_pending || s->policy.chooser || r.chooser_closed)
            n->ram[0x2f] &= (uint8_t)~0x20;
        if ((r.pad & PAD_SELECT) && !(s->input & PAD_SELECT))
            n->ram[0x2f] |= 0x20;
    }
    if (s->policy.chooser || r.chooser_closed) {
        n->ram[2] &= 0x80; /* consume host chooser directions/B/Select/Start */
        n->prgram[0x710e - 0x6000] = 0;
        n->ram[0x2f] = 0;
    }
    if (r.select_verb) {
        static const unsigned ids[QOL_COUNT] = {13,9,10,3,1,2,14,11,12,4,5,7};
        s->saved_x = c.cursor_x; s->saved_y = c.cursor_y;
        if (qol_native_verb_cursor(n, ids[r.verb])) {
            n->prgram[0x710e - 0x6000] = 0x80;
            s->restore_cursor = true;
        }
    }
    uint8_t directions = (uint8_t)(s->input & (PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT));
    if (s->options.object_snap && scene && verb != 7 && directions && (s->input & PAD_B)) {
        n->ram[2] &= (uint8_t)~0x40;
        n->ram[0x2f] &= (uint8_t)~0x40;
    }
    if (s->options.object_snap && scene && verb != 7 &&
        !(s->options.hold_walk && verb == 13 && (s->input & PAD_A)) && !s->policy.chooser && !s->policy.select_pending &&
        !(s->input & (PAD_B|PAD_SELECT|PAD_START))) {
        n->ram[2] &= 0xf0; /* snapping owns scene directions; B keeps free cursor */
        uint8_t edge = (uint8_t)(directions & ~old);
        if (edge) {
            qol_direction_t d = edge & PAD_UP ? QOL_UP : edge & PAD_DOWN ? QOL_DOWN :
                edge & PAD_LEFT ? QOL_LEFT : QOL_RIGHT;
            qol_target_t targets[34], chosen;
            size_t count = qol_native_targets(n, targets, 34);
            if (qol_snap(targets,count,s->selected_id,c.cursor_x,c.cursor_y,16,35,240,175,d,&chosen) &&
                qol_native_cursor(n,chosen.x,chosen.y)) {
                s->selected = chosen; s->selected_id = chosen.id; s->highlight = true;
            }
        }
    } else { s->highlight = false; s->selected_id = 0; }
    if (r.walk_update && now - s->last_walk_ms >= 100 && !s->restore_cursor) {
        n->prgram[0x710e - 0x6000] = 0x80;
        s->last_walk_ms = now;
    }
}
void qol_native_update(nes_t *n, qol_native_t *s, qol_options_t options,
                       uint8_t input, bool permitted)
{
    bool enabled = options.hold_walk || options.object_snap || options.quick_verbs;
    if (enabled && (!(guest(n,0x6b0a)&0x10) || guest(n,0x6b08) || guest(n,0x6b09)))
        permitted = false; /* $913F may not run at all during a cutscene. */
    s->options = options; s->input = input; s->permitted = permitted;
    if (enabled && permitted) {
        n->exec_hook = native_hook;
        n->exec_hook_ud = s;
    } else {
        if (n->exec_hook == native_hook && n->exec_hook_ud == s) {
            n->exec_hook = NULL; n->exec_hook_ud = NULL;
        }
        qol_native_init(s);
        /* Returning from a host context always requires neutral. */
        s->policy.blocked = input != 0;
    }
}
