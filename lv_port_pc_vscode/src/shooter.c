/**
 * shooter.c  — LVGL v9  橫向卷軸飛機射擊  (Sprite Edition v3)
 * 800 x 480  PC Simulator
 *
 * Sprite 來源：Twemoji (CC-BY 4.0)  https://twemoji.twitter.com/
 * 由 emoji2lvgl.py 轉換成 LVGL ARGB8888 C 陣列
 *
 * 操作：
 *   單擊  → 飛機移到 (tap_x - 100, tap_y)
 *   雙擊  → 大絕（全螢幕衝擊波）
 */

#include "lvgl/lvgl.h"
#include "sprites/sprites.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ════════════════════════════════════════
   常數
   ════════════════════════════════════════ */
#define SCR_W            800
#define SCR_H            480
#define SCORE_H           50
#define PLAY_Y           SCORE_H
#define PLAY_H           (SCR_H - SCORE_H)

/* Sprite 碰撞框（比圖片小一圈）*/
#define PLAYER_CW         38
#define PLAYER_CH         22
#define ENEMY_CW          28
#define ENEMY_CH          28
#define BOSS1_CW          64
#define BOSS1_CH          56
#define BOSS2_CW          70
#define BOSS2_CH          60

#define MAX_BULLETS       80
#define MAX_ENEMIES       20
#define MAX_EXPLOSIONS    12
#define MAX_SCORE_FLY     16
#define MAX_WINGMEN        2
#define MAX_BOSS_BULLETS  30

#define BULLET_SPD        16
#define ENEMY_SPD_BASE     2
#define BOSS_BULLET_SPD    5

#define SPAWN_INTERVAL_MS   900   /* 縮短生成間隔，敵機更密 */
#define SHOOT_INTERVAL_MS   180
#define WINGMAN_SHOOT_MS    300

#define STAGE_WAVE_SEC       30   /* 每關普通波次持續秒數 */
#define ULTIMATE_MAX          3   /* 大絕初始數量 */
#define MAX_ENEMY_BULLETS    40   /* 普通敵機飛彈池 */
#define ENEMY_BULLET_SPD      4   /* 敵機飛彈速度 */
#define ENEMY_SHOOT_MS_MIN 3000   /* 敵機最短射擊間隔 ms */
#define ENEMY_SHOOT_MS_MAX 7000   /* 敵機最長射擊間隔 ms */

#define PLAYER_MAX_HP        5
#define BOSS1_MAX_HP        20
#define BOSS2_MAX_HP        35

/* ════════════════════════════════════════
   型別
   ════════════════════════════════════════ */
typedef enum { GS_TITLE = 0, GS_PLAYING, GS_GAMEOVER } GameState;

typedef struct {
    bool      alive;
    float     x, y;    /* 中心座標 */
    int       hp;
    lv_obj_t *sprite;  /* lv_image widget */
    int       type;    /* 敵機型別 0,1,2=普通  3=補給 */
    uint32_t  next_shoot_ms;  /* 下次射擊絕對時間戳 */
} Entity;

typedef struct {
    bool      alive;
    float     x, y;
    float     vx, vy;
    lv_obj_t *obj;  /* lv_obj (solid rect) */
} Bullet;

typedef struct {
    bool      alive;
    float     x, y;
    float     vx, vy;
    lv_obj_t *obj;
} BossBullet;

typedef struct {
    bool      alive;
    float     x, y;
    lv_obj_t *sprite;
} Wingman;

typedef struct {
    bool      alive;
    float     x, y;
    uint32_t  start_ms;
    lv_obj_t *sprite;
} Explosion;

typedef struct {
    bool      alive;
    int       value;
    float     x, y;
    float     tx, ty;
    lv_obj_t *label;
} ScoreFly;

/* ════════════════════════════════════════
   全域狀態
   ════════════════════════════════════════ */
static GameState   g_state = GS_TITLE;
static lv_obj_t   *g_scr   = NULL;
static lv_obj_t   *g_bg    = NULL;
static lv_obj_t   *g_input_catcher = NULL;

/* UI */
static lv_obj_t   *g_score_bar    = NULL;
static lv_obj_t   *g_score_lbl    = NULL;
static lv_obj_t   *g_hp_cont      = NULL;   /* 血量圖示容器 */
static lv_obj_t   *g_hp_icons[PLAYER_MAX_HP];
static lv_obj_t   *g_boss_hp_lbl  = NULL;
static lv_obj_t   *g_title_panel  = NULL;
static lv_obj_t   *g_go_panel     = NULL;
static lv_obj_t   *g_overlay      = NULL;

/* 星星 */
#define MAX_STARS 80
static lv_obj_t   *g_stars[MAX_STARS];
static float       g_star_x[MAX_STARS];
static float       g_star_spd[MAX_STARS];

/* 玩家 */
static Entity      g_player;
static float       g_player_tx, g_player_ty;
static int         g_hp;
static int         g_bullet_mode;
static uint32_t    g_last_shoot_ms;
static uint32_t    g_last_wm_shoot_ms;
static bool        g_invincible;
static uint32_t    g_invincible_ms;
static uint32_t    g_last_tap_ms;
static int         g_tap_count;

/* 子彈 */
static Bullet      g_bullets[MAX_BULLETS];

/* 子機 */
static Wingman     g_wingmen[MAX_WINGMEN];
static int         g_wingman_count;

/* 敵機 */
static Entity      g_enemies[MAX_ENEMIES];
static uint32_t    g_last_spawn_ms;

/* Boss */
static Entity      g_boss1, g_boss2;
static bool        g_boss1_dead, g_boss2_dead;
static BossBullet  g_boss_bullets[MAX_BOSS_BULLETS];
static uint32_t    g_boss_shoot_ms;
static float       g_boss1_vy;

/* 關卡波次系統 */
static int         g_stage;             /* 目前關卡 1, 2, 3(無盡) */
static uint32_t    g_stage_start_ms;    /* 本波普通敵機開始時間 */
static bool        g_boss_wave;         /* 是否已進入 Boss 波次 */

/* 大絕 */
static int         g_ultimates;         /* 剩餘大絕數量 */
static lv_obj_t   *g_ult_lbl   = NULL;  /* 左下角顯示 */

/* 波次提示 */
static lv_obj_t   *g_wave_lbl  = NULL;
static uint32_t    g_wave_lbl_ms;

/* 敵機飛彈池 */
static BossBullet  g_enemy_bullets[MAX_ENEMY_BULLETS];

