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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#ifdef USE_SERIALRX_CRSF

#include "common/time.h"	
#include "common/crc.h"		
#include "common/maths.h"	
#include "common/utils.h"	


#include "pg/rx.h"          //rxConfig
#include "rx/rx.h"
#include "rx/crsf.h"		

#include "telemetry/crsf_telemetry.h"	

#define CRSF_TIME_NEEDED_PER_FRAME_US   1100 // 700 ms + 400 ms for potential ad-hoc request
#define CRSF_TIME_BETWEEN_FRAMES_US     6667 // At fastest, frames are sent by the transmitter every 6.667 milliseconds, 150 Hz

#define CRSF_DIGITAL_CHANNEL_MIN 172
#define CRSF_DIGITAL_CHANNEL_MAX 1811

#define CRSF_PAYLOAD_OFFSET offsetof(crsfFrameDef_t, type)

#define CRSF_LINK_STATUS_UPDATE_TIMEOUT_US  250000 // 250ms, 4 Hz mode 1 telemetry

STATIC_UNIT_TESTED bool crsfFrameDone = false;
STATIC_UNIT_TESTED crsfFrame_t crsfFrame;
STATIC_UNIT_TESTED crsfFrame_t crsfChannelDataFrame;
STATIC_UNIT_TESTED uint32_t crsfChannelData[CRSF_MAX_CHANNEL];

static UART_HandleTypeDef *serialPort;
static timeUs_t crsfFrameStartAtUs = 0;
static uint8_t telemetryBuf[CRSF_FRAME_SIZE_MAX];
static uint8_t telemetryBufLen = 0;

static timeUs_t lastRcFrameTimeUs = 0;
crsfDebug_t crsfDebugs;

/*
 * CRSF protocol
 *
 * CRSF protocol uses a single wire half duplex uart connection.
 * The master sends one frame every 4ms and the slave replies between two frames from the master.
 *
 * 420000 baud
 * not inverted
 * 8 Bit
 * 1 Stop bit
 * Big endian
 * 420000 bit/s = 46667 byte/s (including stop bit) = 21.43us per byte
 * Max frame size is 64 bytes
 * A 64 byte frame plus 1 sync byte can be transmitted in 1393 microseconds.
 *
 * CRSF_TIME_NEEDED_PER_FRAME_US is set conservatively at 1500 microseconds
 *
 * Every frame has the structure:
 * <Device address><Frame length><Type><Payload><CRC>
 *
 * Device address: (uint8_t)
 * Frame length:   length in  bytes including Type (uint8_t)
 * Type:           (uint8_t)
 * CRC:            (uint8_t)
 *
 */

struct crsfPayloadRcChannelsPacked_s {
    // 176 bits of data (11 bits per channel * 16 channels) = 22 bytes.
    unsigned int chan0 : 11;
    unsigned int chan1 : 11;
    unsigned int chan2 : 11;
    unsigned int chan3 : 11;
    unsigned int chan4 : 11;
    unsigned int chan5 : 11;
    unsigned int chan6 : 11;
    unsigned int chan7 : 11;
    unsigned int chan8 : 11;
    unsigned int chan9 : 11;
    unsigned int chan10 : 11;
    unsigned int chan11 : 11;
    unsigned int chan12 : 11;
    unsigned int chan13 : 11;
    unsigned int chan14 : 11;
    unsigned int chan15 : 11;
} __attribute__ ((__packed__));

typedef struct crsfPayloadRcChannelsPacked_s crsfPayloadRcChannelsPacked_t;

#if defined(USE_CRSF_LINK_STATISTICS)
/*
 * 0x14 Link statistics
 * Payload:
 *
 * uint8_t Uplink RSSI Ant. 1 ( dBm * -1 )
 * uint8_t Uplink RSSI Ant. 2 ( dBm * -1 )
 * uint8_t Uplink Package success rate / Link quality ( % )
 * int8_t Uplink SNR ( db )
 * uint8_t Diversity active antenna ( enum ant. 1 = 0, ant. 2 )
 * uint8_t RF Mode ( enum 4fps = 0 , 50fps, 150hz)
 * uint8_t Uplink TX Power ( enum 0mW = 0, 10mW, 25 mW, 100 mW, 500 mW, 1000 mW, 2000mW )
 * uint8_t Downlink RSSI ( dBm * -1 )
 * uint8_t Downlink package success rate / Link quality ( % )
 * int8_t Downlink SNR ( db )
 * Uplink is the connection from the ground to the UAV and downlink the opposite direction.
 */

typedef struct crsfPayloadLinkstatistics_s {
    uint8_t uplink_RSSI_1;
    uint8_t uplink_RSSI_2;
    uint8_t uplink_Link_quality;
    int8_t uplink_SNR;
    uint8_t active_antenna;
    uint8_t rf_Mode;
    uint8_t uplink_TX_Power;
    uint8_t downlink_RSSI;
    uint8_t downlink_Link_quality;
    int8_t downlink_SNR;
} crsfLinkStatistics_t;

