#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common/time.h" //maybe import or check use and get our own HAL
#include "stm32f7xx_hal.h"
#include "rx/crsf_protocol.h"

typedef struct crsfTelDebug_s {
    uint32_t nbrAtt;
    uint32_t nbrBatt;
    uint32_t nbrFlight;
    uint32_t nbrGPS;
    uint8_t two;
    uint8_t three;
    uint8_t four;
} crsfTelDebug_t;

extern crsfTelDebug_t crsfTelDebugs;

void initCrsfTelemetry(void);
bool checkCrsfTelemetryState(void);
void handleCrsfTelemetry(timeUs_t currentTimeUs);
void crsfScheduleDeviceInfoResponse(void);
