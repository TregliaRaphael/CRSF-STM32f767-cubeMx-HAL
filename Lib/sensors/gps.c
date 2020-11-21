#include "gps.h"

gpsSolutionData_t gpsSol;

void gpsInit(void)
{
	gpsSol.numSat = 5;
	gpsSol.hdop = 654;
	gpsSol.groundCourse = 1234;
	gpsSol.groundSpeed = 4567;
	gpsSol.speed3d = 5678;
	gpsSol.llh.lat = 1312021;
	gpsSol.llh.lon = 987456;
	gpsSol.llh.altCm = 1000;
}