static timeUs_t lastLinkStatisticsFrameUs;


static void handleCrsfLinkStatisticsFrame(const crsfLinkStatistics_t* statsPtr, timeUs_t currentTimeUs)
{
    const crsfLinkStatistics_t stats = *statsPtr;
    lastLinkStatisticsFrameUs = currentTimeUs;
    int16_t rssiDbm = -1 * (stats.active_antenna ? stats.uplink_RSSI_2 : stats.uplink_RSSI_1);
    if (rssiSource == RSSI_SOURCE_RX_PROTOCOL_CRSF) {
        const uint16_t rssiPercentScaled = scaleRange(rssiDbm, CRSF_RSSI_MIN, 0, 0, RSSI_MAX_VALUE); //common/math.h
        setRssi(rssiPercentScaled, RSSI_SOURCE_RX_PROTOCOL_CRSF);
    }
#ifdef USE_RX_RSSI_DBM
    /*if (rxConfig()->crsf_use_rx_snr) {
        rssiDbm = stats.uplink_SNR;
    }*/
    setRssiDbm(rssiDbm, RSSI_SOURCE_RX_PROTOCOL_CRSF); 
#endif

#ifdef USE_RX_LINK_QUALITY_INFO
    if (linkQualitySource == LQ_SOURCE_RX_PROTOCOL_CRSF) {
        setLinkQualityDirect(stats.uplink_Link_quality);
        rxSetRfMode(stats.rf_Mode);
    }
#endif

}
#endif

//useless or maybe init
#if defined(USE_CRSF_LINK_STATISTICS)
static void crsfCheckRssi(uint32_t currentTimeUs) {

    if (cmpTimeUs(currentTimeUs, lastLinkStatisticsFrameUs) > CRSF_LINK_STATUS_UPDATE_TIMEOUT_US) {
        if (rssiSource == RSSI_SOURCE_RX_PROTOCOL_CRSF) {
            setRssiDirect(0, RSSI_SOURCE_RX_PROTOCOL_CRSF);
#ifdef USE_RX_RSSI_DBM
            /*if (rxConfig()->crsf_use_rx_snr) {
                setRssiDbmDirect(CRSF_SNR_MIN, RSSI_SOURCE_RX_PROTOCOL_CRSF);
            } else {*/
                setRssiDbmDirect(CRSF_RSSI_MIN, RSSI_SOURCE_RX_PROTOCOL_CRSF);
            //}
#endif
        }
#ifdef USE_RX_LINK_QUALITY_INFO
        if (linkQualitySource == LQ_SOURCE_RX_PROTOCOL_CRSF) {
            setLinkQualityDirect(0);
        }
#endif
    }
}
#endif

//crc so usefull
STATIC_UNIT_TESTED uint8_t crsfFrameCRC(void)
{
    // CRC includes type and payload
    uint8_t crc = crc8_dvb_s2(0, crsfFrame.frame.type);
    for (int ii = 0; ii < crsfFrame.frame.frameLength - CRSF_FRAME_LENGTH_TYPE_CRC; ++ii) {
        crc = crc8_dvb_s2(crc, crsfFrame.frame.payload[ii]);
    }
    return crc;
}


STATIC_UNIT_TESTED uint8_t crsfFrameStatus(void)
{

#if defined(USE_CRSF_LINK_STATISTICS)
    crsfCheckRssi(micros());
#endif
    if (crsfFrameDone) {
        crsfFrameDone = false;

        // unpack the RC channels
        const crsfPayloadRcChannelsPacked_t* const rcChannels = (crsfPayloadRcChannelsPacked_t*)&crsfChannelDataFrame.frame.payload;
        crsfChannelData[0] = rcChannels->chan0;
        crsfChannelData[1] = rcChannels->chan1;
        crsfChannelData[2] = rcChannels->chan2;
        crsfChannelData[3] = rcChannels->chan3;
        crsfChannelData[4] = rcChannels->chan4;
        crsfChannelData[5] = rcChannels->chan5;
        crsfChannelData[6] = rcChannels->chan6;
        crsfChannelData[7] = rcChannels->chan7;
        crsfChannelData[8] = rcChannels->chan8;
        crsfChannelData[9] = rcChannels->chan9;
        crsfChannelData[10] = rcChannels->chan10;
        crsfChannelData[11] = rcChannels->chan11;
        crsfChannelData[12] = rcChannels->chan12;
        crsfChannelData[13] = rcChannels->chan13;
        crsfChannelData[14] = rcChannels->chan14;
        crsfChannelData[15] = rcChannels->chan15;
        return RX_FRAME_COMPLETE;
    }
    return RX_FRAME_PENDING;
}

