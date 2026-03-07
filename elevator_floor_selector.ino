/**
 * ============================================================
 *  Elevator Floor Selector
 *  Device: VIEWE UEDX46460015-MD50ET (1.5" 466x466 Round LCD)
 *  MCU: ESP32-S3R8 / 16MB Flash / 8MB OPI PSRAM
 *  Features:
 *    - 1~50층 선택
 *    - 로터리 엔코더로 층 변경 (IO5/IO6)
 *    - 버튼 클릭으로 확인/전송 (IO0)
 *    - 터치로 +/- 버튼 조작 (CST820)
 *    - 원형 디스플레이 최적화 UI
 * ============================================================
 */

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"
#include <ESP_Knob.h>
#include <Button.h>

// ─── 커스텀 폰트 외부 선언 ──────────────────────────────────
LV_FONT_DECLARE(lv_font_montserrat_96);
LV_FONT_DECLARE(lv_font_montserrat_140);
LV_FONT_DECLARE(lv_font_montserrat_192);

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// ─── 핀 정의 ────────────────────────────────────────────────
#define GPIO_NUM_KNOB_PIN_A   6
#define GPIO_NUM_KNOB_PIN_B   5
#define GPIO_BUTTON_PIN       GPIO_NUM_0

// ─── 층 설정 ────────────────────────────────────────────────
#define FLOOR_MIN   1
#define FLOOR_MAX   50

// ─── 색상 팔레트 ─────────────────────────────────────────────
#define COLOR_BG          lv_color_hex(0x0A0A1A)   // 딥 네이비 배경
#define COLOR_RING_TRACK  lv_color_hex(0x1E2A4A)   // 링 트랙
#define COLOR_RING_FILL   lv_color_hex(0x2196F3)   // 파랑 진행 링
#define COLOR_ACCENT      lv_color_hex(0x42A5F5)   // 밝은 파랑 강조
#define COLOR_FLOOR_NUM   lv_color_hex(0xFFFFFF)   // 흰색 층번호
#define COLOR_FLOOR_DIM   lv_color_hex(0x607D8B)   // 회색 단위 텍스트
#define COLOR_BTN_PLUS    lv_color_hex(0x1565C0)   // + 버튼 배경
#define COLOR_BTN_MINUS   lv_color_hex(0x1565C0)   // - 버튼 배경
#define COLOR_BTN_PRESS   lv_color_hex(0x0D47A1)   // 버튼 눌림
#define COLOR_CONFIRM     lv_color_hex(0x1E88E5)   // 확인 버튼
#define COLOR_CONFIRM_PR  lv_color_hex(0x1565C0)   // 확인 눌림
#define COLOR_SUCCESS     lv_color_hex(0x00BCD4)   // 성공 색상
#define COLOR_GLOW        lv_color_hex(0x1A237E)   // 발광 효과

// ─── UI 객체 ─────────────────────────────────────────────────
static lv_obj_t *scr_main   = nullptr;
static lv_obj_t *scr_confirm= nullptr;

// 메인 화면
static lv_obj_t *arc_ring   = nullptr;   // 진행률 링
static lv_obj_t *lbl_floor  = nullptr;   // 층 번호 대형 텍스트
static lv_obj_t *lbl_unit   = nullptr;   // "층" 단위
static lv_obj_t *lbl_hint   = nullptr;   // 조작 힌트
static lv_obj_t *btn_plus   = nullptr;   // + 버튼
static lv_obj_t *btn_minus  = nullptr;   // - 버튼
static lv_obj_t *btn_confirm= nullptr;   // 확인 버튼
static lv_obj_t *lbl_minmax = nullptr;   // 1~50 표시

// 확인 화면
static lv_obj_t *lbl_confirm_floor = nullptr;
static lv_obj_t *btn_back   = nullptr;

// ─── 상태 변수 ───────────────────────────────────────────────
static int  g_current_floor  = 1;
static bool g_confirmed      = false;
static lv_timer_t *g_auto_return_timer = nullptr;  // 2초 후 자동 복귀 타이머

// ─── 폰트 선언 ───────────────────────────────────────────────
// 빌트인 폰트 사용 (별도 폰트 설치 불필요)
#define FONT_FLOOR_NUM  &lv_font_montserrat_140  // 커스텀 140px 폰트
#define FONT_UNIT       &lv_font_montserrat_32
#define FONT_HINT       &lv_font_montserrat_26    //기존 28
#define FONT_BTN        &lv_font_montserrat_28    //기존 28

