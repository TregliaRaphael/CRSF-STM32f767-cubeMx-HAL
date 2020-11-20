/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */
//MASTER BRANCH REF

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

//#include "platform.h"

//#ifdef USE_TELEMETRY_CRSF

//#include "build/atomic.h"	//unit tests *spit*
//#include "build/build_config.h"//maybe need to do mine
//#include "build/version.h"	// too

//#include "config/feature.h"	//useless maybe implem mine? nah
#include "pg/pg.h"		// only import PG_DECLARE, i removed other fct
//#include "pg/pg_ids.h"	//useless

#include "common/crc.h"		//GOOD
#include "common/maths.h"	//GOOD
//#include "common/printf.h"	// do my own far later
#include "common/mstreambuf.h"   //GOOD
#include "common/utils.h"	//GOOD

//#include "cms/cms.h"		// see yah soon

//#include "drivers/nvic.h"	//GOOD but not sure it's usefull, maybe remove later but keep quoted in case

//#include "config/config.h" 	//can be skipped, need remove some code
//#include "fc/rc_modes.h"	//same
//#include "fc/runtime_config.h"// to get all from fc/config

#include "flight/imu.h"		// for atttttitude
//#include "flight/position.h"	// for alllllltitude

#include "sensors/gps.h"	// gud but need replace by mine later
//#include "io/serial.h"	// do my own implem prior

#include "rx/crsf.h"		//GOOD
#include "rx/crsf_protocol.h"	//GOOD

#include "sensors/battery.h"	// GOOD
#include "sensors/sensors.h"	// free to include, no dependancy

//#include "telemetry/telemetry.h"     // next thing to implem to handle all my sensors ez

#include "telemetry/crsf_telemetry.h"	//GOOD


#define CRSF_CYCLETIME_US                   100000 // 100ms, 10 Hz
#define CRSF_DEVICEINFO_VERSION             0x01
#define CRSF_DEVICEINFO_PARAMETER_COUNT     0


static bool crsfTelemetryEnabled;
static bool deviceInfoReplyPending;
static uint8_t crsfFrame[CRSF_FRAME_SIZE_MAX];

//------------CHECKED-------------
static void crsfInitializeFrame(sbuf_t *dst)
{
    dst->ptr = crsfFrame;
    dst->end = ARRAYEND(crsfFrame);

    sbufWriteU8(dst, CRSF_SYNC_BYTE);
}


//------------CHECKED-------------
static void crsfFinalize(sbuf_t *dst)
{
    crc8_dvb_s2_sbuf_append(dst, &crsfFrame[2]); // start at byte 2, since CRC does not include device address and frame length
    sbufSwitchToReader(dst, crsfFrame);
    // write the telemetry frame to the receiver.
    crsfRxWriteTelemetryData(sbufPtr(dst), sbufBytesRemaining(dst));
}


/*
CRSF frame has the structure:
<Device address> <Frame length> <Type> <Payload> <CRC>
Device address: (uint8_t)
Frame length:   length in  bytes including Type (uint8_t)
Type:           (uint8_t)
CRC:            (uint8_t), crc of <Type> and <Payload>
*/

/*
------------CHECKED-------------
0x02 GPS
Payload:
int32_t     Latitude ( degree / 10`000`000 )
int32_t     Longitude (degree / 10`000`000 )
uint16_t    Groundspeed ( km/h / 10 )
uint16_t    GPS heading ( degree / 100 )
uint16      Altitude ( meter ­1000m offset )
uint8_t     Satellites in use ( counter )
*/
void crsfFrameGps(sbuf_t *dst)
{
    // use sbufWrite since CRC does not include frame length
    sbufWriteU8(dst, CRSF_FRAME_GPS_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_TYPE_CRC);
    sbufWriteU8(dst, CRSF_FRAMETYPE_GPS);
    sbufWriteU32BigEndian(dst, gpsSol.llh.lat); // CRSF and betaflight use same units for degrees
    sbufWriteU32BigEndian(dst, gpsSol.llh.lon);
    sbufWriteU16BigEndian(dst, (gpsSol.groundSpeed * 36 + 50) / 100); // gpsSol.groundSpeed is in cm/s
    sbufWriteU16BigEndian(dst, gpsSol.groundCourse * 10); // gpsSol.groundCourse is degrees * 10
    //can replace altitude by random number, guess later
    //const uint16_t altitude = (constrain(getEstimatedAltitudeCm(), 0 * 100, 5000 * 100) / 100) + 1000; // constrain altitude from 0 to 5,000m
    const uint16_t altitude = gpsSol.hdop;  //to replace to real altitude///////////
    sbufWriteU16BigEndian(dst, altitude);
    sbufWriteU8(dst, gpsSol.numSat);
}


