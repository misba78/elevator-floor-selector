# 🛗 Elevator Floor Selector

**VIEWE UEDX46460015-MD50ET** (1.5인치 466×466 원형 디스플레이) 기반  
ESP32-S3R8 엘리베이터 층 선택 장치 (1층 ~ 50층)

---

## 📱 UI 구성

### 메인 화면
```
      ┌────────────────────┐
      │   ⟳ 엔코더/터치     │   ← 힌트 텍스트
      │                    │
      │  ╭──────────────╮  │
      │  │   ━━━━━━━━━  │  │   ← 진행률 아크 링 (파랑)
      │  │              │  │
      │ [-]    17       [+] │   ← - 버튼 / 층번호 / + 버튼
      │  │     층        │  │
      │  │   1 ~ 50     │  │
      │  │   [ 확인 ]   │  │   ← 확인 버튼
      │  ╰──────────────╯  │
      └────────────────────┘
```

### 확인 화면
```
      ┌────────────────────┐
      │                    │
      │        ✓           │   ← 체크 아이콘
      │      목적지         │
      │        17          │   ← 선택한 층 (초록 강조)
      │        층           │
      │   [ 돌아가기 ]      │
      └────────────────────┘
```

---

## 🎮 조작 방법

| 조작 | 동작 |
|------|------|
| 엔코더 우회전 | 층 +1 (1→2→...→50) |
| 엔코더 좌회전 | 층 -1 (50→49→...→1) |
| 터치 **[+]** 버튼 | 층 +1 |
| 터치 **[-]** 버튼 | 층 -1 |
| 버튼 클릭 (IO0) | 층 확인 (확인 화면으로) |
| 버튼 롱프레스 | 1층으로 초기화 |
| 확인 화면에서 엔코더/버튼 | 메인 화면으로 돌아가기 |

---

## ⚙️ Arduino IDE 설정

| 설정 항목 | 값 |
|-----------|-----|
| Board | ESP32S3 Dev Module |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO |
| Flash Frequency | 80MHz |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| PSRAM | OPI PSRAM |
| USB CDC On Boot | Enabled (USB 포트 사용시) |
| Upload Speed | 921600 |

---

## 📦 필요 라이브러리

1. **ESP32_Display_Panel** ≥ 1.0.3
   - Arduino Library Manager 또는 VIEWE GitHub에서 설치
2. **lvgl** v8.4.0
3. **ESP32_Knob** (Espressif)
4. **ESP32_Button** (Espressif)

---

## 📁 파일 구성

```
elevator_floor_selector/
├── elevator_floor_selector.ino   ← 메인 스케치 (UI + 로직)
├── lvgl_v8_port.cpp              ← LVGL 포팅 (변경 없음)
├── lvgl_v8_port.h                ← LVGL 포트 헤더
├── lv_conf.h                     ← LVGL 설정 (LV_COLOR_16_SWAP=1)
├── esp_panel_board_supported_conf.h  ← 보드: UEDX46460015_MD50ET 활성화됨
├── esp_panel_drivers_conf.h      ← 드라이버 설정
├── esp_utils_conf.h              ← 유틸 설정
└── README.md
```

---

## 🔌 핀 배선

| 기능 | GPIO |
|------|------|
| LCD CS | IO12 |
| LCD PCLK | IO10 |
| LCD DATA0~3 | IO13, IO11, IO14, IO9 |
| LCD RST | IO8 |
| 백라이트 | IO17 |
| Touch SDA | IO0 |
| Touch SCL | IO1 |
| Touch RST | IO3 |
| Touch INT | IO4 |
| 엔코더 PHA | IO6 |
| 엔코더 PHB | IO5 |
| 버튼 (BOOT) | IO0 |

---

## 🎨 디자인 컨셉

- **딥 네이비** 배경으로 고급스러운 엘리베이터 패널 느낌
- **층에 따른 색상 변화**: 저층(파랑) → 중층(하늘) → 고층(시안)
- **아크 링** 진행률 표시로 현재 층 위치 직관적 파악
- 466×466 **원형 디스플레이** 최적화 레이아웃