// ─── 전방 선언 ───────────────────────────────────────────────
static void create_main_screen(void);
static void create_confirm_screen(void);
static void update_floor_display(void);
static void goto_confirm_screen(void);
static void goto_main_screen(void);

// ─── 층 변경 함수 ────────────────────────────────────────────
static void set_floor(int floor) {
    if (floor < FLOOR_MIN) floor = FLOOR_MIN;
    if (floor > FLOOR_MAX) floor = FLOOR_MAX;
    g_current_floor = floor;
    update_floor_display();
}

static void floor_up(void) {
    if (g_current_floor < FLOOR_MAX) set_floor(g_current_floor + 1);
}

static void floor_down(void) {
    if (g_current_floor > FLOOR_MIN) set_floor(g_current_floor - 1);
}

// ─── 진행 링 업데이트 ─────────────────────────────────────────
static void update_floor_display(void) {
    if (!arc_ring || !lbl_floor) return;

    // 층 번호 텍스트 업데이트
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", g_current_floor);
    lv_label_set_text(lbl_floor, buf);

    // 아크 각도 계산 (1층=0°, 50층=360°)
    int16_t angle = (int16_t)(((g_current_floor - FLOOR_MIN) * 360) / (FLOOR_MAX - FLOOR_MIN));
    lv_arc_set_value(arc_ring, g_current_floor);

    // 층에 따른 색상 변화 (저층: 파랑, 중층: 청록, 고층: 하늘)
    lv_color_t ring_color;
    if (g_current_floor <= 17) {
        ring_color = lv_color_hex(0x1565C0);      // 저층 - 진파랑
    } else if (g_current_floor <= 34) {
        ring_color = lv_color_hex(0x32CD32);      // 중층 - 하늘파랑
    } else {
        ring_color = lv_color_hex(0xFF0000);      // 고층 - 시안,0x00BCD4
    }
    lv_obj_set_style_arc_color(arc_ring, ring_color, LV_PART_INDICATOR);

    // Serial 출력
    Serial.printf("[Floor] %dFloor Seleted\n", g_current_floor);
}

// ─── UI 스타일 헬퍼 ──────────────────────────────────────────
static void apply_btn_style(lv_obj_t *btn, lv_color_t color) {
    lv_obj_set_style_bg_color(btn, color, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, COLOR_BTN_PRESS, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, COLOR_ACCENT, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(btn, LV_OPA_60, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(btn, COLOR_ACCENT, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, LV_STATE_DEFAULT);
}

// ─── 이벤트 콜백 ─────────────────────────────────────────────
static void btn_plus_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        floor_up();
    }
}

static void btn_minus_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        floor_down();
    }
}

static void btn_confirm_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        goto_confirm_screen();
    }
}

static void btn_back_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        goto_main_screen();
    }
}

