#ifndef _MAIN_H_
#define _MAIN_H_

// Chroma 74H OEPL build (counterpart of ChromaAeon74/main.h)
#include <stdbool.h>
#include <stdint.h>
#include "cc13xx.h"

// Cap every sleep while bringing the port up, so the tag checks in often
#define DEBUG_MAX_SLEEP    5000

// Reported to the AP. The AP sends zlib compressed images to a tag whose
// version reaches the tagtype's "zlib_compression" value; stay below it
// until zlib decoding fits in RAM. The first version the AP shows is 2.
#define FW_VERSION         0x0002

#define TEMP_BUF_SIZE      256
extern uint8_t gTempBuf[TEMP_BUF_SIZE];
extern uint8_t mSelfMac[8];

void wdt10s(void);
void wdt60s(void);

extern uint8_t wakeUpReason;
extern int8_t  temperature;
extern uint16_t  batteryVoltage;
extern bool  lowBattery;
extern uint8_t capabilities;

// epd_74h.c
void drawImageAtAddress(uint32_t addr, uint8_t lut);

#endif   // _MAIN_H_