/*
------------CHECKED-------------
0x08 Battery sensor
Payload:
uint16_t    Voltage ( mV * 100 )
uint16_t    Current ( mA * 100 )
uint24_t    Fuel ( drawn mAh )
uint8_t     Battery remaining ( percent )
*/
void crsfFrameBatterySensor(sbuf_t *dst)
{
    // use sbufWrite since CRC does not include frame length
    sbufWriteU8(dst, CRSF_FRAME_BATTERY_SENSOR_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_TYPE_CRC);
    sbufWriteU8(dst, CRSF_FRAMETYPE_BATTERY_SENSOR);
    //get random value for voltage is fine
    /*if (telemetryConfig()->report_cell_voltage) {
        sbufWriteU16BigEndian(dst, (getBatteryAverageCellVoltage() + 5) / 10); // vbat is in units of 0.01V
    } else {
        sbufWriteU16BigEndian(dst, getLegacyBatteryVoltage());
    }*/
    sbufWriteU16BigEndian(dst, getLegacyBatteryVoltage());
    sbufWriteU16BigEndian(dst, getAmperage() / 10);
    const uint32_t mAhDrawn = getMAhDrawn();
    const uint8_t batteryRemainingPercentage = calculateBatteryPercentageRemaining();
    sbufWriteU8(dst, (mAhDrawn >> 16));
    sbufWriteU8(dst, (mAhDrawn >> 8));
    sbufWriteU8(dst, (uint8_t)mAhDrawn);
    sbufWriteU8(dst, batteryRemainingPercentage);
}

typedef enum {
    CRSF_ACTIVE_ANTENNA1 = 0,
    CRSF_ACTIVE_ANTENNA2 = 1
} crsfActiveAntenna_e;

typedef enum {
    CRSF_RF_MODE_4_HZ = 0,
    CRSF_RF_MODE_50_HZ = 1,
    CRSF_RF_MODE_150_HZ = 2
} crsrRfMode_e;

typedef enum {
    CRSF_RF_POWER_0_mW = 0,
    CRSF_RF_POWER_10_mW = 1,
    CRSF_RF_POWER_25_mW = 2,
    CRSF_RF_POWER_100_mW = 3,
    CRSF_RF_POWER_500_mW = 4,
    CRSF_RF_POWER_1000_mW = 5,
    CRSF_RF_POWER_2000_mW = 6
} crsrRfPower_e;

/*
------------CHECKED-------------
0x1E Attitude
Payload:
int16_t     Pitch angle ( rad / 10000 )
int16_t     Roll angle ( rad / 10000 )
int16_t     Yaw angle ( rad / 10000 )
*/

#define DECIDEGREES_TO_RADIANS10000(angle) ((int16_t)(1000.0f * (angle) * RAD))

void crsfFrameAttitude(sbuf_t *dst)
{
     sbufWriteU8(dst, CRSF_FRAME_ATTITUDE_PAYLOAD_SIZE + CRSF_FRAME_LENGTH_TYPE_CRC);
     sbufWriteU8(dst, CRSF_FRAMETYPE_ATTITUDE);
     sbufWriteU16BigEndian(dst, DECIDEGREES_TO_RADIANS10000(attitude.values.pitch));
     sbufWriteU16BigEndian(dst, DECIDEGREES_TO_RADIANS10000(attitude.values.roll));
     sbufWriteU16BigEndian(dst, DECIDEGREES_TO_RADIANS10000(attitude.values.yaw));
}