// ─── 메인 화면 생성 ──────────────────────────────────────────
static void create_main_screen(void) {
    scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_main, COLOR_BG, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);

    // ── 배경 글로우 원 (장식) ──
    lv_obj_t *bg_circle = lv_obj_create(scr_main);
    lv_obj_set_size(bg_circle, 480, 480); //기존 (360,360)
    lv_obj_align(bg_circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(bg_circle, COLOR_GLOW, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bg_circle, LV_OPA_50, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bg_circle, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bg_circle, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(bg_circle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // ── 진행률 아크 링 ──
    arc_ring = lv_arc_create(scr_main);
    lv_obj_set_size(arc_ring, 430, 430);
    lv_obj_align(arc_ring, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_mode(arc_ring, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(arc_ring, FLOOR_MIN, FLOOR_MAX);
    lv_arc_set_value(arc_ring, FLOOR_MIN);
    lv_arc_set_start_angle(arc_ring, 135);
    lv_arc_set_end_angle(arc_ring, 45);
    lv_arc_set_bg_angles(arc_ring, 135, 45);
    lv_obj_remove_style(arc_ring, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_ring, LV_OBJ_FLAG_CLICKABLE);

    // 아크 트랙 스타일
    lv_obj_set_style_arc_color(arc_ring, COLOR_RING_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_ring, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc_ring, LV_OPA_80, LV_PART_MAIN);

    // 아크 인디케이터 스타일
    lv_obj_set_style_arc_color(arc_ring, COLOR_RING_FILL, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_ring, 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_ring, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_ring, true, LV_PART_MAIN);

    // ── 층 번호 대형 표시 (커스텀 140px 폰트) ──
    lbl_floor = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_floor, FONT_FLOOR_NUM, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_floor, COLOR_FLOOR_NUM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_floor, "1");
    lv_obj_align(lbl_floor, LV_ALIGN_CENTER, 0, -20);

    // ── "층" 단위 텍스트 ──
    lbl_unit = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_unit, FONT_UNIT, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_unit, COLOR_FLOOR_DIM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_unit, "Floor");
    lv_obj_align(lbl_unit, LV_ALIGN_CENTER, 0, 110);

    // ── 최소/최대 층 표시 ──
    lbl_minmax = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_minmax, FONT_HINT, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_minmax, COLOR_FLOOR_DIM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_minmax, "1 ~ 50");
    lv_obj_align(lbl_minmax, LV_ALIGN_CENTER, 0, 80);

    // ── - 버튼 (좌측) ──
    btn_minus = lv_btn_create(scr_main);
    lv_obj_set_size(btn_minus, 75, 75);//기존 64, 64
    lv_obj_align(btn_minus, LV_ALIGN_CENTER, -140, 0); //기존 -110
    apply_btn_style(btn_minus, COLOR_BTN_MINUS);
    lv_obj_add_flag(btn_minus, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn_minus, btn_minus_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_minus, btn_minus_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    lv_obj_t *lbl_m = lv_label_create(btn_minus);
    lv_obj_set_style_text_font(lbl_m, FONT_BTN, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_m, COLOR_FLOOR_NUM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_m, "-");
    lv_obj_center(lbl_m);

    // ── + 버튼 (우측) ──
    btn_plus = lv_btn_create(scr_main);
    lv_obj_set_size(btn_plus, 75, 75); //기존 64, 64
    lv_obj_align(btn_plus, LV_ALIGN_CENTER, 140, 0); //기존 110
    apply_btn_style(btn_plus, COLOR_BTN_PLUS);
    lv_obj_add_event_cb(btn_plus, btn_plus_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_plus, btn_plus_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    lv_obj_t *lbl_p = lv_label_create(btn_plus);
    lv_obj_set_style_text_font(lbl_p, FONT_BTN, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_p, COLOR_FLOOR_NUM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_p, "+");
    lv_obj_center(lbl_p);

    // ── 확인 버튼 (하단) ──
    btn_confirm = lv_btn_create(scr_main);
    lv_obj_set_size(btn_confirm, 150, 52);
    lv_obj_align(btn_confirm, LV_ALIGN_CENTER, 0, 170); //기존 115
    lv_obj_set_style_bg_color(btn_confirm, COLOR_CONFIRM, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_CONFIRM_PR, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn_confirm, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_confirm, 26, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_confirm, COLOR_ACCENT, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_confirm, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_confirm, 20, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(btn_confirm, COLOR_ACCENT, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(btn_confirm, LV_OPA_40, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_confirm, btn_confirm_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_ok = lv_label_create(btn_confirm);
    lv_obj_set_style_text_font(lbl_ok, FONT_HINT, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_ok, COLOR_FLOOR_NUM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_ok, LV_SYMBOL_OK "  Check");
    lv_obj_center(lbl_ok);

    // ── 조작 힌트 (최상단) ──
    lbl_hint = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_hint, FONT_HINT, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_hint, COLOR_FLOOR_DIM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_hint, LV_SYMBOL_LOOP " Press / Touch");
    lv_obj_align(lbl_hint, LV_ALIGN_CENTER, 0, -150);

    // 초기 화면 업데이트
    update_floor_display();
}

// ─── 확인 화면 생성 ──────────────────────────────────────────
static void create_confirm_screen(void) {
    scr_confirm = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_confirm, COLOR_BG, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(scr_confirm, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_clear_flag(scr_confirm, LV_OBJ_FLAG_SCROLLABLE);

    // 배경 원
    lv_obj_t *bg = lv_obj_create(scr_confirm);
    lv_obj_set_size(bg, 460, 460); //기존 (300,300)
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(bg, COLOR_GLOW, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bg, LV_OPA_50, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bg, LV_RADIUS_CIRCLE, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(bg, COLOR_SUCCESS, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bg, 3, LV_STATE_DEFAULT);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // 체크 아이콘
    lv_obj_t *lbl_icon = lv_label_create(scr_confirm);
    lv_obj_set_style_text_font(lbl_icon, FONT_BTN, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_icon, COLOR_SUCCESS, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_icon, LV_SYMBOL_OK);
    lv_obj_align(lbl_icon, LV_ALIGN_CENTER, 0, -180);

    // "이동합니다" 텍스트
    lv_obj_t *lbl_going = lv_label_create(scr_confirm);
    lv_obj_set_style_text_font(lbl_going, FONT_HINT, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_going, COLOR_FLOOR_DIM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_going, "Target Floor");
    lv_obj_align(lbl_going, LV_ALIGN_CENTER, 0, -150);

    // 층 번호 대형 표시 (커스텀 140px 폰트)
    lbl_confirm_floor = lv_label_create(scr_confirm);
    lv_obj_set_style_text_font(lbl_confirm_floor, FONT_FLOOR_NUM, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_confirm_floor, COLOR_SUCCESS, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_confirm_floor, "1");
    lv_obj_align(lbl_confirm_floor, LV_ALIGN_CENTER, 0, 0);

    // "층" 단위
    lv_obj_t *lbl_u = lv_label_create(scr_confirm);
    lv_obj_set_style_text_font(lbl_u, FONT_UNIT, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_u, COLOR_FLOOR_DIM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_u, "Floor");
    lv_obj_align(lbl_u, LV_ALIGN_CENTER, 150, 0);

    // 돌아가기 버튼
    btn_back = lv_btn_create(scr_confirm);
    lv_obj_set_size(btn_back, 140, 52);
    lv_obj_align(btn_back, LV_ALIGN_CENTER, 0, 150);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x263238), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x1A237E), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_back, 26, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_back, COLOR_FLOOR_DIM, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_back, 1, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_bk = lv_label_create(btn_back);
    lv_obj_set_style_text_font(lbl_bk, FONT_HINT, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_bk, COLOR_FLOOR_NUM, LV_STATE_DEFAULT);
    lv_label_set_text(lbl_bk, LV_SYMBOL_LEFT "  Back");
    lv_obj_center(lbl_bk);
}

// ─── 자동 복귀 타이머 콜백 ──────────────────────────────────
static void auto_return_timer_cb(lv_timer_t *timer) {
    // 타이머 삭제 후 메인 화면으로 복귀
    if (g_auto_return_timer) {
        lv_timer_del(g_auto_return_timer);
        g_auto_return_timer = nullptr;
    }
    goto_main_screen();
}

// ─── 화면 전환 ───────────────────────────────────────────────
static void goto_confirm_screen(void) {
    // 확인 화면의 층 번호 업데이트
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", g_current_floor);
    lv_label_set_text(lbl_confirm_floor, buf);

    g_confirmed = true;
    Serial.printf("[Confirm] Go! %d Floor!\n", g_current_floor);

    lv_scr_load_anim(scr_confirm, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);

    // 기존 타이머가 있으면 삭제 후 새로 시작 (2000ms = 2초)
    if (g_auto_return_timer) {
        lv_timer_del(g_auto_return_timer);
        g_auto_return_timer = nullptr;
    }
    g_auto_return_timer = lv_timer_create(auto_return_timer_cb, 3000, nullptr);
    lv_timer_set_repeat_count(g_auto_return_timer, 1);  // 1회만 실행
}

static void goto_main_screen(void) {
    // 자동 복귀 타이머가 남아있으면 취소
    if (g_auto_return_timer) {
        lv_timer_del(g_auto_return_timer);
        g_auto_return_timer = nullptr;
    }
    g_confirmed = false;
    lv_scr_load_anim(scr_main, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
}

// ─── 엔코더/버튼 LVGL 이벤트 핸들러 ─────────────────────────
// KnobDir_t, ButtonEvent_t 는 ESP32_Knob / ESP32_Button 라이브러리에
// 이미 knob_event_t / button_event_t 로 정의되어 있으므로 재선언하지 않음.

extern "C" {

void LVGL_knob_event(void *dir) {
    knob_event_t d = (knob_event_t)(uintptr_t)dir;
    lv_obj_t *active_scr = lv_scr_act();

    // 확인 화면에서 엔코더 → 돌아가기
    if (active_scr == scr_confirm) {
        if (d == KNOB_LEFT || d == KNOB_RIGHT) {
            goto_main_screen();
        }
        return;
    }

    // 메인 화면에서 층 변경
    if (d == KNOB_RIGHT) {
        floor_up();
    } else if (d == KNOB_LEFT) {
        floor_down();
    }
}

void LVGL_button_event(void *event) {
    button_event_t ev = (button_event_t)(uintptr_t)event;
    lv_obj_t *active_scr = lv_scr_act();

    if (ev == BUTTON_SINGLE_CLICK) {
        if (active_scr == scr_main) {
            goto_confirm_screen();
        } else if (active_scr == scr_confirm) {
            goto_main_screen();
        }
    } else if (ev == BUTTON_LONG_PRESS_START) {
        // 롱프레스: 1층으로 초기화
        if (active_scr == scr_main) {
            set_floor(FLOOR_MIN);
            Serial.println("[Reset] 1 Floor Reset");
        }
    }
}

} // extern "C"

// ─── 엔코더 콜백 ─────────────────────────────────────────────
ESP_Knob *knob = nullptr;

void onKnobLeftEventCallback(int count, void *usr_data) {
    lvgl_port_lock(-1);
    LVGL_knob_event((void*)KNOB_LEFT);
    lvgl_port_unlock();
}

void onKnobRightEventCallback(int count, void *usr_data) {
    lvgl_port_lock(-1);
    LVGL_knob_event((void*)KNOB_RIGHT);
    lvgl_port_unlock();
}

static void SingleClickCb(void *button_handle, void *usr_data) {
    lvgl_port_lock(-1);
    LVGL_button_event((void*)BUTTON_SINGLE_CLICK);
    lvgl_port_unlock();
}

static void LongPressStartCb(void *button_handle, void *usr_data) {
    lvgl_port_lock(-1);
    LVGL_button_event((void*)BUTTON_LONG_PRESS_START);
    lvgl_port_unlock();
}

static void DoubleClickCb(void *button_handle, void *usr_data) {
    // 미사용
}

// ─── Setup ───────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("=== Elevator Floor Selector ===");

    // 보드 초기화
    Serial.println("[Init] Board initializing...");
    Board *board = new Board();
    board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif

    assert(board->begin());
    Serial.println("[Init] Board OK");

    // LVGL 초기화
    Serial.println("[Init] LVGL initializing...");
    lvgl_port_init(board->getLCD(), board->getTouch());
    Serial.println("[Init] LVGL OK");

    // 엔코더 초기화
    Serial.println("[Init] Knob initializing...");
    knob = new ESP_Knob(GPIO_NUM_KNOB_PIN_A, GPIO_NUM_KNOB_PIN_B);
    knob->begin();
    knob->attachLeftEventCallback(onKnobLeftEventCallback);
    knob->attachRightEventCallback(onKnobRightEventCallback);
    Serial.println("[Init] Knob OK");

    // 버튼 초기화
    Serial.println("[Init] Button initializing...");
    Button *btn = new Button(GPIO_BUTTON_PIN, false);
    btn->attachSingleClickEventCb(&SingleClickCb, NULL);
    btn->attachDoubleClickEventCb(&DoubleClickCb, NULL);
    btn->attachLongPressStartEventCb(&LongPressStartCb, NULL);
    Serial.println("[Init] Button OK");

    // UI 생성
    Serial.println("[Init] Creating UI...");
    lvgl_port_lock(-1);

    create_main_screen();
    create_confirm_screen();

    // 메인 화면으로 시작
    lv_disp_load_scr(scr_main);

    lvgl_port_unlock();

    Serial.println("[Init] UI OK");
    Serial.println("=== Ready! ===");
    Serial.println("  - 엔코더 우회전: 층 +1");
    Serial.println("  - 엔코더 좌회전: 층 -1");
    Serial.println("  - 버튼 클릭: 층 확인");
    Serial.println("  - 버튼 롱프레스: 1층 초기화");
    Serial.println("  - 터치 +/-: 층 변경");
}

// ─── Loop ────────────────────────────────────────────────────
void loop() {
    delay(1000);
    // 현재 층 시리얼 출력 (1초마다)
    Serial.printf("[Status] 현재 선택: %d층 | 확인: %s\n",
                  g_current_floor, g_confirmed ? "YES" : "NO");
}