//usefull
uint16_t crsfReadRawRC(uint8_t chan)
{
    /* conversion from RC value to PWM
     *       RC     PWM
     * min  172 ->  988us
     * mid  992 -> 1500us
     * max 1811 -> 2012us
     * scale factor = (2012-988) / (1811-172) = 0.62477120195241
     * offset = 988 - 172 * 0.62477120195241 = 880.53935326418548
     */
    return (0.62477120195241f * crsfChannelData[chan]) + 881;
}


// Receive ISR callback, called back from serial port
void crsfRxCallback(uint8_t c)
{
    crsfDebugs.nbrCallback++;

    static uint8_t crsfFramePosition = 0;
    const timeUs_t currentTimeUs = microsISR(); // system uptime in uS

    if (cmpTimeUs(currentTimeUs, crsfFrameStartAtUs) > CRSF_TIME_NEEDED_PER_FRAME_US) {
        // We've received a character after max time needed to complete a frame,
        // so this must be the start of a new frame.
        crsfFramePosition = 0;
    }

    if (crsfFramePosition == 0) {
        crsfFrameStartAtUs = currentTimeUs;
    }
    // assume frame is 5 bytes long until we have received the frame length
    // full frame length includes the length of the address and framelength fields
    const int fullFrameLength = crsfFramePosition < 3 ? 5 : crsfFrame.frame.frameLength + CRSF_FRAME_LENGTH_ADDRESS + CRSF_FRAME_LENGTH_FRAMELENGTH;

    if (crsfFramePosition < fullFrameLength) {
        crsfFrame.bytes[crsfFramePosition++] = (uint8_t)c;
        if (crsfFramePosition >= fullFrameLength) {
            crsfDebugs.nbrCrc++;
            crsfFramePosition = 0;
            const uint8_t crc = crsfFrameCRC();
            if (crc == crsfFrame.bytes[fullFrameLength - 1]) {
                crsfDebugs.nbrSwitch++;
                switch (crsfFrame.frame.type)
                {
                    case CRSF_FRAMETYPE_RC_CHANNELS_PACKED:
                        if (crsfFrame.frame.deviceAddress == CRSF_ADDRESS_FLIGHT_CONTROLLER) {
                            lastRcFrameTimeUs = currentTimeUs;
                            crsfFrameDone = true;
                            memcpy(&crsfChannelDataFrame, &crsfFrame, sizeof(crsfFrame));
                            crsfFrameStatus(); //maybe add checks on it later
                            crsfDebugs.nbrChanFrameRec++;
                        }
                        break;
//usefull
#if defined(USE_CRSF_LINK_STATISTICS)

                    case CRSF_FRAMETYPE_LINK_STATISTICS: {
                         // if to FC and 10 bytes + CRSF_FRAME_ORIGIN_DEST_SIZE
                         if ((rssiSource == RSSI_SOURCE_RX_PROTOCOL_CRSF) &&
                             (crsfFrame.frame.deviceAddress == CRSF_ADDRESS_FLIGHT_CONTROLLER) &&
                             (crsfFrame.frame.frameLength == CRSF_FRAME_ORIGIN_DEST_SIZE + CRSF_FRAME_LINK_STATISTICS_PAYLOAD_SIZE)) {
                             const crsfLinkStatistics_t* statsFrame = (const crsfLinkStatistics_t*)&crsfFrame.frame.payload;
                             handleCrsfLinkStatisticsFrame(statsFrame, currentTimeUs);
                         }
                        break;
                        crsfDebugs.nbrLQFrameRec++;
                    }
#endif
                    default:
                        break;
                }
            }
        }
    }
}


void crsfRxWriteTelemetryData(const void *data, int len)
{
    len = MIN(len, (int)sizeof(telemetryBuf));
    memcpy(telemetryBuf, data, len);
    telemetryBufLen = len;
}

void crsfRxSendTelemetryData(void)
{
    // if there is telemetry data to write
    if (telemetryBufLen > 0) {
	HAL_UART_Transmit_DMA(serialPort, telemetryBuf, telemetryBufLen);
        //serialWriteBuf(serialPort, telemetryBuf, telemetryBufLen);
        telemetryBufLen = 0; // reset telemetry buffer
    }
}

timeUs_t crsfFrameTimeUs(void)
{
    return lastRcFrameTimeUs;
}


bool crsfRxInit(UART_HandleTypeDef *huart)
{

    for (int ii = 0; ii < CRSF_MAX_CHANNEL; ++ii) {
        crsfChannelData[ii] = (16 * /*rxConfig()->midrc*/992) / 10 - 1408;
    }
        if (rssiSource == RSSI_SOURCE_NONE) {
            rssiSource = RSSI_SOURCE_RX_PROTOCOL_CRSF;
        }
#ifdef USE_RX_LINK_QUALITY_INFO
        if (linkQualitySource == LQ_SOURCE_NONE) {
            linkQualitySource = LQ_SOURCE_RX_PROTOCOL_CRSF;
        }
#endif
   
    serialPort = huart;
    return serialPort != NULL;
}

bool crsfRxIsActive(void)
{
    return serialPort != NULL;
}
#endif