/* 爆炸 */
static Explosion   g_explosions[MAX_EXPLOSIONS];

/* 飛分數 */
static ScoreFly    g_score_fly[MAX_SCORE_FLY];

/* 分數 */
static int         g_score;
static bool        g_score_flash;
static uint32_t    g_score_flash_ms;

/* 計時器 */
static lv_timer_t *g_game_timer = NULL;

/* ════════════════════════════════════════
   前向宣告
   ════════════════════════════════════════ */
static void game_start(void);
static void game_over(void);
static void game_tick(lv_timer_t *t);
static void on_click(lv_event_t *e);
static void spawn_enemy(void);
static void spawn_boss1(void);
static void spawn_boss2(void);
static void shoot_player(void);
static void shoot_wingman(int wm);
static void shoot_boss(void);
static void add_score(int val, float fx, float fy);
static void spawn_explosion(float cx, float cy);
static void do_ultimate(void);
static void update_stars(void);
static void check_collisions(void);
static void update_hp_icons(void);
static void update_boss_hp(void);
static void update_ult_ui(void);
static void show_wave_msg(const char *msg);
static void shoot_enemy(int idx);

/* ════════════════════════════════════════
   工具函式
   ════════════════════════════════════════ */
static inline float fclampf(float v, float lo, float hi) { return v<lo?lo:v>hi?hi:v; }
static inline float lerpf(float a, float b, float t) { return a+(b-a)*t; }
static void safe_del(lv_obj_t **p) { if(*p){lv_obj_del(*p);*p=NULL;} }
static void set_img_center(lv_obj_t *obj, float x, float y) {
    lv_obj_set_pos(obj,
        (int32_t)(x - lv_obj_get_width(obj)  / 2),
        (int32_t)(y - lv_obj_get_height(obj) / 2));
}

