#ifndef _MAIN_H_
#define _MAIN_H_

// Chroma 74H OEPL build (counterpart of ChromaAeon74/main.h)
#include <stdbool.h>
#include <stdint.h>
#include "cc13xx.h"

// No DEBUG_MAX_SLEEP (powermgt.c caps every sleep with it): the AP holds new
// content until the check-in it expects from the sleep it offered, so a tag
// that wakes up early gains nothing. For fast turnaround use the AP's
// "Shorten latency during config" with its web UI open.

// Reported to the AP, which picks the image format from it: G5 once the
// version reaches the tagtype's "g5_compression" value (hex), zlib once it
// reaches "zlib_compression". The 74H tagtype (87.json) has g5_compression
// "3" and no zlib (doesn't fit in RAM yet). 2 = raw images only, 3 = G5.
#define FW_VERSION         0x0003

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
