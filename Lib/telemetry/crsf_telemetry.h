#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common/time.h" //maybe import or check use and get our own HAL
#include "stm32f7xx_hal.h"
#include "rx/crsf_protocol.h"

void initCrsfTelemetry(void);
bool checkCrsfTelemetryState(void);
void handleCrsfTelemetry(timeUs_t currentTimeUs);
void crsfScheduleDeviceInfoResponse(void);