/* 建立 lv_image 並設定 src，放在 parent 上 */
static lv_obj_t *mk_img(lv_obj_t *parent, const lv_image_dsc_t *src) {
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

/* 建立子彈（純色填充矩形）*/
static lv_obj_t *mk_bullet_obj(lv_obj_t *parent, int w, int h, lv_color_t col) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, h/2, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* ════════════════════════════════════════
   初始化入口
   ════════════════════════════════════════ */
void shooter_game_init(void) {
    g_scr = lv_screen_active();
    lv_obj_set_style_bg_color(g_scr, lv_color_hex(0x020210), 0);
    lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 背景 */
    g_bg = lv_obj_create(g_scr);
    lv_obj_set_size(g_bg, SCR_W, SCR_H);
    lv_obj_set_pos(g_bg, 0, 0);
    lv_obj_set_style_bg_color(g_bg, lv_color_hex(0x020210), 0);
    lv_obj_set_style_bg_opa(g_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_bg, 0, 0);
    lv_obj_remove_flag(g_bg, LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);

    /* 星星 */
    srand(42);
    static const char *star_chars[] = {".", "+", "*"};
    for (int i = 0; i < MAX_STARS; i++) {
        g_star_x[i]   = (float)(rand() % SCR_W);
        float sy      = (float)(rand() % SCR_H);
        g_star_spd[i] = 0.4f + (float)(rand()%30)/10.0f;
        lv_obj_t *s   = lv_label_create(g_bg);
        lv_label_set_text(s, star_chars[rand()%3]);
        uint8_t br = 50 + rand()%100;
        lv_obj_set_style_text_color(s, lv_color_make(br,br,br), 0);
        lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
        lv_obj_set_pos(s, (int32_t)g_star_x[i], (int32_t)sy);
        g_stars[i] = s;
    }

    /* 分數欄 */
    g_score_bar = lv_obj_create(g_scr);
    lv_obj_set_size(g_score_bar, SCR_W, SCORE_H);
    lv_obj_set_pos(g_score_bar, 0, 0);
    lv_obj_set_style_bg_color(g_score_bar, lv_color_hex(0x060616), 0);
    lv_obj_set_style_bg_opa(g_score_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_score_bar, 0, 0);
    lv_obj_set_style_border_side(g_score_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(g_score_bar, lv_color_hex(0x1a2060), 0);
    lv_obj_set_style_border_width(g_score_bar, 1, 0);
    lv_obj_remove_flag(g_score_bar, LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);

    g_score_lbl = lv_label_create(g_score_bar);
    lv_label_set_text(g_score_lbl, "SCORE: 0");
    lv_obj_set_style_text_color(g_score_lbl, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_text_font(g_score_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(g_score_lbl, LV_ALIGN_LEFT_MID, 12, 0);

    /* 血量圖示區 */
    g_hp_cont = lv_obj_create(g_score_bar);
    lv_obj_set_size(g_hp_cont, PLAYER_MAX_HP * 28, 28);
    lv_obj_align(g_hp_cont, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_bg_opa(g_hp_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_hp_cont, 0, 0);
    lv_obj_set_style_pad_all(g_hp_cont, 0, 0);
    lv_obj_set_style_pad_column(g_hp_cont, 4, 0);
    lv_obj_set_layout(g_hp_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_hp_cont, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(g_hp_cont, LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < PLAYER_MAX_HP; i++) {
        g_hp_icons[i] = mk_img(g_hp_cont, &img_heart);
        lv_obj_set_size(g_hp_icons[i], 22, 20);
        lv_image_set_scale(g_hp_icons[i], 256);
    }

    /* Boss HP */
    g_boss_hp_lbl = lv_label_create(g_score_bar);
    lv_label_set_text(g_boss_hp_lbl, "");
    lv_obj_set_style_text_color(g_boss_hp_lbl, lv_color_hex(0xff4466), 0);
    lv_obj_set_style_text_font(g_boss_hp_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(g_boss_hp_lbl, LV_ALIGN_CENTER, 0, 0);

    /* 透明點擊層 */
    g_input_catcher = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_input_catcher, SCR_W, SCR_H);
    lv_obj_set_pos(g_input_catcher, 0, 0);
    lv_obj_set_style_bg_opa(g_input_catcher, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_input_catcher, 0, 0);
    lv_obj_set_style_pad_all(g_input_catcher, 0, 0);
    lv_obj_remove_flag(g_input_catcher, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_input_catcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_input_catcher, on_click, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_input_catcher, on_click, LV_EVENT_PRESSING, NULL);

    /* 大絕數量（左下角）*/
    g_ult_lbl = lv_label_create(g_scr);
    lv_label_set_text(g_ult_lbl, "");
    lv_obj_set_style_text_color(g_ult_lbl, lv_color_hex(0xff9900), 0);
    lv_obj_set_style_text_font(g_ult_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(g_ult_lbl, 10, SCR_H - 36);

    /* 波次提示（畫面中央）*/
    g_wave_lbl = lv_label_create(g_scr);
    lv_label_set_text(g_wave_lbl, "");
    lv_obj_set_style_text_color(g_wave_lbl, lv_color_hex(0x00ffff), 0);
    lv_obj_set_style_text_font(g_wave_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(g_wave_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(g_wave_lbl, LV_ALIGN_CENTER, 0, 30);

    /* 主計時器 33ms ≈ 30fps */
    g_game_timer = lv_timer_create(game_tick, 33, NULL);

    /* ── 標題畫面 ── */
    g_state = GS_TITLE;
    g_title_panel = lv_obj_create(g_scr);
    lv_obj_set_size(g_title_panel, SCR_W, SCR_H);
    lv_obj_set_pos(g_title_panel, 0, 0);
    lv_obj_set_style_bg_color(g_title_panel, lv_color_hex(0x010118), 0);
    lv_obj_set_style_bg_opa(g_title_panel, 230, 0);
    lv_obj_set_style_border_width(g_title_panel, 0, 0);
    lv_obj_remove_flag(g_title_panel, LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);

    /* 標題大圖（player sprite） */
    lv_obj_t *title_img = mk_img(g_title_panel, &img_player);
    lv_image_set_scale(title_img, 512);
    lv_obj_align(title_img, LV_ALIGN_CENTER, -180, -100);

    lv_obj_t *t1 = lv_label_create(g_title_panel);
    lv_label_set_text(t1, "SPACE SHOOTER");
    lv_obj_set_style_text_color(t1, lv_color_hex(0x00e5ff), 0);
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_36, 0);
    lv_obj_align(t1, LV_ALIGN_CENTER, 0, -90);

    lv_obj_t *t2 = lv_label_create(g_title_panel);
    lv_label_set_text(t2,
        "Click -> Move ship  |  Double Click -> BOMB (limited!\n"
        "Collect [P] pickup -> Multi-bullet + Wingman\n"
        "Stage 1: 30s normal enemies -> BOSS1\n"
        "Stage 2: 30s normal enemies -> BOSS2  (enemies shoot missiles!)");
    lv_obj_set_style_text_color(t2, lv_color_hex(0xaabbcc), 0);
    lv_obj_set_style_text_font(t2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(t2, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t2, LV_ALIGN_CENTER, 0, 10);

    /* Boss 預覽 */
    lv_obj_t *bi1 = mk_img(g_title_panel, &img_boss1);
    lv_image_set_scale(bi1, 200);
    lv_obj_align(bi1, LV_ALIGN_CENTER, -80, 100);
    lv_obj_t *bi2 = mk_img(g_title_panel, &img_boss2);
    lv_image_set_scale(bi2, 200);
    lv_obj_align(bi2, LV_ALIGN_CENTER, 60, 100);

    lv_obj_t *t3 = lv_label_create(g_title_panel);
    lv_label_set_text(t3, "[ TAP ANYWHERE TO START ]");
    lv_obj_set_style_text_color(t3, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_text_font(t3, &lv_font_montserrat_18, 0);
    lv_obj_align(t3, LV_ALIGN_BOTTOM_MID, 0, -20);
}

/* ════════════════════════════════════════
   開始遊戲
   ════════════════════════════════════════ */
static void game_start(void) {
    safe_del(&g_title_panel);
    safe_del(&g_go_panel);
    safe_del(&g_overlay);

    g_hp              = PLAYER_MAX_HP;
    g_score           = 0;
    g_bullet_mode     = 1;
    g_last_shoot_ms   = 0;
    g_last_wm_shoot_ms= 0;
    g_last_spawn_ms   = 0;
    g_boss1_dead      = false;
    g_boss2_dead      = false;
    g_invincible      = false;
    g_wingman_count   = 0;
    g_tap_count       = 0;
    g_last_tap_ms     = 0;
    g_boss1_vy        = 1.2f;
    g_stage           = 1;
    g_stage_start_ms  = lv_tick_get();
    g_boss_wave       = false;
    g_ultimates       = ULTIMATE_MAX;

    for (int i = 0; i < MAX_BULLETS;      i++) { safe_del(&g_bullets[i].obj);       g_bullets[i].alive      = false; }
    for (int i = 0; i < MAX_ENEMIES;      i++) { safe_del(&g_enemies[i].sprite);    g_enemies[i].alive      = false; }
    for (int i = 0; i < MAX_EXPLOSIONS;   i++) { safe_del(&g_explosions[i].sprite); g_explosions[i].alive   = false; }
    for (int i = 0; i < MAX_SCORE_FLY;   i++) { safe_del(&g_score_fly[i].label);   g_score_fly[i].alive    = false; }
    for (int i = 0; i < MAX_WINGMEN;      i++) { safe_del(&g_wingmen[i].sprite);    g_wingmen[i].alive      = false; }
    for (int i = 0; i < MAX_BOSS_BULLETS;  i++) { safe_del(&g_boss_bullets[i].obj);   g_boss_bullets[i].alive  = false; }
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) { safe_del(&g_enemy_bullets[i].obj);  g_enemy_bullets[i].alive = false; }
    safe_del(&g_boss1.sprite); g_boss1.alive = false;
    safe_del(&g_boss2.sprite); g_boss2.alive = false;

    /* 玩家 sprite */
    g_player.sprite = mk_img(g_scr, &img_player);
    g_player.x  = 120;
    g_player.y  = PLAY_Y + PLAY_H / 2;
    g_player_tx = g_player.x;
    g_player_ty = g_player.y;
    g_player.alive = true;
    set_img_center(g_player.sprite, g_player.x, g_player.y);

    update_hp_icons();
    update_ult_ui();
    lv_label_set_text(g_score_lbl, "SCORE: 0");
    lv_label_set_text(g_boss_hp_lbl, "");
    show_wave_msg("STAGE 1");

    g_state = GS_PLAYING;
}

/* ════════════════════════════════════════
   Game Over
   ════════════════════════════════════════ */
static void game_over(void) {
    g_state = GS_GAMEOVER;
    safe_del(&g_player.sprite);
    g_player.alive = false;

    g_go_panel = lv_obj_create(g_scr);
    lv_obj_set_size(g_go_panel, SCR_W, SCR_H);
    lv_obj_set_pos(g_go_panel, 0, 0);
    lv_obj_set_style_bg_color(g_go_panel, lv_color_hex(0x100005), 0);
    lv_obj_set_style_bg_opa(g_go_panel, 225, 0);
    lv_obj_set_style_border_width(g_go_panel, 0, 0);
    lv_obj_remove_flag(g_go_panel, LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *g1 = lv_label_create(g_go_panel);
    lv_label_set_text(g1, "GAME OVER");
    lv_obj_set_style_text_color(g1, lv_color_hex(0xff3333), 0);
    lv_obj_set_style_text_font(g1, &lv_font_montserrat_48, 0);
    lv_obj_align(g1, LV_ALIGN_CENTER, 0, -80);

    char buf[48];
    snprintf(buf, sizeof(buf), "FINAL SCORE: %d", g_score);
    lv_obj_t *g2 = lv_label_create(g_go_panel);
    lv_label_set_text(g2, buf);
    lv_obj_set_style_text_color(g2, lv_color_hex(0xffd700), 0);
    lv_obj_set_style_text_font(g2, &lv_font_montserrat_24, 0);
    lv_obj_align(g2, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t *g3 = lv_label_create(g_go_panel);
    lv_label_set_text(g3, "[ TAP TO RETRY ]");
    lv_obj_set_style_text_color(g3, lv_color_hex(0x88aacc), 0);
    lv_obj_set_style_text_font(g3, &lv_font_montserrat_18, 0);
    lv_obj_align(g3, LV_ALIGN_CENTER, 0, 80);
}

/* ════════════════════════════════════════
   點擊事件
   ════════════════════════════════════════ */
static void on_click(lv_event_t *e) {
    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev) indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (g_state == GS_TITLE || g_state == GS_GAMEOVER) {
        if (lv_event_get_code(e) == LV_EVENT_PRESSED) game_start();
        return;
    }
    if (g_state != GS_PLAYING) return;

    /* PRESSING：按住拖曳時持續更新位置，不觸發雙擊 */
    if (lv_event_get_code(e) == LV_EVENT_PRESSING) {
        g_player_tx = fclampf((float)p.x + 100.0f, PLAYER_CW/2, SCR_W - PLAYER_CW/2);
        g_player_ty = fclampf((float)p.y, PLAY_Y + PLAYER_CH/2, SCR_H - PLAYER_CH/2);
        return;
    }

    /* PRESSED：計算雙擊 + 更新位置 */
    uint32_t now = lv_tick_get();
    if (now - g_last_tap_ms < 400) {
        g_tap_count++;
        if (g_tap_count >= 2) { g_tap_count = 0; do_ultimate(); return; }
    } else {
        g_tap_count = 1;
    }
    g_last_tap_ms = now;

    /* 飛機在滑鼠右側 +100，手指不會遮住飛機 */
    g_player_tx = fclampf((float)p.x + 100.0f, PLAYER_CW/2, SCR_W - PLAYER_CW/2);
    g_player_ty = fclampf((float)p.y, PLAY_Y + PLAYER_CH/2, SCR_H - PLAYER_CH/2);
}

/* ════════════════════════════════════════
   HP 圖示更新
   ════════════════════════════════════════ */
static void update_hp_icons(void) {
    for (int i = 0; i < PLAYER_MAX_HP; i++) {
        lv_opa_t op = (i < g_hp) ? LV_OPA_COVER : LV_OPA_20;
        lv_obj_set_style_img_opa(g_hp_icons[i], op, 0);
    }
}

/* ════════════════════════════════════════
   Boss HP 文字
   ════════════════════════════════════════ */
static void update_boss_hp(void) {
    char buf[48];
    if      (g_boss2.alive) snprintf(buf, sizeof(buf), "BOSS2 HP: %d / %d", g_boss2.hp, BOSS2_MAX_HP);
    else if (g_boss1.alive) snprintf(buf, sizeof(buf), "BOSS1 HP: %d / %d", g_boss1.hp, BOSS1_MAX_HP);
    else                    buf[0] = '\0';
    lv_label_set_text(g_boss_hp_lbl, buf);
}

/* ════════════════════════════════════════
   大絕 UI 更新
   ════════════════════════════════════════ */
static void update_ult_ui(void) {
    if (!g_ult_lbl) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "BOMB x%d", g_ultimates);
    lv_label_set_text(g_ult_lbl, buf);
    lv_color_t col = (g_ultimates > 0) ? lv_color_hex(0xff9900) : lv_color_hex(0x555555);
    lv_obj_set_style_text_color(g_ult_lbl, col, 0);
}

/* ════════════════════════════════════════
   波次提示（短暇顯示後淡出）
   ════════════════════════════════════════ */
static void show_wave_msg(const char *msg) {
    if (!g_wave_lbl) return;
    lv_label_set_text(g_wave_lbl, msg);
    lv_obj_set_style_text_opa(g_wave_lbl, LV_OPA_COVER, 0);
    lv_obj_align(g_wave_lbl, LV_ALIGN_CENTER, 0, 30);
    g_wave_lbl_ms = lv_tick_get();
}

/* ════════════════════════════════════════
   敵機發射飛彈
   ════════════════════════════════════════ */
static void shoot_enemy(int idx) {
    if (!g_player.alive) return;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (g_enemy_bullets[i].alive) continue;
        float dx = g_player.x - g_enemies[idx].x;
        float dy = g_player.y - g_enemies[idx].y;
        float len = sqrtf(dx*dx+dy*dy); if (len < 0.1f) len = 0.1f;
        g_enemy_bullets[i].alive = true;
        g_enemy_bullets[i].x    = g_enemies[idx].x - ENEMY_CW/2.0f;
        g_enemy_bullets[i].y    = g_enemies[idx].y;
        g_enemy_bullets[i].vx   = dx/len * ENEMY_BULLET_SPD;
        g_enemy_bullets[i].vy   = dy/len * ENEMY_BULLET_SPD;
        g_enemy_bullets[i].obj  = mk_img(g_scr, &img_bomb);
        set_img_center(g_enemy_bullets[i].obj, g_enemy_bullets[i].x, g_enemy_bullets[i].y);
        g_enemies[idx].next_shoot_ms = lv_tick_get() + ENEMY_SHOOT_MS_MIN
                                       + rand() % (ENEMY_SHOOT_MS_MAX - ENEMY_SHOOT_MS_MIN);
        break;
    }
}

/* ════════════════════════════════════════
   大絕招
   ════════════════════════════════════════ */
static void do_ultimate(void) {
    if (g_ultimates <= 0) return;   /* 炸彈用完，無法發動 */
    g_ultimates--;
    update_ult_ui();
    safe_del(&g_overlay);
    g_overlay = lv_obj_create(g_scr);
    lv_obj_set_size(g_overlay, SCR_W, SCR_H);
    lv_obj_set_pos(g_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_overlay, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(g_overlay, 220, 0);
    lv_obj_set_style_border_width(g_overlay, 0, 0);
    lv_obj_remove_flag(g_overlay, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ul = lv_label_create(g_overlay);
    lv_label_set_text(ul, "*** ULTIMATE ***");
    lv_obj_set_style_text_color(ul, lv_color_hex(0xff4400), 0);
    lv_obj_set_style_text_font(ul, &lv_font_montserrat_28, 0);
    lv_obj_align(ul, LV_ALIGN_CENTER, 0, 0);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].alive) continue;
        spawn_explosion(g_enemies[i].x, g_enemies[i].y);
        add_score(50, g_enemies[i].x, g_enemies[i].y);
        safe_del(&g_enemies[i].sprite); g_enemies[i].alive = false;
    }
    if (g_boss1.alive) {
        g_boss1.hp -= 8;
        if (g_boss1.hp <= 0) {
            spawn_explosion(g_boss1.x, g_boss1.y);
            add_score(300, g_boss1.x, g_boss1.y);
            safe_del(&g_boss1.sprite); g_boss1.alive = false; g_boss1_dead = true;
            g_stage = 2; g_stage_start_ms = lv_tick_get(); g_boss_wave = false;
            show_wave_msg("STAGE 2  CLEAR!");
        }
    }
    if (g_boss2.alive) {
        g_boss2.hp -= 8;
        if (g_boss2.hp <= 0) {
            spawn_explosion(g_boss2.x, g_boss2.y);
            add_score(600, g_boss2.x, g_boss2.y);
            safe_del(&g_boss2.sprite); g_boss2.alive = false; g_boss2_dead = true;
            g_stage = 3; g_stage_start_ms = lv_tick_get(); g_boss_wave = false;
            show_wave_msg("ALL BOSSES  DEFEATED!");
        }
    }
    for (int i = 0; i < MAX_BOSS_BULLETS;  i++) { safe_del(&g_boss_bullets[i].obj);  g_boss_bullets[i].alive  = false; }
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) { safe_del(&g_enemy_bullets[i].obj); g_enemy_bullets[i].alive = false; }
    update_boss_hp();
}

/* ════════════════════════════════════════
   生成敵機
   ════════════════════════════════════════ */
static const lv_image_dsc_t *enemy_srcs[] = {
    &img_enemy_ufo, &img_enemy_rocket, &img_enemy_alien, &img_pickup
};

static void spawn_enemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g_enemies[i].alive) continue;
        /* type: 0,1,2=普通  3=補給（機率較低）*/
        int type;
        if (rand()%8 == 0) type = 3;   /* ~12% 機率補給 */
        else               type = rand()%3;

        g_enemies[i].alive  = true;
        g_enemies[i].type   = type;
        g_enemies[i].x      = SCR_W + 30;
        g_enemies[i].y      = PLAY_Y + 30 + (float)(rand()%(PLAY_H-60));
        g_enemies[i].hp            = (type==3) ? 1 : 1 + rand()%2;
        g_enemies[i].next_shoot_ms = lv_tick_get() + ENEMY_SHOOT_MS_MIN
                                     + rand() % (ENEMY_SHOOT_MS_MAX - ENEMY_SHOOT_MS_MIN);
        g_enemies[i].sprite = mk_img(g_scr, enemy_srcs[type]);
        set_img_center(g_enemies[i].sprite, g_enemies[i].x, g_enemies[i].y);
        break;
    }
}

static void spawn_boss1(void) {
    if (g_boss1.alive || g_boss1_dead) return;
    g_boss1.alive  = true;
    g_boss1.hp     = BOSS1_MAX_HP;
    g_boss1.x      = SCR_W - 60;
    g_boss1.y      = PLAY_Y + PLAY_H/2;
    g_boss1.sprite = mk_img(g_scr, &img_boss1);
    set_img_center(g_boss1.sprite, g_boss1.x, g_boss1.y);
    update_boss_hp();
}

static void spawn_boss2(void) {
    if (g_boss2.alive || g_boss2_dead) return;
    g_boss2.alive  = true;
    g_boss2.hp     = BOSS2_MAX_HP;
    g_boss2.x      = SCR_W - 60;
    g_boss2.y      = PLAY_Y + 80;
    g_boss2.sprite = mk_img(g_scr, &img_boss2);
    set_img_center(g_boss2.sprite, g_boss2.x, g_boss2.y);
    update_boss_hp();
}

/* ════════════════════════════════════════
   發射子彈
   ════════════════════════════════════════ */
static Bullet *alloc_bullet(void) {
    for (int i = 0; i < MAX_BULLETS; i++) if (!g_bullets[i].alive) return &g_bullets[i];
    return NULL;
}

static void fire_one(float x, float y, float vx, float vy, lv_color_t col) {
    Bullet *b = alloc_bullet();
    if (!b) return;
    b->alive = true; b->x = x; b->y = y; b->vx = vx; b->vy = vy;
    b->obj = mk_bullet_obj(g_scr, 14, 6, col);
    lv_obj_set_pos(b->obj, (int32_t)(x-7), (int32_t)(y-3));
}

static void shoot_player(void) {
    float bx = g_player.x + PLAYER_CW/2 + 2;
    float by = g_player.y;
    lv_color_t bc = lv_color_hex(0x00ffff);
    lv_color_t by2= lv_color_hex(0xffff44);
    if      (g_bullet_mode==1) { fire_one(bx,by, BULLET_SPD,    0,  bc); }
    else if (g_bullet_mode==2) { fire_one(bx,by, BULLET_SPD,    0,  bc);
                                  fire_one(bx,by, BULLET_SPD*0.95f,-3.5f, bc);
                                  fire_one(bx,by, BULLET_SPD*0.95f, 3.5f, bc); }
    else                       { fire_one(bx,by, BULLET_SPD,    0,  bc);
                                  fire_one(bx,by, BULLET_SPD*0.95f,-3.0f, bc);
                                  fire_one(bx,by, BULLET_SPD*0.95f, 3.0f, bc);
                                  fire_one(bx,by, BULLET_SPD*0.85f,-6.0f, by2);
                                  fire_one(bx,by, BULLET_SPD*0.85f, 6.0f, by2); }
}

static void shoot_wingman(int wm) {
    if (!g_wingmen[wm].alive) return;
    Bullet *b = alloc_bullet();
    if (!b) return;
    b->alive = true;
    b->x = g_wingmen[wm].x + 14;
    b->y = g_wingmen[wm].y;
    b->vx = BULLET_SPD; b->vy = 0;
    b->obj = mk_bullet_obj(g_scr, 12, 5, lv_color_hex(0xffaa00));
    lv_obj_set_pos(b->obj, (int32_t)(b->x-6), (int32_t)(b->y-2));
}

static void shoot_boss(void) {
    if (!g_player.alive) return;
    Entity *boss = g_boss1.alive ? &g_boss1 : (g_boss2.alive ? &g_boss2 : NULL);
    if (!boss) return;
    for (int i = 0; i < MAX_BOSS_BULLETS; i++) {
        if (g_boss_bullets[i].alive) continue;
        float dx = g_player.x - boss->x, dy = g_player.y - boss->y;
        float len = sqrtf(dx*dx+dy*dy); if (len<0.1f) len=0.1f;
        g_boss_bullets[i].alive = true;
        g_boss_bullets[i].x  = boss->x - 35;
        g_boss_bullets[i].y  = boss->y;
        g_boss_bullets[i].vx = dx/len * BOSS_BULLET_SPD;
        g_boss_bullets[i].vy = dy/len * BOSS_BULLET_SPD;
        /* 用 img_bomb 當 boss 子彈 sprite */
        g_boss_bullets[i].obj = mk_img(g_scr, &img_bomb);
        set_img_center(g_boss_bullets[i].obj, g_boss_bullets[i].x, g_boss_bullets[i].y);
        break;
    }
}

/* ════════════════════════════════════════
   爆炸（縮放+淡出動畫）
   ════════════════════════════════════════ */
static void spawn_explosion(float cx, float cy) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (g_explosions[i].alive) continue;
        g_explosions[i].alive    = true;
        g_explosions[i].x        = cx;
        g_explosions[i].y        = cy;
        g_explosions[i].start_ms = lv_tick_get();
        g_explosions[i].sprite   = mk_img(g_scr, &img_explosion);
        lv_image_set_scale(g_explosions[i].sprite, 256);
        set_img_center(g_explosions[i].sprite, cx, cy);
        break;
    }
}

/* ════════════════════════════════════════
   飛分數
   ════════════════════════════════════════ */
static void add_score(int val, float fx, float fy) {
    for (int i = 0; i < MAX_SCORE_FLY; i++) {
        if (g_score_fly[i].alive) continue;
        g_score_fly[i].alive = true;
        g_score_fly[i].value = val;
        g_score_fly[i].x = fx; g_score_fly[i].y = fy;
        g_score_fly[i].tx = 100.0f; g_score_fly[i].ty = SCORE_H/2.0f;
        char buf[16]; snprintf(buf,sizeof(buf),"+%d",val);
        g_score_fly[i].label = lv_label_create(g_scr);
        lv_label_set_text(g_score_fly[i].label, buf);
        lv_obj_set_style_text_color(g_score_fly[i].label, lv_color_hex(0xffd700), 0);
        lv_obj_set_style_text_font(g_score_fly[i].label, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(g_score_fly[i].label, (int32_t)fx, (int32_t)fy);
        break;
    }
}

/* ════════════════════════════════════════
   星星捲動
   ════════════════════════════════════════ */
static void update_stars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        g_star_x[i] -= g_star_spd[i];
        if (g_star_x[i] < -6) g_star_x[i] = SCR_W + 4;
        lv_obj_set_x(g_stars[i], (int32_t)g_star_x[i]);
    }
}

