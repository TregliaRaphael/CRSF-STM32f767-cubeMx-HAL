#include "pg/rx.h"
#include "common/maths.h"
#include "common/utils.h"

#include "crsf.h"
#include "rx.h"

#define DELAY_50_HZ (1000000 / 50)
#define DELAY_33_HZ (1000000 / 33)
#define DELAY_10_HZ (1000000 / 10)
#define DELAY_5_HZ (1000000 / 5)

#define RSSI_ADC_DIVISOR (4096 / 1024)
#define RSSI_OFFSET_SCALING (1024 / 100.0f)

#ifdef USE_RX_LINK_QUALITY_INFO
static uint16_t linkQuality = 0;
static uint8_t rfMode = 0;
#endif

static uint16_t rssi = 0;                   // range: [0;1023]
static int16_t rssiDbm = CRSF_RSSI_MIN;      // range: [-130,20]

rssiSource_e rssiSource;
rxRuntimeState_t rxRuntimeState;
linkQualitySource_e linkQualitySource;
const char rcChannelLetters[] = "AERT12345678abcdefgh";
int16_t rcData[MAX_SUPPORTED_RC_CHANNEL_COUNT];     // interval [1000;2000]


void rxInit(void)
{
  
}

void setRssiDirect(uint16_t newRssi, rssiSource_e source)
{
    if (source != rssiSource) {
        return;
    }

    rssi = newRssi;
}

#define RSSI_SAMPLE_COUNT 16

static uint16_t updateRssiSamples(uint16_t value)
{
    static uint16_t samples[RSSI_SAMPLE_COUNT];
    static uint8_t sampleIndex = 0;
    static unsigned sum = 0;

    sum += value - samples[sampleIndex];
    samples[sampleIndex] = value;
    sampleIndex = (sampleIndex + 1) % RSSI_SAMPLE_COUNT;
    return sum / RSSI_SAMPLE_COUNT;
}


void setRssi(uint16_t rssiValue, rssiSource_e source)
{
    if (source != rssiSource) {
        return;
    }

    // Filter RSSI value
    /*if (source == RSSI_SOURCE_FRAME_ERRORS) {
        rssi = pt1FilterApply(&frameErrFilter, rssiValue);
    } else {*/
        // calculate new sample mean
        rssi = updateRssiSamples(rssiValue);
    //}
}


uint16_t getRssi(void)
{
    uint16_t rssiValue = rssi;

    // RSSI_Invert option
    if (rxConfig()->rssi_invert) {
        rssiValue = RSSI_MAX_VALUE - rssiValue;
    }

    return rxConfig()->rssi_scale / 100.0f * rssiValue + rxConfig()->rssi_offset * RSSI_OFFSET_SCALING;
}

uint8_t getRssiPercent(void)
{
    return scaleRange(getRssi(), 0, RSSI_MAX_VALUE, 0, 100);
}

bool isRssiConfigured(void)
{
    return rssiSource != RSSI_SOURCE_NONE;
}

/*TODO 
#ifdef USE_RX_LINK_QUALITY_INFO
#define LINK_QUALITY_SAMPLE_COUNT 16

STATIC_UNIT_TESTED uint16_t updateLinkQualitySamples(uint16_t value)
{
    static uint16_t samples[LINK_QUALITY_SAMPLE_COUNT];
    static uint8_t sampleIndex = 0;
    static uint16_t sum = 0;

    sum += value - samples[sampleIndex];
    samples[sampleIndex] = value;
    sampleIndex = (sampleIndex + 1) % LINK_QUALITY_SAMPLE_COUNT;
    return sum / LINK_QUALITY_SAMPLE_COUNT;
}

void rxSetRfMode(uint8_t rfModeValue)
{
    rfMode = rfModeValue;
}
#endif

static void setLinkQuality(bool validFrame, timeDelta_t currentDeltaTimeUs)
{
    static uint16_t rssiSum = 0;
    static uint16_t rssiCount = 0;
    static timeDelta_t resampleTimeUs = 0;

#ifdef USE_RX_LINK_QUALITY_INFO
    if (linkQualitySource != LQ_SOURCE_RX_PROTOCOL_CRSF) {
        // calculate new sample mean
        linkQuality = updateLinkQualitySamples(validFrame ? LINK_QUALITY_MAX_VALUE : 0);
    }
#endif

    if (rssiSource == RSSI_SOURCE_FRAME_ERRORS) {
        resampleTimeUs += currentDeltaTimeUs;
        rssiSum += validFrame ? RSSI_MAX_VALUE : 0;
        rssiCount++;

        if (resampleTimeUs >= FRAME_ERR_RESAMPLE_US) {
            setRssi(rssiSum / rssiCount, rssiSource);
            rssiSum = 0;
            rssiCount = 0;
            resampleTimeUs -= FRAME_ERR_RESAMPLE_US;
        }
    }
}*/


void setLinkQualityDirect(uint16_t linkqualityValue)
{
#ifdef USE_RX_LINK_QUALITY_INFO
    linkQuality = linkqualityValue;
#else
    UNUSED(linkqualityValue);
#endif
}


#ifdef USE_RX_LINK_QUALITY_INFO
uint16_t rxGetLinkQuality(void)
{
    return linkQuality;
}

uint8_t rxGetRfMode(void)
{
    return rfMode;
}

uint16_t rxGetLinkQualityPercent(void)
{
    return (linkQualitySource == LQ_SOURCE_RX_PROTOCOL_CRSF) ?  linkQuality : scaleRange(linkQuality, 0, LINK_QUALITY_MAX_VALUE, 0, 100);
}
#endif



//RSSI_DBM
int16_t getRssiDbm(void)
{
    return rssiDbm;
}

#define RSSI_SAMPLE_COUNT_DBM 16

static int16_t updateRssiDbmSamples(int16_t value)
{
    static int16_t samplesdbm[RSSI_SAMPLE_COUNT_DBM];
    static uint8_t sampledbmIndex = 0;
    static int sumdbm = 0;

    sumdbm += value - samplesdbm[sampledbmIndex];
    samplesdbm[sampledbmIndex] = value;
    sampledbmIndex = (sampledbmIndex + 1) % RSSI_SAMPLE_COUNT_DBM;
    return sumdbm / RSSI_SAMPLE_COUNT_DBM;
}

void setRssiDbm(int16_t rssiDbmValue, rssiSource_e source)
{
    if (source != rssiSource) {
        return;
    }

    rssiDbm = updateRssiDbmSamples(rssiDbmValue);
}

void setRssiDbmDirect(int16_t newRssiDbm, rssiSource_e source)
{
    if (source != rssiSource) {
        return;
    }

    rssiDbm = newRssiDbm;
}
