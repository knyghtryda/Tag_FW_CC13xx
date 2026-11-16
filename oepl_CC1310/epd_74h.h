#ifndef _EPD_74H_H_
#define _EPD_74H_H_

#include <stdbool.h>
#include <stdint.h>

#define EPD_WIDTH       800
#define EPD_HEIGHT      480
#define EPD_PLANE_SIZE  (EPD_WIDTH * EPD_HEIGHT / 8)

// EpdDrawFromFlash() flags
#define EPD_INVERT_BW   0x01  // plane 1 is 1 = black (OEPL), panel wants 1 = white
#define EPD_INVERT_Y    0x02  // plane 2 is 1 = yellow (OEPL), panel wants 0 = yellow
#define EPD_NO_YELLOW   0x04  // B/W only image: send an empty yellow plane

// Draw an image stored in SPI flash at PlaneAdr: the B/W plane followed by
// the yellow plane, EPD_PLANE_SIZE bytes each. With Flags = 0 the bytes go
// to the panel as stored (the stock screen slot layout). Runs the panel's
// factory command script. Returns true on error.
bool EpdDrawFromFlash(uint32_t PlaneAdr,uint8_t Flags);

#ifdef EPD_TEST
void EpdTest(void);
#endif

#endif   // _EPD_74H_H_