/*
------------CHECKED-------------
0x21 Flight mode text based
Payload:
char[]      Flight mode ( Null terminated string )
*/
void crsfFrameFlightMode(sbuf_t *dst)
{
    // write zero for frame length, since we don't know it yet
    uint8_t *lengthPtr = sbufPtr(dst);
    sbufWriteU8(dst, 0);
    sbufWriteU8(dst, CRSF_FRAMETYPE_FLIGHT_MODE);

    // Acro is the default mode
    const char *flightMode = "ACRO";

    /*// Modes that are only relevant when disarmed
    if (!ARMING_FLAG(ARMED) && isArmingDisabled()) {
        flightMode = "!ERR";
    } else

#if defined(USE_GPS)
    if (!ARMING_FLAG(ARMED) && featureIsEnabled(FEATURE_GPS) && (!STATE(GPS_FIX) || !STATE(GPS_FIX_HOME))) {
        flightMode = "WAIT"; // Waiting for GPS lock
    } else
#endif
    // Flight modes in decreasing order of importance
    if (FLIGHT_MODE(FAILSAFE_MODE)) {
        flightMode = "!FS!";
    } else if (FLIGHT_MODE(GPS_RESCUE_MODE)) {
        flightMode = "RTH";
    } else if (FLIGHT_MODE(PASSTHRU_MODE)) {
        flightMode = "MANU";
    } else if (FLIGHT_MODE(ANGLE_MODE)) {
        flightMode = "STAB";
    } else if (FLIGHT_MODE(HORIZON_MODE)) {
        flightMode = "HOR";
    } else if (airmodeIsEnabled()) {
        flightMode = "AIR";
    }*/

    //remove other to put the the string we wanted
    sbufWriteString(dst, flightMode);
    /*if (!ARMING_FLAG(ARMED)) {
        sbufWriteU8(dst, '*');
    }*/
    sbufWriteU8(dst, '*'); //to remove maybe
    sbufWriteU8(dst, '\0');     // zero-terminate string
    // write in the frame length
    *lengthPtr = sbufPtr(dst) - lengthPtr;
}

/*
------------CHECKED-------------
0x29 Device Info
Payload:
uint8_t     Destination
uint8_t     Origin
char[]      Device Name ( Null terminated string )
uint32_t    Null Bytes
uint32_t    Null Bytes
uint32_t    Null Bytes
uint8_t     255 (Max MSP Parameter)
uint8_t     0x01 (Parameter version 1)
*/
void crsfFrameDeviceInfo(sbuf_t *dst) {

    char buff[30] = "RaphFlight lel: having_fun_baa";
    //tfp_sprintf(buff, "%s %s: %s", FC_FIRMWARE_NAME, FC_VERSION_STRING, systemConfig()->boardIdentifier);

    uint8_t *lengthPtr = sbufPtr(dst);
    sbufWriteU8(dst, 0);
    sbufWriteU8(dst, CRSF_FRAMETYPE_DEVICE_INFO);
    sbufWriteU8(dst, CRSF_ADDRESS_RADIO_TRANSMITTER);
    sbufWriteU8(dst, CRSF_ADDRESS_FLIGHT_CONTROLLER);
    sbufWriteStringWithZeroTerminator(dst, buff);
    for (unsigned int ii=0; ii<12; ii++) {
        sbufWriteU8(dst, 0x00);
    }
    sbufWriteU8(dst, CRSF_DEVICEINFO_PARAMETER_COUNT);
    sbufWriteU8(dst, CRSF_DEVICEINFO_VERSION);
    *lengthPtr = sbufPtr(dst) - lengthPtr;
}


#define BV(x)  (1 << (x)) // bit value

// schedule array to decide how often each type of frame is sent
typedef enum {
    CRSF_FRAME_START_INDEX = 0,
    CRSF_FRAME_ATTITUDE_INDEX = CRSF_FRAME_START_INDEX,
    CRSF_FRAME_BATTERY_SENSOR_INDEX,
    CRSF_FRAME_FLIGHT_MODE_INDEX,
    CRSF_FRAME_GPS_INDEX,
    CRSF_SCHEDULE_COUNT_MAX
} crsfFrameTypeIndex_e;

static uint8_t crsfScheduleCount;
static uint8_t crsfSchedule[CRSF_SCHEDULE_COUNT_MAX];