/* ════════════════════════════════════════
   碰撞
   ════════════════════════════════════════ */
static inline bool rect_hit(float ax,float ay,float aw,float ah,
                              float bx,float by,float bw,float bh){
    return ax<bx+bw && ax+aw>bx && ay<by+bh && ay+ah>by;
}

static void check_collisions(void) {
    /* 子彈 vs 敵機 */
    for (int bi=0; bi<MAX_BULLETS; bi++) {
        if (!g_bullets[bi].alive) continue;
        float bx=g_bullets[bi].x-7, by=g_bullets[bi].y-3;

        for (int ei=0; ei<MAX_ENEMIES; ei++) {
            if (!g_enemies[ei].alive) continue;
            if (!rect_hit(bx,by,14,6, g_enemies[ei].x-ENEMY_CW/2, g_enemies[ei].y-ENEMY_CH/2, ENEMY_CW,ENEMY_CH)) continue;
            safe_del(&g_bullets[bi].obj); g_bullets[bi].alive=false;
            g_enemies[ei].hp--;
            if (g_enemies[ei].hp<=0) {
                if (g_enemies[ei].type==3) {
                    /* 補給 */
                    g_bullet_mode = (g_bullet_mode<3) ? g_bullet_mode+1 : 3;
                    if (g_wingman_count<MAX_WINGMEN) {
                        int w=g_wingman_count;
                        g_wingmen[w].alive=true;
                        g_wingmen[w].x=g_player.x-10;
                        g_wingmen[w].y=g_player.y+(w==0?-38:38);
                        g_wingmen[w].sprite=mk_img(g_scr,&img_wingman);
                        set_img_center(g_wingmen[w].sprite,g_wingmen[w].x,g_wingmen[w].y);
                        g_wingman_count++;
                    }
                    add_score(30, g_enemies[ei].x, g_enemies[ei].y);
                } else {
                    spawn_explosion(g_enemies[ei].x, g_enemies[ei].y);
                    add_score(100, g_enemies[ei].x, g_enemies[ei].y);
                }
                safe_del(&g_enemies[ei].sprite); g_enemies[ei].alive=false;
            }
            break;
        }
        if (!g_bullets[bi].alive) continue;

        /* vs Boss1 */
        if (g_boss1.alive && rect_hit(bx,by,14,6, g_boss1.x-BOSS1_CW/2,g_boss1.y-BOSS1_CH/2,BOSS1_CW,BOSS1_CH)) {
            safe_del(&g_bullets[bi].obj); g_bullets[bi].alive=false;
            g_boss1.hp--;
            if (g_boss1.hp<=0) {
                spawn_explosion(g_boss1.x,g_boss1.y); add_score(300,g_boss1.x,g_boss1.y);
                safe_del(&g_boss1.sprite); g_boss1.alive=false; g_boss1_dead=true;
                g_stage=2; g_stage_start_ms=lv_tick_get(); g_boss_wave=false;
                show_wave_msg("STAGE 2  CLEAR!");
            }
            update_boss_hp(); continue;
        }
        /* vs Boss2 */
        if (g_boss2.alive && rect_hit(bx,by,14,6, g_boss2.x-BOSS2_CW/2,g_boss2.y-BOSS2_CH/2,BOSS2_CW,BOSS2_CH)) {
            safe_del(&g_bullets[bi].obj); g_bullets[bi].alive=false;
            g_boss2.hp--;
            if (g_boss2.hp<=0) {
                spawn_explosion(g_boss2.x,g_boss2.y); add_score(600,g_boss2.x,g_boss2.y);
                safe_del(&g_boss2.sprite); g_boss2.alive=false; g_boss2_dead=true;
                g_stage=3; g_stage_start_ms=lv_tick_get(); g_boss_wave=false;
                show_wave_msg("ALL BOSSES  DEFEATED!");
            }
            update_boss_hp(); continue;
        }
    }

    if (!g_player.alive || g_invincible) return;
    float px=g_player.x-PLAYER_CW/2, py=g_player.y-PLAYER_CH/2;

    /* Boss 子彈 vs 玩家 */
    for (int i=0; i<MAX_BOSS_BULLETS; i++) {
        if (!g_boss_bullets[i].alive) continue;
        if (!rect_hit(g_boss_bullets[i].x-8,g_boss_bullets[i].y-8,16,16, px,py,PLAYER_CW,PLAYER_CH)) continue;
        safe_del(&g_boss_bullets[i].obj); g_boss_bullets[i].alive=false;
        g_hp--; update_hp_icons();
        g_invincible=true; g_invincible_ms=lv_tick_get();
        if (g_hp<=0) { spawn_explosion(g_player.x,g_player.y); game_over(); return; }
    }

    /* 敵機飛彈 vs 玩家 */
    for (int i=0; i<MAX_ENEMY_BULLETS; i++) {
        if (!g_enemy_bullets[i].alive) continue;
        if (!rect_hit(g_enemy_bullets[i].x-8,g_enemy_bullets[i].y-8,16,16, px,py,PLAYER_CW,PLAYER_CH)) continue;
        safe_del(&g_enemy_bullets[i].obj); g_enemy_bullets[i].alive=false;
        g_hp--; update_hp_icons();
        g_invincible=true; g_invincible_ms=lv_tick_get();
        if (g_hp<=0) { spawn_explosion(g_player.x,g_player.y); game_over(); return; }
    }

    /* 敵機 vs 玩家 */
    for (int ei=0; ei<MAX_ENEMIES; ei++) {
        if (!g_enemies[ei].alive) continue;
        if (!rect_hit(px,py,PLAYER_CW,PLAYER_CH, g_enemies[ei].x-ENEMY_CW/2,g_enemies[ei].y-ENEMY_CH/2,ENEMY_CW,ENEMY_CH)) continue;
        spawn_explosion(g_enemies[ei].x,g_enemies[ei].y);
        safe_del(&g_enemies[ei].sprite); g_enemies[ei].alive=false;
        g_hp--; update_hp_icons();
        g_invincible=true; g_invincible_ms=lv_tick_get();
        if (g_hp<=0) { spawn_explosion(g_player.x,g_player.y); game_over(); return; }
    }
}

