#include "imu.h"

attitudeEulerAngles_t attitude;

void imuInit(void){
	attitude.values.roll = 111;
	attitude.values.pitch = 222;
	attitude.values.yaw = 333;
}