//-------------CHECKED--------------
static void processCrsf(void)
{
    static uint8_t crsfScheduleIndex = 0;

    const uint8_t currentSchedule = crsfSchedule[crsfScheduleIndex];

    sbuf_t crsfPayloadBuf;
    sbuf_t *dst = &crsfPayloadBuf;

    if (currentSchedule & BV(CRSF_FRAME_ATTITUDE_INDEX)) {
        crsfInitializeFrame(dst);
        crsfFrameAttitude(dst);
        crsfFinalize(dst);
    }
    if (currentSchedule & BV(CRSF_FRAME_BATTERY_SENSOR_INDEX)) {
        crsfInitializeFrame(dst);
        crsfFrameBatterySensor(dst);
        crsfFinalize(dst);
    }

    if (currentSchedule & BV(CRSF_FRAME_FLIGHT_MODE_INDEX)) {
        crsfInitializeFrame(dst);
        crsfFrameFlightMode(dst);
        crsfFinalize(dst);
    }
//#ifdef USE_GPS
    if (currentSchedule & BV(CRSF_FRAME_GPS_INDEX)) {
        crsfInitializeFrame(dst);
        crsfFrameGps(dst);
        crsfFinalize(dst);
    }
//#endif
    crsfScheduleIndex = (crsfScheduleIndex + 1) % crsfScheduleCount;
}

void crsfScheduleDeviceInfoResponse(void)
{
    deviceInfoReplyPending = true;
}


//-------------CHECKED--------------
void initCrsfTelemetry(void)
{
    // check if there is a serial port open for CRSF telemetry (ie opened by the CRSF RX)
    // and feature is enabled, if so, set CRSF telemetry enabled
    //crsfTelemetryEnabled = crsfRxIsActive(); active if serialPort opened so... true lel
    crsfTelemetryEnabled = true;

    /*if (!crsfTelemetryEnabled) {
        return;
    }*/

    deviceInfoReplyPending = false;
    int index = 0;
    //if (sensors(SENSOR_ACC) && telemetryIsSensorEnabled(SENSOR_PITCH | SENSOR_ROLL | SENSOR_HEADING)) {
    crsfSchedule[index++] = BV(CRSF_FRAME_ATTITUDE_INDEX);
    //}
    //if ((isBatteryVoltageConfigured() && telemetryIsSensorEnabled(SENSOR_VOLTAGE))
    //    || (isAmperageConfigured() && telemetryIsSensorEnabled(SENSOR_CURRENT | SENSOR_FUEL))) {
    crsfSchedule[index++] = BV(CRSF_FRAME_BATTERY_SENSOR_INDEX);
    //}
    crsfSchedule[index++] = BV(CRSF_FRAME_FLIGHT_MODE_INDEX);
//#ifdef USE_GPS
   // if (featureIsEnabled(FEATURE_GPS)
    //   && telemetryIsSensorEnabled(SENSOR_ALTITUDE | SENSOR_LAT_LONG | SENSOR_GROUND_SPEED | SENSOR_HEADING)) {
        crsfSchedule[index++] = BV(CRSF_FRAME_GPS_INDEX);
    //}
//#endif
    crsfScheduleCount = (uint8_t)index;
 }

bool checkCrsfTelemetryState(void)
{
    return crsfTelemetryEnabled;
}


/*
 * Called periodically by the scheduler
 *
 * this function try to send data to rx everytime she's called by scheduler 
 * thanks to crsfRxSendTelemetryData();
 * if deviceInfos is asked by receiver, so do a frame to send to the receiver
 * else do a data frame if it's in time 10Hz refresh
 */
void handleCrsfTelemetry(timeUs_t currentTimeUs)
{
    static uint32_t crsfLastCycleTime;

    if (!crsfTelemetryEnabled) {
        return;
    }
    // Give the receiver a chance to send any outstanding telemetry data.
    // This needs to be done at high frequency, to enable the RX to send the telemetry frame
    // in between the RX frames.
    crsfRxSendTelemetryData();

    // Send ad-hoc response frames as soon as possible
    // is triggered by receiver in rx/crsf.c
    if (deviceInfoReplyPending) {
        sbuf_t crsfPayloadBuf;
        sbuf_t *dst = &crsfPayloadBuf;
        crsfInitializeFrame(dst);
        crsfFrameDeviceInfo(dst);
        crsfFinalize(dst);
        deviceInfoReplyPending = false;
        crsfLastCycleTime = currentTimeUs; // reset telemetry timing due to ad-hoc request
        return;
    }

    // Actual telemetry data only needs to be sent at a low frequency, ie 10Hz
    // Spread out scheduled frames evenly so each frame is sent at the same frequency.
    if (currentTimeUs >= crsfLastCycleTime + (CRSF_CYCLETIME_US / crsfScheduleCount)) {
        crsfLastCycleTime = currentTimeUs;
        processCrsf();
    }
}

//#endif