/* ════════════════════════════════════════
   主遊戲 Tick (33ms)
   ════════════════════════════════════════ */
static void game_tick(lv_timer_t *t) {
    (void)t;
    uint32_t now = lv_tick_get();

    update_stars();

    if (g_state != GS_PLAYING) {
        if (g_overlay) { lv_obj_del(g_overlay); g_overlay=NULL; }
        return;
    }

    /* overlay 淡出 */
    if (g_overlay) {
        lv_opa_t op = lv_obj_get_style_bg_opa(g_overlay,0);
        if (op>25) lv_obj_set_style_bg_opa(g_overlay,(lv_opa_t)(op-25),0);
        else { lv_obj_del(g_overlay); g_overlay=NULL; }
    }

    /* 玩家無敵閃爍 */
    if (g_invincible && now-g_invincible_ms>1500) g_invincible=false;
    if (g_player.sprite) {
        lv_opa_t op = (g_invincible && (now/120)%2) ? LV_OPA_40 : LV_OPA_COVER;
        lv_obj_set_style_img_opa(g_player.sprite, op, 0);
    }

    /* 玩家平滑移動 */
    g_player.x = lerpf(g_player.x, g_player_tx, 0.18f);
    g_player.y = lerpf(g_player.y, g_player_ty, 0.18f);
    if (g_player.sprite) set_img_center(g_player.sprite, g_player.x, g_player.y);

    /* 子機跟隨 */
    for (int i=0; i<MAX_WINGMEN; i++) {
        if (!g_wingmen[i].alive) continue;
        float tx = g_player.x-14, ty = g_player.y+(i==0?-38:38);
        g_wingmen[i].x = lerpf(g_wingmen[i].x, tx, 0.14f);
        g_wingmen[i].y = lerpf(g_wingmen[i].y, ty, 0.14f);
        set_img_center(g_wingmen[i].sprite, g_wingmen[i].x, g_wingmen[i].y);
    }

    /* 自動發射 */
    if (now-g_last_shoot_ms >= SHOOT_INTERVAL_MS) { shoot_player(); g_last_shoot_ms=now; }
    if (now-g_last_wm_shoot_ms >= WINGMAN_SHOOT_MS) { for(int i=0;i<MAX_WINGMEN;i++) shoot_wingman(i); g_last_wm_shoot_ms=now; }

    /* 更新子彈 */
    for (int i=0; i<MAX_BULLETS; i++) {
        if (!g_bullets[i].alive) continue;
        g_bullets[i].x += g_bullets[i].vx;
        g_bullets[i].y += g_bullets[i].vy;
        if (g_bullets[i].x>SCR_W+20 || g_bullets[i].y<PLAY_Y-10 || g_bullets[i].y>SCR_H+10) {
            safe_del(&g_bullets[i].obj); g_bullets[i].alive=false; continue;
        }
        lv_obj_set_pos(g_bullets[i].obj, (int32_t)(g_bullets[i].x-7), (int32_t)(g_bullets[i].y-3));
    }

    /* 生成敵機（Boss 波次期間停止生成）*/
    if (!g_boss_wave && now - g_last_spawn_ms >= SPAWN_INTERVAL_MS) {
        spawn_enemy();
        g_last_spawn_ms = now;
    }

    /* 波次推進：30 秒後召喚 Boss */
    if (!g_boss_wave && (now - g_stage_start_ms) >= (uint32_t)STAGE_WAVE_SEC * 1000) {
        g_boss_wave = true;
        if      (g_stage == 1 && !g_boss1_dead) { spawn_boss1(); show_wave_msg("!! BOSS INCOMING !!"); }
        else if (g_stage == 2 && !g_boss2_dead) { spawn_boss2(); show_wave_msg("!! FINAL BOSS !!"); }
        else                                    { g_boss_wave = false; } /* 無更多 Boss */
    }

    /* 更新敵機 */
    float espd = ENEMY_SPD_BASE + (float)g_score/600.0f;
    if (espd>6.0f) espd=6.0f;
    for (int i=0; i<MAX_ENEMIES; i++) {
        if (!g_enemies[i].alive) continue;
        g_enemies[i].x -= espd;
        if (g_enemies[i].x < -50) { safe_del(&g_enemies[i].sprite); g_enemies[i].alive=false; continue; }
        set_img_center(g_enemies[i].sprite, g_enemies[i].x, g_enemies[i].y);
        /* 敵機射擊飛彈 */
        if (g_enemies[i].type != 3 && now >= g_enemies[i].next_shoot_ms) {
            shoot_enemy(i);
        }
    }

    /* Boss1 上下巡邏 */
    if (g_boss1.alive) {
        g_boss1.y += g_boss1_vy;
        if (g_boss1.y>SCR_H-60 || g_boss1.y<PLAY_Y+60) g_boss1_vy=-g_boss1_vy;
        if (g_boss1.x>SCR_W*0.65f) g_boss1.x-=0.5f;
        set_img_center(g_boss1.sprite, g_boss1.x, g_boss1.y);
    }

    /* Boss2 追蹤玩家 Y */
    if (g_boss2.alive) {
        float dy = g_player.y-g_boss2.y;
        g_boss2.y += dy*0.022f;
        if (g_boss2.x>SCR_W*0.60f) g_boss2.x-=0.6f;
        set_img_center(g_boss2.sprite, g_boss2.x, g_boss2.y);
    }

    /* Boss 發射 */
    if ((g_boss1.alive||g_boss2.alive) && now-g_boss_shoot_ms>=850) {
        shoot_boss(); g_boss_shoot_ms=now;
    }

    /* Boss 子彈更新 */
    for (int i=0; i<MAX_BOSS_BULLETS; i++) {
        if (!g_boss_bullets[i].alive) continue;
        g_boss_bullets[i].x += g_boss_bullets[i].vx;
        g_boss_bullets[i].y += g_boss_bullets[i].vy;
        if (g_boss_bullets[i].x<-20||g_boss_bullets[i].x>SCR_W+20||
            g_boss_bullets[i].y<0  ||g_boss_bullets[i].y>SCR_H) {
            safe_del(&g_boss_bullets[i].obj); g_boss_bullets[i].alive=false; continue;
        }
        set_img_center(g_boss_bullets[i].obj, g_boss_bullets[i].x, g_boss_bullets[i].y);
    }

    /* 敵機飛彈更新 */
    for (int i=0; i<MAX_ENEMY_BULLETS; i++) {
        if (!g_enemy_bullets[i].alive) continue;
        g_enemy_bullets[i].x += g_enemy_bullets[i].vx;
        g_enemy_bullets[i].y += g_enemy_bullets[i].vy;
        if (g_enemy_bullets[i].x<-20||g_enemy_bullets[i].x>SCR_W+20||
            g_enemy_bullets[i].y<PLAY_Y-20||g_enemy_bullets[i].y>SCR_H+20) {
            safe_del(&g_enemy_bullets[i].obj); g_enemy_bullets[i].alive=false; continue;
        }
        set_img_center(g_enemy_bullets[i].obj, g_enemy_bullets[i].x, g_enemy_bullets[i].y);
    }

    /* 碰撞 */
    check_collisions();

    /* 爆炸動畫（縮放放大 + 淡出）*/
    for (int i=0; i<MAX_EXPLOSIONS; i++) {
        if (!g_explosions[i].alive) continue;
        uint32_t el = now - g_explosions[i].start_ms;
        if (el > 500) {
            safe_del(&g_explosions[i].sprite); g_explosions[i].alive=false; continue;
        }
        /* scale 256→512, opa 255→0 */
        int32_t scale = (int32_t)(256 + el * 512 / 500);
        lv_opa_t  opa = (lv_opa_t)(255 - el * 255 / 500);
        lv_image_set_scale(g_explosions[i].sprite, scale);
        lv_obj_set_style_img_opa(g_explosions[i].sprite, opa, 0);
        set_img_center(g_explosions[i].sprite, g_explosions[i].x, g_explosions[i].y);
    }

    /* 飛分數 */
    for (int i=0; i<MAX_SCORE_FLY; i++) {
        if (!g_score_fly[i].alive) continue;
        g_score_fly[i].x = lerpf(g_score_fly[i].x, g_score_fly[i].tx, 0.10f);
        g_score_fly[i].y = lerpf(g_score_fly[i].y, g_score_fly[i].ty, 0.10f);
        lv_obj_set_pos(g_score_fly[i].label,
            (int32_t)g_score_fly[i].x, (int32_t)g_score_fly[i].y);

        float dx=g_score_fly[i].tx-g_score_fly[i].x;
        float dy=g_score_fly[i].ty-g_score_fly[i].y;
        if (sqrtf(dx*dx+dy*dy)<8.0f) {
            g_score += g_score_fly[i].value;
            safe_del(&g_score_fly[i].label); g_score_fly[i].alive=false;
            char buf[32]; snprintf(buf,sizeof(buf),"SCORE: %d",g_score);
            lv_label_set_text(g_score_lbl, buf);
            g_score_flash=true; g_score_flash_ms=now;
        }
    }

    /* 分數欄彈跳（白→金） */
    if (g_score_flash) {
        uint32_t el = now-g_score_flash_ms;
        if      (el<100) lv_obj_set_style_text_color(g_score_lbl, lv_color_hex(0xffffff),0);
        else if (el<200) lv_obj_set_style_text_color(g_score_lbl, lv_color_hex(0xffee55),0);
        else { g_score_flash=false; lv_obj_set_style_text_color(g_score_lbl, lv_color_hex(0xffd700),0); }
    }
}
