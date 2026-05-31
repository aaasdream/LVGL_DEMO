/**
 * @file  demo.c
 * @brief LVGL 10-Page Showcase — "讓我驚喜" Edition
 *
 * Page  1 – Cyberpunk HUD      : 4 neon-arc system meters + scan animation
 * Page  2 – Music Equalizer    : 24 animated bars + player controls
 * Page  3 – Smart Home         : 6 room-cards with arc gauges
 * Page  4 – Race Dashboard     : Speedometer + RPM + live telemetry
 * Page  5 – Weather Station    : Forecast + animated sun/cloud
 * Page  6 – Analog Clock       : Canvas: smooth hands, tick marks
 * Page  7 – Matrix Rain        : Canvas: 駭客任務 green-rain
 * Page  8 – Particle Fireworks : Canvas: gravity-particle explosions
 * Page  9 – Pong Game          : Canvas: AI vs AI pong with score
 * Page 10 – Oscilloscope       : Canvas: multi-channel waveform analyser
 */

#include "demo.h"
#include "lvgl/lvgl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═══════════════════════════════════════════════════════════
   CANVAS BUFFERS  (ARGB8888, persist for session lifetime)
   ═══════════════════════════════════════════════════════════ */
#define CV_W 740
#define CV_H 370

LV_ATTRIBUTE_MEM_ALIGN static uint8_t clock_buf [360 * 360 * 4];
LV_ATTRIBUTE_MEM_ALIGN static uint8_t matrix_buf[CV_W * CV_H * 4];
LV_ATTRIBUTE_MEM_ALIGN static uint8_t fire_buf  [CV_W * CV_H * 4];
LV_ATTRIBUTE_MEM_ALIGN static uint8_t pong_buf  [CV_W * CV_H * 4];
LV_ATTRIBUTE_MEM_ALIGN static uint8_t osc_buf   [CV_W * CV_H * 4];

/* canvas widget pointers */
static lv_obj_t *g_clock_cv  = NULL;
static lv_obj_t *g_matrix_cv = NULL;
static lv_obj_t *g_fire_cv   = NULL;
static lv_obj_t *g_pong_cv   = NULL;
static lv_obj_t *g_osc_cv    = NULL;

/* ═══════════════════════════════════════════════════════════
   SHARED DRAWING HELPERS
   ═══════════════════════════════════════════════════════════ */

#define CANVAS_BEGIN(cv)   lv_layer_t _lyr; lv_canvas_init_layer((cv), &_lyr)
#define CANVAS_END(cv)     lv_canvas_finish_layer((cv), &_lyr)
#define LYR                (&_lyr)

static void h_fill(lv_layer_t *l, int32_t x, int32_t y,
                   int32_t w, int32_t h, lv_color_t c, lv_opa_t opa)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = c; d.bg_opa = opa; d.radius = 0; d.border_width = 0;
    lv_area_t a = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(l, &d, &a);
}

static void h_line(lv_layer_t *l,
                   int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                   lv_color_t c, int32_t w)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.color = c; d.width = w; d.opa = LV_OPA_COVER;
    d.p1.x = (lv_value_precise_t)x1; d.p1.y = (lv_value_precise_t)y1;
    d.p2.x = (lv_value_precise_t)x2; d.p2.y = (lv_value_precise_t)y2;
    lv_draw_line(l, &d);
}

static void h_arc(lv_layer_t *l, int32_t cx, int32_t cy, uint16_t r,
                  lv_value_precise_t sa, lv_value_precise_t ea,
                  lv_color_t c, int32_t w)
{
    lv_draw_arc_dsc_t d; lv_draw_arc_dsc_init(&d);
    d.color = c; d.width = w; d.opa = LV_OPA_COVER;
    d.center.x = cx; d.center.y = cy;
    d.radius = r; d.start_angle = sa; d.end_angle = ea;
    lv_draw_arc(l, &d);
}

static void h_text(lv_layer_t *l, int32_t x, int32_t y, int32_t w,
                   const char *txt, lv_color_t c, const lv_font_t *f)
{
    lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d);
    d.color = c; d.font = f; d.opa = LV_OPA_COVER;
    d.text  = txt;
    lv_area_t a = {x, y, x + w - 1, y + (int32_t)f->line_height + 4};
    lv_draw_label(l, &d, &a);
}

static void h_circle(lv_layer_t *l, int32_t cx, int32_t cy, int32_t r,
                     lv_color_t c, lv_opa_t opa)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = c; d.bg_opa = opa; d.radius = LV_RADIUS_CIRCLE; d.border_width = 0;
    lv_area_t a = {cx - r, cy - r, cx + r, cy + r};
    lv_draw_rect(l, &d, &a);
}

/* Dark card helper */
static lv_obj_t *mk_card(lv_obj_t *parent, int32_t w, int32_t h, uint32_t bg)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(c, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_pad_all(c, 10, 0);
    lv_obj_set_style_shadow_width(c, 0, 0);
    return c;
}

/* ═══════════════════════════════════════════════════════════
   PAGE 1 – CYBERPUNK HUD
   ═══════════════════════════════════════════════════════════ */

typedef struct { lv_obj_t *arc, *val; int32_t target, cur; } HudMeter;
static HudMeter  g_hud[4];
static lv_obj_t *g_scan_arc   = NULL;
static lv_obj_t *g_hud_online = NULL;
static int32_t   g_scan_ang   = 0;
static uint32_t  g_hud_blink  = 0;

static void hud_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    static uint32_t step = 0; step++;
    if ((step % 14) == 0) {
        g_hud[0].target = 20 + rand() % 80;
        g_hud[1].target = 40 + rand() % 45;
        g_hud[2].target = rand() % 95;
        g_hud[3].target = 30 + rand() % 55;
    }
    for (int i = 0; i < 4; i++) {
        int32_t d = g_hud[i].target - g_hud[i].cur;
        g_hud[i].cur += d / 6 + (d > 0 ? 1 : -1);
        g_hud[i].cur = LV_MAX(0, LV_MIN(100, g_hud[i].cur));
        if (g_hud[i].arc) lv_arc_set_value(g_hud[i].arc, g_hud[i].cur);
        if (g_hud[i].val) {
            char b[8]; lv_snprintf(b, 8, "%d%%", (int)g_hud[i].cur);
            lv_label_set_text(g_hud[i].val, b);
        }
    }
    g_scan_ang = (g_scan_ang + 8) % 360;
    if (g_scan_arc) lv_arc_set_angles(g_scan_arc, g_scan_ang, (g_scan_ang + 50) % 360);
    g_hud_blink++;
    if (g_hud_online) {
        uint32_t clr = ((g_hud_blink / 5) % 2) ? 0x00FF41 : 0x0A2010;
        lv_obj_set_style_text_color(g_hud_online, lv_color_hex(clr), 0);
    }
}

