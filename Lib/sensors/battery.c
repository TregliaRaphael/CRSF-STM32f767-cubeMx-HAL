#include "battery.h"

uint16_t vbat = 100;
uint32_t amp = 98431;
uint32_t mahDrawn = 12456;
uint8_t percentBat = 50;


uint16_t getLegacyBatteryVoltage(void){
  return vbat;
}

int32_t getAmperage(void){
	return amp;
}

int32_t getMAhDrawn(void){
	return mahDrawn;
}

uint8_t calculateBatteryPercentageRemaining(void){
	return percentBat;
}
