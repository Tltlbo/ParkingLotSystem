#include "parkingModel.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static ParkingSystem_t g_parking_sys;

void parkingModelInit(void) {
    memset(&g_parking_sys, 0, sizeof(ParkingSystem_t));
}

/**
 * @brief $PARK 패킷 파싱
 * 예: "$PARK,A-01:0,A-02:1,B-01:0"
 */
bool parkingModelParsePacket(const char *packet_str) {
    if (packet_str == NULL || strncmp(packet_str, "$PARK", 5) != 0) {
        return false;
    }

    /* 원본 문자열 수정을 방지하기 위해 로컬 복사 */
    printf("%s\n", packet_str);
    char temp_buf[256];
    strncpy(temp_buf, packet_str, sizeof(temp_buf) - 1);
    temp_buf[sizeof(temp_buf) - 1] = '\0';

    char *saveptr;
    char *token = strtok_r(temp_buf, ",", &saveptr); /* "$PARK" 건너뛰기 */

    uint8_t count = 0;
    uint8_t occupied = 0;

    while ((token = strtok_r(NULL, ",", &saveptr)) != NULL && count < MAX_PARKING_SLOTS) {
        /* token 형식: "A-01:0" */
        char section = 0;
        int number = 0;
        int status = 0;

        if (sscanf(token, "%c-%d:%d", &section, &number, &status) == 3) {
            g_parking_sys.slots[count].section = section;
            g_parking_sys.slots[count].number = (uint8_t)number;
            g_parking_sys.slots[count].is_occupied = (status == 1);
            g_parking_sys.slots[count].is_valid = true;

            if (status == 1) {
                occupied++;
            }
            count++;
        }
    }

    g_parking_sys.total_count = count;
    g_parking_sys.occupied_count = occupied;
    g_parking_sys.empty_count = count - occupied;

    return true;
}

const ParkingSystem_t* parkingModelGetSystem(void) {
    return &g_parking_sys;
}