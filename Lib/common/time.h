#pragma once

#include <stdbool.h>
#include <stdint.h>

//#include "platform.h"

#include "pg/pg.h"

// time difference, 32 bits always sufficient
typedef int32_t timeDelta_t;
// millisecond time
typedef uint32_t timeMs_t ;
// microsecond time
#ifdef USE_64BIT_TIME
typedef uint64_t timeUs_t;
#define TIMEUS_MAX UINT64_MAX
#else
typedef uint32_t timeUs_t;
#define TIMEUS_MAX UINT32_MAX
#endif

#define TIMEZONE_OFFSET_MINUTES_MIN -780  // -13 hours
#define TIMEZONE_OFFSET_MINUTES_MAX 780   // +13 hours

static inline timeDelta_t cmpTimeUs(timeUs_t a, timeUs_t b) { return (timeDelta_t)(a - b); }

#define FORMATTED_DATE_TIME_BUFSIZE 30

timeUs_t microsISR(void);

#ifdef USE_RTC_TIME

typedef struct timeConfig_s {
    int16_t tz_offsetMinutes; // Offset from UTC in minutes, might be positive or negative
} timeConfig_t;

PG_DECLARE(timeConfig_t, timeConfig);

// Milliseconds since Jan 1 1970
typedef int64_t rtcTime_t;

/*rtcTime_t rtcTimeMake(int32_t secs, uint16_t millis);
int32_t rtcTimeGetSeconds(rtcTime_t *t);
uint16_t rtcTimeGetMillis(rtcTime_t *t);*/

typedef struct _dateTime_s {
    // full year
    uint16_t year;
    // 1-12
    uint8_t month;
    // 1-31
    uint8_t day;
    // 0-23
    uint8_t hours;
    // 0-59
    uint8_t minutes;
    // 0-59
    uint8_t seconds;
    // 0-999
    uint16_t millis;
} dateTime_t;


#endif
