#include "parkingModel.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static ParkingSystem_t g_parking_sys;

static void updateCounts(void) {
    uint8_t count = 0;
    uint8_t occupied = 0;
    for (int i = 0; i < MAX_PARKING_SLOTS; i++) {
        if (g_parking_sys.slots[i].is_valid) {
            count++;
            if (g_parking_sys.slots[i].is_occupied) {
                occupied++;
            }
        }
    }
    g_parking_sys.total_count = count;
    g_parking_sys.occupied_count = occupied;
    g_parking_sys.empty_count = count - occupied;
}

void parkingModelInit(void) {
    memset(&g_parking_sys, 0, sizeof(ParkingSystem_t));

    // A구역 (A1 ~ A6) 기본 빈자리 초기화
    for (int i = 0; i < 6; i++) {
        g_parking_sys.slots[i].section = 'A';
        g_parking_sys.slots[i].number = i + 1;
        g_parking_sys.slots[i].is_occupied = false;
        g_parking_sys.slots[i].is_valid = true;
    }
    // B구역 (B1, B2) 기본 빈자리 초기화
    for (int i = 0; i < 2; i++) {
        g_parking_sys.slots[6 + i].section = 'B';
        g_parking_sys.slots[6 + i].number = i + 1;
        g_parking_sys.slots[6 + i].is_occupied = false;
        g_parking_sys.slots[6 + i].is_valid = true;
    }
    // C구역 (C1, C2) 기본 빈자리 초기화
    for (int i = 0; i < 2; i++) {
        g_parking_sys.slots[8 + i].section = 'C';
        g_parking_sys.slots[8 + i].number = i + 1;
        g_parking_sys.slots[8 + i].is_occupied = false;
        g_parking_sys.slots[8 + i].is_valid = true;
    }

    g_parking_sys.total_count = 10;
    g_parking_sys.occupied_count = 0;
    g_parking_sys.empty_count = 10;
}

/**
 * @brief $PARK 및 $EVENT 패킷 파싱
 * 예1: "$PARK,A-01:1,A-02:0,A-03:0,A-04:1,A-05:0,A-06:0,B-01:0,B-02:1,C-01:0,C-02:0"
 * 예2: "$EVENT,A-01:OCCUPIED"
 */
bool parkingModelParsePacket(const char *packet_str) {
    if (packet_str == NULL) return false;

    // 1. $PARK 패킷 검색 (버퍼 중간에 있어도 감지)
    const char *park_ptr = strstr(packet_str, "$PARK");
    if (park_ptr != NULL) {
        char temp_buf[256];
        strncpy(temp_buf, park_ptr, sizeof(temp_buf) - 1);
        temp_buf[sizeof(temp_buf) - 1] = '\0';

        char *saveptr = NULL;
        char *token = strtok_r(temp_buf, ",\r\n", &saveptr); /* "$PARK" 건너뛰기 */

        uint8_t parsed_count = 0;

        while ((token = strtok_r(NULL, ",\r\n", &saveptr)) != NULL) {
            char section = 0;
            int number = 0;
            int status = 0;

            // "A-01:1", "A-1:1", "A1:1", "A01:1" 등 다양한 형식 지원
            if (sscanf(token, "%c-%d:%d", &section, &number, &status) == 3 ||
                sscanf(token, "%c%d:%d", &section, &number, &status) == 3) {
                
                // 대문자 변환
                if (section >= 'a' && section <= 'z') section -= 32;

                // 기존 슬롯에서 일치하는 슬롯 상태 갱신
                for (int i = 0; i < MAX_PARKING_SLOTS; i++) {
                    if (g_parking_sys.slots[i].is_valid &&
                        g_parking_sys.slots[i].section == section &&
                        g_parking_sys.slots[i].number == (uint8_t)number) {
                        g_parking_sys.slots[i].is_occupied = (status == 1);
                        parsed_count++;
                        break;
                    }
                }
            }
        }

        if (parsed_count > 0) {
            updateCounts();
            printf("[PARSER] Successfully updated %d slots. Empty: %d/10\r\n", parsed_count, g_parking_sys.empty_count);
            return true;
        }
    }

    // 2. $EVENT 단일 이벤트 패킷 검색 ($EVENT,A-01:OCCUPIED 또는 $EVENT,A-01:1)
    const char *event_ptr = strstr(packet_str, "$EVENT");
    if (event_ptr != NULL) {
        char section = 0;
        int number = 0;
        char status_str[16] = {0};

        if (sscanf(event_ptr, "$EVENT,%c-%d:%15s", &section, &number, status_str) == 3 ||
            sscanf(event_ptr, "$EVENT,%c%d:%15s", &section, &number, status_str) == 3) {
            
            if (section >= 'a' && section <= 'z') section -= 32;
            bool is_occupied = (strncmp(status_str, "OCCUPIED", 8) == 0 || 
                                strcmp(status_str, "1") == 0 ||
                                strncmp(status_str, "occ", 3) == 0);

            for (int i = 0; i < MAX_PARKING_SLOTS; i++) {
                if (g_parking_sys.slots[i].is_valid && 
                    g_parking_sys.slots[i].section == section && 
                    g_parking_sys.slots[i].number == (uint8_t)number) {
                    g_parking_sys.slots[i].is_occupied = is_occupied;
                    updateCounts();
                    printf("[PARSER] Event Slot %c%d -> %s. Empty: %d/10\r\n", 
                           section, number, is_occupied ? "OCCUPIED" : "EMPTY", g_parking_sys.empty_count);
                    return true;
                }
            }
        }
    }

    return false;
}

const ParkingSystem_t* parkingModelGetSystem(void) {
    return &g_parking_sys;
}