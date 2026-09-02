#include "parkingDisplay.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
#include <string.h>

// 슬롯 하나의 가로/세로 크기
#define SLOT_W 18
#define SLOT_H 18

void parkingDisplayInit(I2C_HandleTypeDef *hi2c) {
    ssd1306_Init(hi2c, I2C_OLED_ADDR);
}

// 특정 주차 슬롯을 그려주는 내부 함수
static void drawSlot(uint8_t x, uint8_t y, const char *label, bool is_occupied, bool is_recommended) {
    // 1. 테두리 그리기
    ssd1306_DrawRect(x, y, SLOT_W, SLOT_H, White);
    
    // 추천 자리인 경우 이중 테두리
    if (is_recommended) {
        ssd1306_DrawRect(x + 1, y + 1, SLOT_W - 2, SLOT_H - 2, White);
    }

    // 2. 라벨 텍스트 그리기 (가운데 위쪽)
    ssd1306_SetCursor(x + 3, y + 2);
    // 라벨 출력
    char buf[4];
    strncpy(buf, label, 3);
    buf[3] = '\0';
    ssd1306_WriteString(buf, Font_6x8, White);
    
    // 3. 점유 상태 및 기호 그리기
    ssd1306_SetCursor(x + 6, y + 10);
    if (is_occupied) {
        // 주차 불가 (X 표시)
        ssd1306_WriteChar('X', Font_6x8, White);
    } else if (is_recommended) {
        // 추천 자리 표시 (별 표시, '*')
        ssd1306_WriteChar('*', Font_6x8, White);
    } else {
        // 주차 가능 자리 표시 (O 표시)
        ssd1306_WriteChar('O', Font_6x8, White);
    }
}

// 특정 주차 구역의 상태를 찾는 헬퍼 함수
static bool getSlotOccupied(const ParkingSystem_t *sys, char section, uint8_t number) {
    for (int i = 0; i < MAX_PARKING_SLOTS; i++) {
        if (sys->slots[i].is_valid && 
            sys->slots[i].section == section && 
            sys->slots[i].number == number) {
            return sys->slots[i].is_occupied;
        }
    }
    return false;
}

// 주차가 편한 자리인지 확인하는 함수 (양 옆에 차가 없는 경우)
static bool is_easy_to_park(const ParkingSystem_t *sys, char section, uint8_t num) {
    if (getSlotOccupied(sys, section, num)) return false;
    
    bool left_occ = false;
    bool right_occ = false;
    
    if (section == 'A') {
        if (num > 1) left_occ = getSlotOccupied(sys, 'A', num - 1);
        if (num < 6) right_occ = getSlotOccupied(sys, 'A', num + 1);
    } else if (section == 'B') {
        if (num == 1) right_occ = getSlotOccupied(sys, 'B', 2);
        if (num == 2) left_occ = getSlotOccupied(sys, 'B', 1);
    } else if (section == 'C') {
        if (num == 1) right_occ = getSlotOccupied(sys, 'C', 2);
        if (num == 2) left_occ = getSlotOccupied(sys, 'C', 1);
    }
    
    return (!left_occ && !right_occ);
}

// 슬롯이 추천 대상인지 확인하는 함수
static bool is_slot_recommended(const ParkingSystem_t *sys, char section, uint8_t num) {
    if (getSlotOccupied(sys, section, num)) return false;

    // 1순위: A4, A5, A6 중 빈 자리 확인
    bool p1_available = (!getSlotOccupied(sys, 'A', 4) || !getSlotOccupied(sys, 'A', 5) || !getSlotOccupied(sys, 'A', 6));

    if (p1_available) {
        return (section == 'A' && (num == 4 || num == 5 || num == 6));
    }

    // 2순위: 양 쪽에 차가 없는 주차하기 편한 자리 확인
    bool p2_available = false;
    for (int i = 1; i <= 6; i++) {
        if (is_easy_to_park(sys, 'A', i)) { p2_available = true; break; }
    }
    if (!p2_available) {
        for (int i = 1; i <= 2; i++) {
            if (is_easy_to_park(sys, 'B', i)) { p2_available = true; break; }
            if (is_easy_to_park(sys, 'C', i)) { p2_available = true; break; }
        }
    }

    if (p2_available) {
        return is_easy_to_park(sys, section, num);
    }

    // 1, 2순위 모두 없으면 남은 빈자리 추천
    return true;
}

void parkingDisplayRenderSlots(const ParkingSystem_t *sys) {
    ssd1306_Fill(Black); // 화면 버퍼 클리어
    
    // 1. 상단 상태 요약 바
    char top_buf[32];
    snprintf(top_buf, sizeof(top_buf), "PARK EMPTY: %d/10", sys->empty_count);
    ssd1306_SetCursor(2, 1);
    ssd1306_WriteString(top_buf, Font_6x8, White);
    
    // 2. 상단 구분선 (Y = 11)
    ssd1306_DrawLine(0, 11, 127, 11, White);
    
    // 3. A구역 (A1 ~ A6) 렌더링 (Y = 14)
    int spacing = 3;
    for (int num = 1; num <= 6; num++) {
        int x = spacing + (num - 1) * (SLOT_W + spacing);
        char label[5];
        snprintf(label, sizeof(label), "A%d", num);
        bool is_occ = getSlotOccupied(sys, 'A', num);
        bool is_rec = is_slot_recommended(sys, 'A', num);
        drawSlot(x, 14, label, is_occ, is_rec);
    }
    
    // 4. 중앙 주행 통로 라인 (점선: Y = 38)
    for (int i = 0; i < 128; i += 8) {
        ssd1306_DrawLine(i, 38, i + 4, 38, White);
    }
    
    // 5. 하단 구역 렌더링 (Y = 44)
    // B구역 (B1, B2) - 좌측
    for (int num = 1; num <= 2; num++) {
        int x = spacing + (num - 1) * (SLOT_W + spacing);
        char label[5];
        snprintf(label, sizeof(label), "B%d", num);
        bool is_occ = getSlotOccupied(sys, 'B', num);
        bool is_rec = is_slot_recommended(sys, 'B', num);
        drawSlot(x, 44, label, is_occ, is_rec);
    }
    
    // 중앙 입구 (IN GATE)
    ssd1306_DrawRect(50, 44, 28, 18, White);
    ssd1306_SetCursor(58, 49);
    ssd1306_WriteString("IN", Font_6x8, White);
    
    // C구역 (C1, C2) - 우측
    for (int num = 1; num <= 2; num++) {
        int idx = (num == 1) ? 4 : 5;
        int x = spacing + idx * (SLOT_W + spacing);
        char label[5];
        snprintf(label, sizeof(label), "C%d", num);
        bool is_occ = getSlotOccupied(sys, 'C', num);
        bool is_rec = is_slot_recommended(sys, 'C', num);
        drawSlot(x, 44, label, is_occ, is_rec);
    }
    
    // 6. OLED 화면으로 전송
    ssd1306_UpdateScreen();
}