static void make_hud_meter(lv_obj_t *parent, int idx,
                           const char *name, uint32_t hex, int32_t init)
{
    lv_obj_t *card = mk_card(parent, 190, 205, 0x030810);
    lv_obj_set_style_border_color(card, lv_color_hex(hex), 0);
    lv_obj_set_style_shadow_width(card, 20, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(hex), 0);
    lv_obj_set_style_shadow_opa(card, 100, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 4, 0);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, lv_color_hex(hex), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    lv_obj_t *arc = lv_arc_create(card);
    lv_obj_set_size(arc, 145, 145);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, init);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc, lv_color_hex(hex), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x060C18), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(arc, lv_color_hex(0x030810), 0);

    if (idx == 0) {
        g_scan_arc = lv_arc_create(arc);
        lv_obj_set_size(g_scan_arc, 145, 145);
        lv_obj_set_pos(g_scan_arc, 0, 0);
        lv_arc_set_bg_angles(g_scan_arc, 0, 0);
        lv_arc_set_angles(g_scan_arc, 0, 50);
        lv_obj_remove_style(g_scan_arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(g_scan_arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_color(g_scan_arc, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(g_scan_arc, 2, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(g_scan_arc, 55, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(g_scan_arc, lv_color_hex(0x030810), LV_PART_MAIN);
        lv_obj_set_style_arc_width(g_scan_arc, 12, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_scan_arc, LV_OPA_TRANSP, 0);
    }

    lv_obj_t *val = lv_label_create(arc);
    char b[8]; lv_snprintf(b, 8, "%d%%", (int)init);
    lv_label_set_text(val, b);
    lv_obj_set_style_text_color(val, lv_color_hex(0xDDF0FF), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_obj_center(val);

    g_hud[idx].arc = arc;
    g_hud[idx].val = val;
    g_hud[idx].cur = init;
    g_hud[idx].target = init;
}

static void page_hud(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x020612), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 10, 0);
    lv_obj_set_style_pad_column(parent, 14, 0);

    const char    *names[] = { "CPU", "MEMORY", "NETWORK", "DISK I/O" };
    const uint32_t hexs[]  = { 0x00FFFF, 0xFF00FF, 0x00FF41, 0xFFD700 };
    const int32_t  inits[] = { 37, 62, 18, 44 };
    for (int i = 0; i < 4; i++)
        make_hud_meter(parent, i, names[i], hexs[i], inits[i]);

    lv_obj_t *bar = mk_card(parent, LV_PCT(100), 50, 0x020612);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    g_hud_online = lv_label_create(bar);
    lv_label_set_text(g_hud_online, LV_SYMBOL_OK "  SYSTEM ONLINE");
    lv_obj_set_style_text_color(g_hud_online, lv_color_hex(0x00FF41), 0);
    const char *stats[] = { "CORE: 3.6GHz", "TEMP: 52°C", "UPLINK: 942Mbps" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *l = lv_label_create(bar);
        lv_label_set_text(l, stats[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(0x3A5070), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    }
    lv_timer_create(hud_cb, 80, NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 2 – MUSIC EQUALIZER
   ═══════════════════════════════════════════════════════════ */

#define EQ_N 24
static lv_obj_t *g_eq_bars[EQ_N];
static lv_obj_t *g_eq_prog  = NULL;
static lv_obj_t *g_eq_time  = NULL;
static uint32_t  g_eq_tick  = 0;

static lv_color_t eq_color(int i)
{
    static const uint32_t pal[] = {
        0x0000FF,0x0040FF,0x0080FF,0x00BFFF,0x00FFFF,0x00FFB0,
        0x00FF80,0x00FF40,0x00FF00,0x40FF00,0x80FF00,0xBFFF00,
        0xFFFF00,0xFFD000,0xFFA000,0xFF8000,0xFF6000,0xFF4000,
        0xFF2000,0xFF0000,0xFF0040,0xFF0080,0xFF00BF,0xFF00FF };
    return lv_color_hex(pal[i % EQ_N]);
}

static void eq_h_cb(void *obj, int32_t v)
{
    lv_obj_t *b = (lv_obj_t *)obj;
    lv_obj_set_height(b, v);
    lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, lv_obj_get_x(b), 0);
}

static void eq_cb(lv_timer_t *t)
{
    LV_UNUSED(t); g_eq_tick++;
    if (g_eq_prog) lv_slider_set_value(g_eq_prog, (int32_t)(g_eq_tick % 300), LV_ANIM_OFF);
    if (g_eq_time) {
        int s = (int)(g_eq_tick / 5);
        char b[16]; lv_snprintf(b, 16, "%d:%02d / 3:47", s/60, s%60);
        lv_label_set_text(g_eq_time, b);
    }
}

static void page_equalizer(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x080010), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);

    lv_obj_t *info = mk_card(parent, LV_PCT(100), 52, 0x100018);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *song = lv_label_create(info);
    lv_label_set_text(song, LV_SYMBOL_AUDIO "  Daft Punk — Harder Better Faster Stronger");
    lv_obj_set_style_text_color(song, lv_color_hex(0xE0C0FF), 0);
    lv_obj_set_style_text_font(song, &lv_font_montserrat_14, 0);
    lv_obj_t *bpm_l = lv_label_create(info);
    lv_label_set_text(bpm_l, "138 BPM");
    lv_obj_set_style_text_color(bpm_l, lv_color_hex(0xFF00FF), 0);

    lv_obj_t *viz = lv_obj_create(parent);
    lv_obj_set_size(viz, LV_PCT(100), 215);
    lv_obj_set_style_bg_color(viz, lv_color_hex(0x05000E), 0);
    lv_obj_set_style_border_color(viz, lv_color_hex(0x300050), 0);
    lv_obj_set_style_border_width(viz, 1, 0);
    lv_obj_set_style_radius(viz, 12, 0);
    lv_obj_set_style_pad_all(viz, 8, 0);
    lv_obj_clear_flag(viz, LV_OBJ_FLAG_SCROLLABLE);

    int bw = 22, bg = 5;
    int total_w = EQ_N * (bw + bg) - bg;
    int sx = (740 - total_w) / 2 - 30;

    for (int i = 0; i < EQ_N; i++) {
        lv_obj_t *b = lv_obj_create(viz);
        int32_t h = 20 + rand() % 160;
        lv_obj_set_size(b, bw, h);
        lv_obj_set_style_bg_color(b, eq_color(i), 0);
        lv_obj_set_style_bg_grad_color(b, lv_color_hex(0x000010), 0);
        lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_radius(b, 3, 0);
        lv_obj_set_style_shadow_width(b, 8, 0);
        lv_obj_set_style_shadow_color(b, eq_color(i), 0);
        lv_obj_set_style_shadow_opa(b, 100, 0);
        lv_obj_set_pos(b, sx + i * (bw + bg), 215 - h - 28);
        g_eq_bars[i] = b;

        lv_anim_t a; lv_anim_init(&a);
        lv_anim_set_var(&a, b);
        lv_anim_set_exec_cb(&a, eq_h_cb);
        lv_anim_set_values(&a, 8 + rand() % 40, 80 + rand() % 130);
        lv_anim_set_duration(&a, 110 + rand() % 280);
        lv_anim_set_playback_duration(&a, 110 + rand() % 280);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_delay(&a, rand() % 500);
        lv_anim_start(&a);
    }

    lv_obj_t *ctrl = mk_card(parent, LV_PCT(100), 92, 0x100018);
    lv_obj_set_flex_flow(ctrl, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ctrl, 6, 0);

    g_eq_prog = lv_slider_create(ctrl);
    lv_obj_set_width(g_eq_prog, LV_PCT(100));
    lv_slider_set_range(g_eq_prog, 0, 300);
    lv_slider_set_value(g_eq_prog, 42, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_eq_prog, lv_color_hex(0xFF00FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g_eq_prog, lv_color_hex(0xFF00FF), LV_PART_KNOB);

    lv_obj_t *br = lv_obj_create(ctrl);
    lv_obj_set_size(br, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(br, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(br, 0, 0);
    lv_obj_set_style_pad_all(br, 0, 0);
    lv_obj_set_flex_flow(br, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(br, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *btns[] = { LV_SYMBOL_PREV, LV_SYMBOL_PLAY, LV_SYMBOL_NEXT,
                            LV_SYMBOL_SHUFFLE, LV_SYMBOL_LOOP };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = lv_button_create(br);
        lv_obj_set_size(b, 42, 34);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x280040), 0);
        lv_obj_set_style_border_color(b, lv_color_hex(0xFF00FF), 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, btns[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFF80FF), 0);
        lv_obj_center(l);
    }

    g_eq_time = lv_label_create(ctrl);
    lv_label_set_text(g_eq_time, "0:42 / 3:47");
    lv_obj_set_style_text_color(g_eq_time, lv_color_hex(0x604060), 0);
    lv_obj_set_style_text_font(g_eq_time, &lv_font_montserrat_12, 0);

    lv_timer_create(eq_cb, 200, NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 3 – SMART HOME
   ═══════════════════════════════════════════════════════════ */

static lv_obj_t *g_room_arcs[6][2];   /* [room][0=temp,1=bright] */
static uint32_t  g_home_tick = 0;

static void home_cb(lv_timer_t *t)
{
    LV_UNUSED(t); g_home_tick++;
    if ((g_home_tick % 25) != 0) return;
    for (int i = 0; i < 6; i++) {
        if (g_room_arcs[i][0]) lv_arc_set_value(g_room_arcs[i][0], 18 + rand() % 10);
        if (g_room_arcs[i][1]) lv_arc_set_value(g_room_arcs[i][1], 10 + rand() % 90);
    }
}

static void page_smarthome(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0A0A14), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 10, 0);
    lv_obj_set_style_pad_column(parent, 10, 0);

    const char    *rnames[] = { "Living Room","Bedroom","Kitchen","Bathroom","Office","Garden" };
    const char    *icons[]  = { LV_SYMBOL_HOME, LV_SYMBOL_AUDIO, LV_SYMBOL_OK,
                                 LV_SYMBOL_WIFI, LV_SYMBOL_LIST, LV_SYMBOL_CALL };
    const uint32_t accent[] = { 0xFB923C,0x818CF8,0xFBBF24,0x34D399,0x60A5FA,0x4ADE80 };
    const int      active[] = { 1,1,0,0,1,0 };
    const int32_t  temps[]  = { 22,20,24,21,23,17 };
    const int32_t  brights[]= { 80,60, 0, 0,90,100 };

    for (int i = 0; i < 6; i++) {
        lv_obj_t *card = mk_card(parent, 232, 175, active[i] ? 0x14182A : 0x0C0E18);
        if (active[i]) {
            lv_obj_set_style_border_color(card, lv_color_hex(accent[i]), 0);
            lv_obj_set_style_shadow_width(card, 16, 0);
            lv_obj_set_style_shadow_color(card, lv_color_hex(accent[i]), 0);
            lv_obj_set_style_shadow_opa(card, 80, 0);
        }
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 4, 0);

        lv_obj_t *hdr = lv_obj_create(card);
        lv_obj_set_size(hdr, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(hdr, 0, 0);
        lv_obj_set_style_pad_all(hdr, 0, 0);
        lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *nl = lv_label_create(hdr);
        char buf[32]; lv_snprintf(buf, 32, "%s  %s", icons[i], rnames[i]);
        lv_label_set_text(nl, buf);
        lv_obj_set_style_text_color(nl, lv_color_hex(accent[i]), 0);
        lv_obj_set_style_text_font(nl, &lv_font_montserrat_12, 0);
        lv_obj_t *sw = lv_switch_create(hdr);
        if (active[i]) lv_obj_add_state(sw, LV_STATE_CHECKED);

        lv_obj_t *gr = lv_obj_create(card);
        lv_obj_set_size(gr, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(gr, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(gr, 0, 0);
        lv_obj_set_style_pad_all(gr, 0, 0);
        lv_obj_set_flex_flow(gr, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(gr, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        for (int g = 0; g < 2; g++) {
            lv_obj_t *a = lv_arc_create(gr);
            lv_obj_set_size(a, 98, 98);
            lv_arc_set_rotation(a, 135); lv_arc_set_bg_angles(a, 0, 270);
            if (g == 0) {
                lv_arc_set_range(a, 15, 30); lv_arc_set_value(a, temps[i]);
                lv_obj_set_style_arc_color(a, lv_color_hex(0xF97316), LV_PART_INDICATOR);
            } else {
                lv_arc_set_range(a, 0, 100); lv_arc_set_value(a, brights[i]);
                lv_obj_set_style_arc_color(a, lv_color_hex(accent[i]), LV_PART_INDICATOR);
            }
            lv_obj_remove_style(a, NULL, LV_PART_KNOB);
            lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_arc_width(a, 8, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(a, lv_color_hex(0x0C100C), LV_PART_MAIN);
            lv_obj_set_style_arc_width(a, 8, LV_PART_MAIN);
            lv_obj_t *vl = lv_label_create(a);
            char tb[8];
            if (g == 0) lv_snprintf(tb, 8, "%d°", (int)temps[i]);
            else        lv_snprintf(tb, 8, "%d%%", (int)brights[i]);
            lv_label_set_text(vl, tb);
            lv_obj_set_style_text_color(vl, lv_color_hex(0xFEF9C3), 0);
            lv_obj_center(vl);
            g_room_arcs[i][g] = a;
        }
    }
    lv_timer_create(home_cb, 200, NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 4 – RACE DASHBOARD
   ═══════════════════════════════════════════════════════════ */

static lv_obj_t *g_spd_arc=NULL, *g_spd_val=NULL;
static lv_obj_t *g_rpm_arc=NULL, *g_rpm_val=NULL;
static lv_obj_t *g_gear_lbl=NULL, *g_lap_lbl=NULL;
static int32_t   g_speed=0, g_speed_t=220;
static int32_t   g_rpm=0,   g_rpm_t=7200;
static uint32_t  g_race_tick=0;

static void race_cb(lv_timer_t *t)
{
    LV_UNUSED(t); g_race_tick++;
    if ((g_race_tick % 60) == 0) {
        g_speed_t = 80 + rand() % 200;
        g_rpm_t   = 2000 + rand() % 7000;
    }
    g_speed += (g_speed_t - g_speed) / 8 + 1;
    g_speed  = LV_MAX(0, LV_MIN(280, g_speed));
    g_rpm   += (g_rpm_t - g_rpm) / 6 + 10;
    g_rpm    = LV_MAX(700, LV_MIN(9000, g_rpm));

    if (g_spd_arc) lv_arc_set_value(g_spd_arc, g_speed);
    if (g_rpm_arc) lv_arc_set_value(g_rpm_arc, g_rpm / 90);
    if (g_spd_val) {
        char b[8]; lv_snprintf(b, 8, "%d", (int)g_speed);
        lv_label_set_text(g_spd_val, b);
    }
    if (g_rpm_val) {
        char b[10]; lv_snprintf(b, 10, "%d", (int)g_rpm);
        lv_label_set_text(g_rpm_val, b);
    }
    if (g_gear_lbl) {
        int gear = 1 + (int)(g_speed / 45); if (gear > 6) gear = 6;
        char b[4]; lv_snprintf(b, 4, "%d", gear);
        lv_label_set_text(g_gear_lbl, b);
    }
    if (g_lap_lbl) {
        uint32_t ms = g_race_tick * 80;
        lv_label_set_text_fmt(g_lap_lbl, "LAP  %d:%02d.%03d",
                              (int)(ms/60000),(int)((ms/1000)%60),(int)(ms%1000));
    }
    if (g_rpm_arc) {
        uint32_t c = (g_rpm > 7500) ? 0xFF1111 : 0x22CC22;
        lv_obj_set_style_arc_color(g_rpm_arc, lv_color_hex(c), LV_PART_INDICATOR);
    }
}

static lv_obj_t *big_arc(lv_obj_t *p, int32_t max, uint32_t col, int32_t sz)
{
    lv_obj_t *a = lv_arc_create(p);
    lv_obj_set_size(a, sz, sz);
    lv_arc_set_rotation(a, 135); lv_arc_set_bg_angles(a, 0, 270);
    lv_arc_set_range(a, 0, max); lv_arc_set_value(a, 0);
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(a, lv_color_hex(col), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(a, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(a, lv_color_hex(0x050505), 0);
    lv_obj_set_style_shadow_width(a, 24, 0);
    lv_obj_set_style_shadow_color(a, lv_color_hex(col), 0);
    lv_obj_set_style_shadow_opa(a, 90, 0);
    return a;
}

static void page_race(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x050505), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 305);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* RPM */
    lv_obj_t *rw = lv_obj_create(row);
    lv_obj_set_size(rw, 275, 275);
    lv_obj_set_style_bg_opa(rw, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rw, 0, 0);
    lv_obj_set_style_pad_all(rw, 0, 0);
    g_rpm_arc = big_arc(rw, 100, 0x22CC22, 275);
    lv_obj_center(g_rpm_arc);
    g_rpm_val = lv_label_create(g_rpm_arc);
    lv_label_set_text(g_rpm_val, "0");
    lv_obj_set_style_text_color(g_rpm_val, lv_color_hex(0xCCFFCC), 0);
    lv_obj_set_style_text_font(g_rpm_val, &lv_font_montserrat_20, 0);
    lv_obj_center(g_rpm_val);
    lv_obj_t *ru = lv_label_create(g_rpm_arc);
    lv_label_set_text(ru, "RPM");
    lv_obj_set_style_text_color(ru, lv_color_hex(0x336633), 0);
    lv_obj_set_style_text_font(ru, &lv_font_montserrat_12, 0);
    lv_obj_align(ru, LV_ALIGN_BOTTOM_MID, 0, -30);

    /* Centre */
    lv_obj_t *ctr = lv_obj_create(row);
    lv_obj_set_size(ctr, 120, 275);
    lv_obj_set_style_bg_opa(ctr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctr, 0, 0);
    lv_obj_set_style_pad_all(ctr, 0, 0);
    lv_obj_set_flex_flow(ctr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ctr, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *gl = lv_label_create(ctr);
    lv_label_set_text(gl, "GEAR");
    lv_obj_set_style_text_color(gl, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_font(gl, &lv_font_montserrat_12, 0);
    g_gear_lbl = lv_label_create(ctr);
    lv_label_set_text(g_gear_lbl, "1");
    lv_obj_set_style_text_color(g_gear_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(g_gear_lbl, &lv_font_montserrat_20, 0);
    const char    *bdg[]  = { "DRS","TC","ABS" };
    const uint32_t bcol[] = { 0x22FF22,0xFF8800,0x2288FF };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_obj_create(ctr);
        lv_obj_set_size(b, 58, 22);
        lv_obj_set_style_bg_color(b, lv_color_hex(bcol[i]), 0);
        lv_obj_set_style_bg_opa(b, 40, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(bcol[i]), 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_t *bl = lv_label_create(b);
        lv_label_set_text(bl, bdg[i]);
        lv_obj_set_style_text_color(bl, lv_color_hex(bcol[i]), 0);
        lv_obj_set_style_text_font(bl, &lv_font_montserrat_12, 0);
        lv_obj_center(bl);
    }

    /* Speed */
    lv_obj_t *sw = lv_obj_create(row);
    lv_obj_set_size(sw, 275, 275);
    lv_obj_set_style_bg_opa(sw, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sw, 0, 0);
    lv_obj_set_style_pad_all(sw, 0, 0);
    g_spd_arc = big_arc(sw, 280, 0xFF4400, 275);
    lv_obj_center(g_spd_arc);
    g_spd_val = lv_label_create(g_spd_arc);
    lv_label_set_text(g_spd_val, "0");
    lv_obj_set_style_text_color(g_spd_val, lv_color_hex(0xFFCCAA), 0);
    lv_obj_set_style_text_font(g_spd_val, &lv_font_montserrat_20, 0);
    lv_obj_center(g_spd_val);
    lv_obj_t *su = lv_label_create(g_spd_arc);
    lv_label_set_text(su, "km/h");
    lv_obj_set_style_text_color(su, lv_color_hex(0x664422), 0);
    lv_obj_set_style_text_font(su, &lv_font_montserrat_12, 0);
    lv_obj_align(su, LV_ALIGN_BOTTOM_MID, 0, -30);

    g_lap_lbl = lv_label_create(parent);
    lv_label_set_text(g_lap_lbl, "LAP  0:00.000");
    lv_obj_set_style_text_color(g_lap_lbl, lv_color_hex(0xFF8800), 0);
    lv_obj_set_style_text_font(g_lap_lbl, &lv_font_montserrat_20, 0);

    lv_timer_create(race_cb, 80, NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 5 – WEATHER STATION
   ═══════════════════════════════════════════════════════════ */

static lv_obj_t *g_sun1=NULL, *g_sun2=NULL;
static int32_t   g_sun_rot=0;

static void weather_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    g_sun_rot = (g_sun_rot + 2) % 360;
    if (g_sun1) lv_arc_set_rotation(g_sun1, g_sun_rot);
    if (g_sun2) lv_arc_set_rotation(g_sun2, (360 - g_sun_rot) % 360);
}

static void page_weather(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0C1628), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 10, 0);
    lv_obj_set_style_pad_column(parent, 12, 0);

    lv_obj_t *mc = mk_card(parent, 365, 350, 0x132238);
    lv_obj_set_style_border_color(mc, lv_color_hex(0x1E4060), 0);
    lv_obj_set_flex_flow(mc, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mc, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(mc, 5, 0);

    lv_obj_t *city = lv_label_create(mc);
    lv_label_set_text(city, LV_SYMBOL_WIFI "  Taipei City");
    lv_obj_set_style_text_color(city, lv_color_hex(0x93C5FD), 0);

    lv_obj_t *sw = lv_obj_create(mc);
    lv_obj_set_size(sw, 140, 140);
    lv_obj_set_style_bg_opa(sw, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sw, 0, 0);

    g_sun1 = lv_arc_create(sw);
    lv_obj_set_size(g_sun1, 140, 140); lv_obj_center(g_sun1);
    lv_arc_set_bg_angles(g_sun1, 0, 0); lv_arc_set_angles(g_sun1, 0, 340);
    lv_obj_remove_style(g_sun1, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(g_sun1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(g_sun1, lv_color_hex(0xFBBF24), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(g_sun1, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_sun1, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_sun1, LV_OPA_TRANSP, 0);

    g_sun2 = lv_arc_create(sw);
    lv_obj_set_size(g_sun2, 108, 108); lv_obj_center(g_sun2);
    lv_arc_set_bg_angles(g_sun2, 0, 0); lv_arc_set_angles(g_sun2, 0, 300);
    lv_arc_set_rotation(g_sun2, 30);
    lv_obj_remove_style(g_sun2, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(g_sun2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(g_sun2, lv_color_hex(0xFDE68A), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(g_sun2, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_sun2, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_sun2, LV_OPA_TRANSP, 0);

    lv_obj_t *disc = lv_obj_create(sw);
    lv_obj_set_size(disc, 70, 70); lv_obj_center(disc);
    lv_obj_set_style_bg_color(disc, lv_color_hex(0xFCD34D), 0);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(disc, 0, 0);
    lv_obj_set_style_shadow_width(disc, 28, 0);
    lv_obj_set_style_shadow_color(disc, lv_color_hex(0xFCD34D), 0);

    lv_obj_t *temp_l = lv_label_create(mc);
    lv_label_set_text(temp_l, "28°C");
    lv_obj_set_style_text_color(temp_l, lv_color_hex(0xFEF3C7), 0);
    lv_obj_set_style_text_font(temp_l, &lv_font_montserrat_20, 0);
    lv_obj_t *cond = lv_label_create(mc);
    lv_label_set_text(cond, "Partly Cloudy  |  Feels like 32°");
    lv_obj_set_style_text_color(cond, lv_color_hex(0x7CB9E8), 0);
    lv_obj_set_style_text_font(cond, &lv_font_montserrat_12, 0);

    lv_obj_t *stats = lv_obj_create(mc);
    lv_obj_set_size(stats, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(stats, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats, 0, 0);
    lv_obj_set_style_pad_all(stats, 0, 0);
    lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stats, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    typedef struct { const char *v, *u; uint32_t c; } Stat;
    const Stat ss[] = {
        {"72%","Humidity",0x38BDF8},{"1013","hPa",0x818CF8},
        {"14km/h","Wind",0x4ADE80},{"8.2km","Vis.",0xFBBF24}
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *sc = lv_obj_create(stats);
        lv_obj_set_size(sc, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(sc, lv_color_hex(0x0C1A2E), 0);
        lv_obj_set_style_border_color(sc, lv_color_hex(ss[i].c), 0);
        lv_obj_set_style_border_width(sc, 1, 0);
        lv_obj_set_style_radius(sc, 8, 0);
        lv_obj_set_style_pad_hor(sc, 8, 0);
        lv_obj_set_style_pad_ver(sc, 4, 0);
        lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(sc, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *sv = lv_label_create(sc); lv_label_set_text(sv, ss[i].v);
        lv_obj_set_style_text_color(sv, lv_color_hex(ss[i].c), 0);
        lv_obj_t *su = lv_label_create(sc); lv_label_set_text(su, ss[i].u);
        lv_obj_set_style_text_color(su, lv_color_hex(0x4A6080), 0);
        lv_obj_set_style_text_font(su, &lv_font_montserrat_12, 0);
    }

    lv_obj_t *fc = lv_obj_create(parent);
    lv_obj_set_size(fc, 340, 350);
    lv_obj_set_style_bg_opa(fc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fc, 0, 0);
    lv_obj_set_style_pad_all(fc, 0, 0);
    lv_obj_set_flex_flow(fc, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(fc, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(fc, 8, 0);

    typedef struct { const char *day,*cond; int hi,lo; uint32_t c; } Day;
    const Day days[] = {
        {"Today","Sunny",28,22,0xFBBF24},{"Tue","Partly Cloudy",27,21,0x93C5FD},
        {"Wed","Rainy",23,18,0x60A5FA},  {"Thu","Stormy",20,16,0xA78BFA},
        {"Fri","Sunny",29,23,0xFBBF24}
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *dc = mk_card(fc, LV_PCT(100), 54, 0x0E1E34);
        lv_obj_set_style_border_color(dc, lv_color_hex(days[i].c), 0);
        lv_obj_set_flex_flow(dc, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(dc, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *dl = lv_label_create(dc); lv_label_set_text(dl, days[i].day);
        lv_obj_set_style_text_color(dl, lv_color_hex(days[i].c), 0);
        lv_obj_set_style_text_font(dl, &lv_font_montserrat_14, 0);
        lv_obj_t *cl = lv_label_create(dc); lv_label_set_text(cl, days[i].cond);
        lv_obj_set_style_text_color(cl, lv_color_hex(0x64748B), 0);
        lv_obj_set_style_text_font(cl, &lv_font_montserrat_12, 0);
        char tb[12]; lv_snprintf(tb, 12, "%d°/%d°", days[i].hi, days[i].lo);
        lv_obj_t *tl = lv_label_create(dc); lv_label_set_text(tl, tb);
        lv_obj_set_style_text_color(tl, lv_color_hex(0xCBD5E1), 0);
    }
    lv_timer_create(weather_cb, 40, NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 6 – ANALOG CLOCK  (canvas)
   ═══════════════════════════════════════════════════════════ */

#define CLK_CX 180
#define CLK_CY 180
#define CLK_R  165

static void draw_clock(lv_layer_t *l, uint32_t ms)
{
    /* Background disc */
    h_circle(l, CLK_CX, CLK_CY, CLK_R + 4, lv_color_hex(0x060616), LV_OPA_COVER);
    h_arc(l, CLK_CX, CLK_CY, CLK_R,     0, 360, lv_color_hex(0x1E3A6A), 3);
    h_arc(l, CLK_CX, CLK_CY, CLK_R - 6, 0, 360, lv_color_hex(0x0D2040), 2);

    /* Tick marks */
    for (int i = 0; i < 60; i++) {
        double a = (i * 6.0 - 90.0) * M_PI / 180.0;
        int is_hour = (i % 5 == 0);
        int r1 = CLK_R - (is_hour ? 16 : 8);
        int x1 = CLK_CX + (int)(r1 * cos(a));
        int y1 = CLK_CY + (int)(r1 * sin(a));
        int x2 = CLK_CX + (int)((CLK_R - 3) * cos(a));
        int y2 = CLK_CY + (int)((CLK_R - 3) * sin(a));
        h_line(l, x1, y1, x2, y2,
               is_hour ? lv_color_hex(0x4A80C0) : lv_color_hex(0x1E3050),
               is_hour ? 3 : 1);
    }

    /* Hour numbers */
    const char *nums[] = {"12","1","2","3","4","5","6","7","8","9","10","11"};
    for (int h = 0; h < 12; h++) {
        double a = (h * 30.0 - 90.0) * M_PI / 180.0;
        int x = CLK_CX + (int)((CLK_R - 32) * cos(a)) - 8;
        int y = CLK_CY + (int)((CLK_R - 32) * sin(a)) - 8;
        h_text(l, x, y, 24, nums[h], lv_color_hex(0x5090D0), &lv_font_montserrat_12);
    }

    uint32_t secs = (ms / 1000) % 60;
    uint32_t mins = (ms / 60000) % 60;
    uint32_t hrs  = (ms / 3600000) % 12;
    float    frac = (ms % 1000) / 1000.0f;

    /* Hour hand */
    double ha = ((hrs + mins / 60.0) * 30.0 - 90.0) * M_PI / 180.0;
    h_line(l, CLK_CX - (int)(18*cos(ha)), CLK_CY - (int)(18*sin(ha)),
              CLK_CX + (int)((CLK_R-55)*cos(ha)), CLK_CY + (int)((CLK_R-55)*sin(ha)),
              lv_color_hex(0x88AAFF), 5);
    /* Minute hand */
    double ma = ((mins + secs / 60.0) * 6.0 - 90.0) * M_PI / 180.0;
    h_line(l, CLK_CX - (int)(18*cos(ma)), CLK_CY - (int)(18*sin(ma)),
              CLK_CX + (int)((CLK_R-25)*cos(ma)), CLK_CY + (int)((CLK_R-25)*sin(ma)),
              lv_color_hex(0xCCDDFF), 3);
    /* Second hand (smooth) */
    double sa = ((secs + frac) * 6.0 - 90.0) * M_PI / 180.0;
    h_line(l, CLK_CX - (int)(22*cos(sa)), CLK_CY - (int)(22*sin(sa)),
              CLK_CX + (int)((CLK_R-10)*cos(sa)), CLK_CY + (int)((CLK_R-10)*sin(sa)),
              lv_color_hex(0xFF3333), 1);

    /* Cap */
    h_circle(l, CLK_CX, CLK_CY, 6, lv_color_hex(0xFF3333), LV_OPA_COVER);
    h_circle(l, CLK_CX, CLK_CY, 3, lv_color_hex(0xFFFFFF), LV_OPA_COVER);

    /* Digital readout */
    char tb[12];
    uint32_t dh = (hrs == 0) ? 12 : hrs;
    lv_snprintf(tb, 12, "%02u:%02u:%02u", (unsigned)dh, (unsigned)mins, (unsigned)secs);
    h_text(l, CLK_CX - 42, CLK_CY + CLK_R + 10, 100, tb,
           lv_color_hex(0x3A78BB), &lv_font_montserrat_20);
}

static void clock_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (!g_clock_cv) return;
    lv_canvas_fill_bg(g_clock_cv, lv_color_hex(0x020208), LV_OPA_COVER);
    CANVAS_BEGIN(g_clock_cv);
    draw_clock(LYR, lv_tick_get());
    CANVAS_END(g_clock_cv);
}

static void page_clock(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x020208), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    g_clock_cv = lv_canvas_create(parent);
    lv_canvas_set_buffer(g_clock_cv, clock_buf, 360, 360, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(g_clock_cv, lv_color_hex(0x020208), LV_OPA_COVER);
    lv_obj_center(g_clock_cv);
    lv_timer_create(clock_cb, 33, NULL);
    clock_cb(NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 7 – MATRIX RAIN  (canvas)
   ═══════════════════════════════════════════════════════════ */

#define MX_COLS  37
#define MX_ROWS  22
#define MX_CELL  20

typedef struct { int head, len, delay; char ch[MX_ROWS + 8]; } MxCol;
static MxCol g_mx[MX_COLS];

static char mx_char(void)
{
    static const char pool[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%&*+-=!<>|^~:.";
    return pool[rand() % (int)(sizeof(pool)-1)];
}

static void matrix_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (!g_matrix_cv) return;
    lv_canvas_fill_bg(g_matrix_cv, lv_color_hex(0x000000), LV_OPA_COVER);
    CANVAS_BEGIN(g_matrix_cv);
    for (int c = 0; c < MX_COLS; c++) {
        MxCol *col = &g_mx[c];
        if (col->delay > 0) { col->delay--; continue; }
        col->head++;
        if (col->head >= MX_ROWS + col->len) {
            col->head  = -1;
            col->len   = 5 + rand() % 14;
            col->delay = rand() % 40;
            for (int r = 0; r < MX_ROWS + 8; r++) col->ch[r] = mx_char();
        }
        for (int r = 0; r < MX_ROWS; r++) {
            int dist = col->head - r;
            if (dist < 0 || dist >= col->len) continue;
            int x = c * MX_CELL + 2;
            int y = r * MX_CELL;
            if ((rand() % 10) == 0) col->ch[r] = mx_char();
            lv_color_t clr;
            if (dist == 0) clr = lv_color_hex(0xCCFFCC);
            else {
                int g2 = 255 - dist * 255 / col->len;
                clr = lv_color_make(0, (uint8_t)LV_MAX(30, g2), 0);
            }
            char ch[2] = { col->ch[r], '\0' };
            h_text(LYR, x, y, MX_CELL, ch, clr, &lv_font_montserrat_12);
        }
    }
    CANVAS_END(g_matrix_cv);
}

static void page_matrix(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *tl = lv_label_create(parent);
    lv_label_set_text(tl, "WAKE UP, NEO...");
    lv_obj_set_style_text_color(tl, lv_color_hex(0x00FF41), 0);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);
    g_matrix_cv = lv_canvas_create(parent);
    lv_canvas_set_buffer(g_matrix_cv, matrix_buf, CV_W, CV_H, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(g_matrix_cv, lv_color_hex(0x000000), LV_OPA_COVER);
    for (int c = 0; c < MX_COLS; c++) {
        g_mx[c].head  = -1;
        g_mx[c].len   = 5 + rand() % 14;
        g_mx[c].delay = rand() % 60;
        for (int r = 0; r < MX_ROWS + 8; r++) g_mx[c].ch[r] = mx_char();
    }
    lv_timer_create(matrix_cb, 55, NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 8 – PARTICLE FIREWORKS  (canvas)
   ═══════════════════════════════════════════════════════════ */

#define MAX_P 320
typedef struct { float x,y,vx,vy; uint32_t col; uint8_t alpha,alive; } Part;
static Part     g_par[MAX_P];
static uint32_t g_fire_tick=0;

static void spawn_fw(float ox, float oy, uint32_t col)
{
    int n = 0;
    for (int i = 0; i < MAX_P && n < 64; i++) {
        if (g_par[i].alive) continue;
        float ang   = (float)(rand() % 6283) / 1000.0f;
        float spd   = 1.5f + (rand() % 40) / 10.0f;
        g_par[i] = (Part){ ox, oy, spd*cosf(ang), spd*sinf(ang)-1.5f,
                            col, 255, 1 };
        n++;
    }
}

static void fire_cb(lv_timer_t *t)
{
    LV_UNUSED(t); g_fire_tick++;
    if ((g_fire_tick % 28) == 0) {
        float ox = 80.0f + (rand() % (CV_W - 160));
        float oy = 50.0f + (rand() % (CV_H / 2));
        const uint32_t cols[] = {0xFF4444,0x44FF44,0x4488FF,0xFFFF44,
                                  0xFF44FF,0x44FFFF,0xFF8844,0x88FF44};
        spawn_fw(ox, oy, cols[rand() % 8]);
    }
    for (int i = 0; i < MAX_P; i++) {
        if (!g_par[i].alive) continue;
        g_par[i].x  += g_par[i].vx;
        g_par[i].y  += g_par[i].vy;
        g_par[i].vy += 0.09f;
        g_par[i].vx *= 0.98f;
        if (g_par[i].alpha > 7) g_par[i].alpha -= 7;
        else g_par[i].alive = 0;
    }
    lv_canvas_fill_bg(g_fire_cv, lv_color_hex(0x020208), LV_OPA_COVER);
    CANVAS_BEGIN(g_fire_cv);
    for (int i = 0; i < MAX_P; i++) {
        if (!g_par[i].alive) continue;
        int x = (int)g_par[i].x, y = (int)g_par[i].y;
        if (x<0||x>=CV_W-3||y<0||y>=CV_H-3) continue;
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(g_par[i].col);
        d.bg_opa   = (lv_opa_t)g_par[i].alpha;
        d.radius   = LV_RADIUS_CIRCLE; d.border_width = 0;
        lv_area_t a = {x,y,x+3,y+3};
        lv_draw_rect(LYR, &d, &a);
    }
    CANVAS_END(g_fire_cv);
}

static void page_fireworks(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x020208), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *tl = lv_label_create(parent);
    lv_label_set_text(tl, LV_SYMBOL_PLAY "  Particle Fireworks  " LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(tl, lv_color_hex(0xFFDD44), 0);
    g_fire_cv = lv_canvas_create(parent);
    lv_canvas_set_buffer(g_fire_cv, fire_buf, CV_W, CV_H, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(g_fire_cv, lv_color_hex(0x020208), LV_OPA_COVER);
    memset(g_par, 0, sizeof(g_par));
    lv_timer_create(fire_cb, 33, NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 9 – PONG GAME  (canvas, AI vs AI)
   ═══════════════════════════════════════════════════════════ */

#define PG_W  CV_W
#define PG_H  CV_H
#define PAD_H 60
#define PAD_W 10
#define BALL_R 5

typedef struct { float x,y,vx,vy; } Ball;
static Ball     g_ball;
static int32_t  g_pl, g_pr;
static int      g_sl=0, g_sr=0;

static void reset_ball(void)
{
    g_ball.x = PG_W / 2.0f; g_ball.y = PG_H / 2.0f;
    float dir = (rand()%2) ? 1.0f : -1.0f;
    float ang = ((rand()%50)-25) * (float)M_PI / 180.0f;
    g_ball.vx = dir * (3.8f + cosf(fabsf(ang)));
    g_ball.vy = sinf(ang) * 3.0f;
}

static void pong_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (!g_pong_cv) return;

    /* AI: imperfect tracking */
    float tl = g_ball.y - PAD_H / 2.0f;
    float tr = g_ball.y - PAD_H / 2.0f;
    g_pl += (int32_t)((tl - g_pl) * 0.08f);
    g_pr += (int32_t)((tr - g_pr) * 0.10f);
    g_pl  = LV_MAX(0, LV_MIN(PG_H - PAD_H, g_pl));
    g_pr  = LV_MAX(0, LV_MIN(PG_H - PAD_H, g_pr));

    g_ball.x += g_ball.vx; g_ball.y += g_ball.vy;
    if (g_ball.y <= BALL_R)          { g_ball.y = BALL_R;          g_ball.vy = fabsf(g_ball.vy); }
    if (g_ball.y >= PG_H - BALL_R)  { g_ball.y = PG_H - BALL_R;  g_ball.vy = -fabsf(g_ball.vy); }

    int PLX = 18, PRX = PG_W - 18 - PAD_W;
    if (g_ball.x-BALL_R <= PLX+PAD_W && g_ball.x >= PLX &&
        g_ball.y+BALL_R >= g_pl && g_ball.y-BALL_R <= g_pl+PAD_H) {
        g_ball.vx = fabsf(g_ball.vx) * 1.05f;
        g_ball.vy = ((g_ball.y - (g_pl + PAD_H/2.0f)) / (PAD_H/2.0f)) * 4.5f;
    }
    if (g_ball.x+BALL_R >= PRX && g_ball.x+BALL_R <= PRX+PAD_W+4 &&
        g_ball.y+BALL_R >= g_pr && g_ball.y-BALL_R <= g_pr+PAD_H) {
        g_ball.vx = -fabsf(g_ball.vx) * 1.05f;
        g_ball.vy = ((g_ball.y - (g_pr + PAD_H/2.0f)) / (PAD_H/2.0f)) * 4.5f;
    }
    if (fabsf(g_ball.vx) > 10.0f) g_ball.vx = (g_ball.vx>0) ? 10.0f : -10.0f;
    if (g_ball.x < 0)      { g_sr++; reset_ball(); }
    if (g_ball.x >= PG_W)  { g_sl++; reset_ball(); }

    lv_canvas_fill_bg(g_pong_cv, lv_color_hex(0x060610), LV_OPA_COVER);
    CANVAS_BEGIN(g_pong_cv);

    /* Centre dashes */
    for (int y = 0; y < PG_H; y += 20)
        h_fill(LYR, PG_W/2-1, y, 3, 10, lv_color_hex(0x1A2A4A), LV_OPA_COVER);

    /* Score */
    char sc[8];
    lv_snprintf(sc, 8, "%d", g_sl);
    h_text(LYR, PG_W/4-10, 8, 40, sc, lv_color_hex(0xCCDDFF), &lv_font_montserrat_20);
    lv_snprintf(sc, 8, "%d", g_sr);
    h_text(LYR, 3*PG_W/4-10, 8, 40, sc, lv_color_hex(0xCCDDFF), &lv_font_montserrat_20);

    /* Paddles with glow */
    lv_draw_rect_dsc_t pd; lv_draw_rect_dsc_init(&pd);
    pd.radius=4; pd.border_width=0;
    pd.shadow_width=14;
    pd.bg_color=lv_color_hex(0x44AAFF); pd.bg_opa=LV_OPA_COVER;
    pd.shadow_color=lv_color_hex(0x44AAFF); pd.shadow_opa=180;
    lv_area_t la={PLX, g_pl, PLX+PAD_W-1, g_pl+PAD_H-1};
    lv_draw_rect(LYR, &pd, &la);
    pd.bg_color=lv_color_hex(0xFF6644);
    pd.shadow_color=lv_color_hex(0xFF6644);
    lv_area_t ra={PRX, g_pr, PRX+PAD_W-1, g_pr+PAD_H-1};
    lv_draw_rect(LYR, &pd, &ra);

    /* Ball with glow */
    int bx=(int)g_ball.x, by=(int)g_ball.y;
    h_circle(LYR, bx, by, BALL_R+5, lv_color_hex(0xFFFFFF), 40);
    h_circle(LYR, bx, by, BALL_R,   lv_color_hex(0xFFFFFF), LV_OPA_COVER);

    CANVAS_END(g_pong_cv);
}

static void page_pong(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x060610), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 4, 0);
    lv_obj_set_style_pad_row(parent, 4, 0);
    lv_obj_t *hdr = lv_label_create(parent);
    lv_label_set_text(hdr, "PONG  —  AI vs AI");
    lv_obj_set_style_text_color(hdr, lv_color_hex(0x3366AA), 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
    g_pong_cv = lv_canvas_create(parent);
    lv_canvas_set_buffer(g_pong_cv, pong_buf, CV_W, CV_H, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(g_pong_cv, lv_color_hex(0x060610), LV_OPA_COVER);
    g_pl = PG_H/2 - PAD_H/2;
    g_pr = PG_H/2 - PAD_H/2;
    reset_ball();
    lv_timer_create(pong_cb, 16, NULL);
}

/* ═══════════════════════════════════════════════════════════
   PAGE 10 – OSCILLOSCOPE  (canvas)
   ═══════════════════════════════════════════════════════════ */

static float g_osc_ph  = 0.0f;
static float g_osc_mrp = 0.0f;

static void osc_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (!g_osc_cv) return;
    g_osc_ph  += 0.04f;
    g_osc_mrp += 0.006f;

    lv_canvas_fill_bg(g_osc_cv, lv_color_hex(0x020610), LV_OPA_COVER);
    CANVAS_BEGIN(g_osc_cv);

    /* Grid */
    for (int x = 0; x < CV_W; x += 60)
        h_line(LYR, x, 0, x, CV_H-1, lv_color_hex(0x0A1830), 1);
    for (int y = 0; y < CV_H; y += 40)
        h_line(LYR, 0, y, CV_W-1, y, lv_color_hex(0x0A1830), 1);
    h_line(LYR, 0, CV_H/2, CV_W-1, CV_H/2, lv_color_hex(0x1A3050), 1);

    typedef struct { float freq,amp,poff; uint32_t col; int yoff; } Chan;
    const Chan ch[] = {
        {2.0f, 0.88f, 0.0f,            0xFF4444, 0     },
        {3.1f, 0.54f, (float)M_PI/4,   0x44FF44, 0     },
        {5.0f, 0.34f, (float)M_PI/3,   0x4488FF, 0     },
        {1.5f, 0.68f, (float)M_PI/6,   0xFFAA22, CV_H/5},
    };
    int nch = 4;
    for (int c = 0; c < nch; c++) {
        int px0=-1, py0=-1;
        for (int px = 0; px < CV_W; px++) {
            float tf = (float)px / CV_W * 2.0f*(float)M_PI * ch[c].freq + g_osc_ph;
            float h2 = sinf(tf*2.0f + g_osc_mrp) * 0.28f;
            float v  = sinf(tf + ch[c].poff) * ch[c].amp + h2;
            int y = (int)(CV_H/2 + v*(CV_H/2-20)) + ch[c].yoff;
            y = LV_MAX(2, LV_MIN(CV_H-3, y));
            if (px0 >= 0) h_line(LYR, px0, py0, px, y, lv_color_hex(ch[c].col), 2);
            px0=px; py0=y;
        }
    }

    /* Labels */
    const char    *chn[]  = {"CH1","CH2","CH3","CH4"};
    const uint32_t chcol[]= {0xFF4444,0x44FF44,0x4488FF,0xFFAA22};
    for (int c = 0; c < nch; c++)
        h_text(LYR, 4, 4+c*18, 36, chn[c], lv_color_hex(chcol[c]), &lv_font_montserrat_12);

    h_text(LYR, CV_W-200, 4, 196, "SIGNAL ANALYSER v2.4",
           lv_color_hex(0x224466), &lv_font_montserrat_12);

    char fb[24];
    lv_snprintf(fb, 24, "%.2f MHz", (double)(2.0f + sinf(g_osc_mrp)*0.4f));
    h_text(LYR, CV_W-140, CV_H-18, 138, fb, lv_color_hex(0xFF4444), &lv_font_montserrat_12);

    CANVAS_END(g_osc_cv);
}

static void page_osc(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x020610), 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 4, 0);
    lv_obj_set_style_pad_row(parent, 4, 0);
    lv_obj_t *hdr = lv_label_create(parent);
    lv_label_set_text(hdr, "OSCILLOSCOPE  —  Multi-Channel Waveform Analysis");
    lv_obj_set_style_text_color(hdr, lv_color_hex(0x224466), 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
    g_osc_cv = lv_canvas_create(parent);
    lv_canvas_set_buffer(g_osc_cv, osc_buf, CV_W, CV_H, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(g_osc_cv, lv_color_hex(0x020610), LV_OPA_COVER);
    lv_timer_create(osc_cb, 33, NULL);
    osc_cb(NULL);
}

/* ═══════════════════════════════════════════════════════════
   ENTRY POINT
   ═══════════════════════════════════════════════════════════ */
void my_demo_init(void)
{
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x050510), 0);

    lv_obj_t *tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tv, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tv, 42);

    lv_obj_t *bar = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x050510), 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x1A2A40), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_text_color(bar, lv_color_hex(0x607090), 0);
    lv_obj_set_style_text_font(bar, &lv_font_montserrat_12, 0);

    lv_obj_t *t1  = lv_tabview_add_tab(tv, "HUD");
    lv_obj_t *t2  = lv_tabview_add_tab(tv, "EQ");
    lv_obj_t *t3  = lv_tabview_add_tab(tv, "HOME");
    lv_obj_t *t4  = lv_tabview_add_tab(tv, "RACE");
    lv_obj_t *t5  = lv_tabview_add_tab(tv, "WEATHER");
    lv_obj_t *t6  = lv_tabview_add_tab(tv, "CLOCK");
    lv_obj_t *t7  = lv_tabview_add_tab(tv, "MATRIX");
    lv_obj_t *t8  = lv_tabview_add_tab(tv, "FIREWORKS");
    lv_obj_t *t9  = lv_tabview_add_tab(tv, "PONG");
    lv_obj_t *t10 = lv_tabview_add_tab(tv, "SCOPE");

    page_hud(t1);
    page_equalizer(t2);
    page_smarthome(t3);
    page_race(t4);
    page_weather(t5);
    page_clock(t6);
    page_matrix(t7);
    page_fireworks(t8);
    page_pong(t9);
    page_osc(t10);
}
