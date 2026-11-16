#ifndef _EPD_74H_H_
#define _EPD_74H_H_

#include <stdbool.h>
#include <stdint.h>

#define EPD_WIDTH       800
#define EPD_HEIGHT      480
#define EPD_PLANE_SIZE  (EPD_WIDTH * EPD_HEIGHT / 8)

// Draw an image stored in SPI flash at PlaneAdr: the B/W plane followed by
// the yellow plane, EPD_PLANE_SIZE bytes each (the stock screen slot layout
// after its 0x20 byte header). Runs the panel's factory command script.
// Returns true on error.
bool EpdDrawFromFlash(uint32_t PlaneAdr);

#ifdef EPD_TEST
void EpdTest(void);
#endif

#endif   // _EPD_74H_H_
