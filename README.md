# TBS Crossfire  (CRSF) with cubeMx

TODO: Makefile need to handle multiple file with same name on different directory

For now, i'm able to send telemetry like GPS/Current/attitude ... and receive all needed from CRSF protocol to play.
It receives channels and linkquality.

I tested it on USART6 with 420k baud rate and circular mode DMA on rx.
USART clock should be fast like at least 90Mhz or it won't receive all frames
For tx telemetry, it should stay stock and put in a scheduler. It has to be sent at a very strict time or it just wont works (lost 2hours on it already)
If you used another board than f7, you should just change few includes from f7 HAL to yours like H7 or F4 for exemple

#### To add in makefile:
```
-DUSE_CRSF_LINK_STATISTICS \
-DUSE_RX_RSSI_DBM \
-DUSE_RX_LINK_QUALITY_INFO \
-DUSE_SERIALRX_CRSF \
-DUSE_TELEMETRY_CRSF \
-DUSE_GPS \
```

#### Includes:
```
#include "common/time.h"
#include "drivers/system.h"

#include "sensors/gps.h"
#include "flight/imu.h"
#include "rx/rx.h"
#include "rx/crsf.h"
#include "telemetry/crsf_telemetry.h"

```

#### Basic main init:
To use it, you must add` __weak` in front of Systick_Handler declared by cube mx in `Core/Inc/stm32f7xx_hal_it.h`

```
/* USER CODE BEGIN 2 */
  cycleCounterInit();       //init of register clock uS register, mandatory

  gpsInit();
  imuInit();
  crsfRxInit(&huart6);
  HAL_UART_Receive_DMA(&huart6, &data, 1);
  initCrsfTelemetry();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  timeUs_t last, now, lastTel;
  int cpt = 0;
  while (1)
  {
    now = microsISR();
    if (now - lastTel > 10000){
      handleCrsfTelemetry(microsISR());   //sending telemetry 10HZ, dont need refresh it faster
      lastTel = now;
    }
    if (now - last > 10000){              //printing data every 10MS but you can reduce it to make it less spammable
        printf("[%d] Chan[0]: %d, Chan[1]: %d, Chan[2]: %d, Chan[3]: %d\r\n", cpt++, crsfReadRawRC(0), crsfReadRawRC(1),crsfReadRawRC(2),crsfReadRawRC(3));
        //printf("Rssi: %d\r\n", getRssi());
        //printf("cb: %d, callback: %d, crc: %d, switch: %d, Chan: %d, LQ: %d\r\n",cbcpt, crsfDebugs.nbrCallback, crsfDebugs.nbrCrc, crsfDebugs.nbrSwitch, crsfDebugs.nbrChanFrameRec, crsfDebugs.nbrLQFrameRec);
        last = microsISR();
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

```

#### DMA callback:
```
  void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
  {
      cbcpt++;

      if (huart->Instance == USART6){
          crsfRxCallback(data);
          HAL_UART_Receive_DMA(&huart6, &data, 1);
      }
  }
```

it is all about a readapted code from betaflight software.
Just a long time clearing and understanding.
https://github.com/betaflight/betaflight
