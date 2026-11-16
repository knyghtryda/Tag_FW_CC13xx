/*
 * OEPL main loop for the Chroma 74H (CC1310F64), built with -DOEPL_74H
 * (makefile CONFIG=Chroma74H). Follows ChromaAeon74/main.c.
 */
#include "board.h"

#ifdef OEPL_74H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <NoRTOS.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/aon_batmon.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include <ti/drivers/Watchdog.h>
#include <ti/drivers/dpl/HwiP.h>
#include DeviceFamily_constructPath(inc/hw_types.h)
#include DeviceFamily_constructPath(inc/hw_ints.h)

#include "Board.h"
#include "main.h"
#include "radio.h"
#include "syncedproto.h"
#include "oepl-proto.h"
#include "oepl-definitions.h"
#include "powermgt.h"
#include "eeprom.h"
#include "epd_74h.h"
#include "drawing.h"
#include "logging.h"

#define NVRAM_SECTOR_SIZE  0x1000
#define BATMON_TIMEOUT_MS  100

// Longest single sleep: the watchdog is reloaded before each chunk, so a
// sleep that doesn't reach standby (the counter only pauses there) can't
// trip it. The watchdog period limit is 2863 s.
#define SLEEP_CHUNK_MS     (20 * 60 * 1000UL)
#define SLEEP_WDT_MARGIN_MS 60000UL

// Set by the watchdog NMI before it resets the chip, read on the next boot.
// Not zeroed by the C startup; the CC1310 keeps SRAM through a system reset.
#define WDT_RESET_MAGIC    0x57445452UL   // "WDTR"
#pragma NOINIT(gWdtResetMagic)
static uint32_t gWdtResetMagic;

// HardFault record, kept through the reset for a JTAG read (find gFault in
// the .map). Stack[] starts at the handler's frame; the exception frame
// (R0-R3, R12, LR, PC, xPSR) is a few words up.
#define FAULT_MAGIC        0x464c5421UL   // "FLT!"
#pragma NOINIT(gFault)
struct {
   uint32_t Magic;
   uint32_t Cfsr;
   uint32_t Hfsr;
   uint32_t Bfar;
   uint32_t Mmfar;
   uint32_t Sp;
   uint32_t Stack[26];
} gFault;
static Watchdog_Handle gWdt;

uint8_t wakeUpReason = WAKEUP_REASON_FIRSTBOOT;
int8_t  temperature;
uint16_t  batteryVoltage;
bool  lowBattery;
bool noApShown;
uint8_t capabilities;
uint8_t gTempBuf[TEMP_BUF_SIZE];

static void InitSN(void);
static uint8_t channelSelect(uint8_t rounds);
static void GetTempAndBattV(void);
static void WdtInit(void);
static void SleepMs(uint32_t Ms);
static void ShowInfoScreen(const char *pStatus);
static void ShowNoAp(uint8_t SpecialType);
static void ShowApFound(void);

// AvailDataInfo.nextCheckIn: minutes, or seconds in the low 15 bits when
// bit 15 is set
static uint32_t ApSleepMs(uint16_t NextCheckIn)
{
   if(NextCheckIn & 0x8000) {
      return (NextCheckIn & 0x7fff) * 1000UL;
   }
   return NextCheckIn * 60000UL;
}

int main(void)
{
   bool bSpecialShown = false;   // a preloaded AP lost / no AP screen is up

   Board_initGeneral();
   NoRTOS_start();
   InitLogging();
   WdtInit();
#ifdef WDT_TEST
// Hang once: the watchdog should reset us within 2 s, and the next boot
// checks in with WAKEUP_REASON_WDT_RESET (the AP logs it)
   if(wakeUpReason != WAKEUP_REASON_WDT_RESET) {
      ALOG("WDT_TEST: hanging\n");
      wdtSet(2000);
      while(true);
   }
#endif
   ALOG(BOARD_NAME " OEPL Ver %d compiled " __DATE__ " " __TIME__ "\n",FW_VERSION);

   InitSN();
   GetTempAndBattV();
   if(radioInit()) {
      ELOG("radioInit failed\n");
   }

   initializeProto();
#ifdef SCREEN_TEST
// The built-in screens, one after the other (each draw takes ~1 minute)
   ShowInfoScreen("Screen test");
   drawOnOffline(0);
   drawCurrentImage();
   drawOnOffline(1);
#endif
   currentChannel = channelSelect(1);

   if(currentChannel) {
      ALOG("AP Found\n");
      if(curImgSlot == 0xFF) {
      // Nothing stored to show yet (first boot): say who we are
         ShowApFound();
      }
      SleepMs(5000UL);
   }
   else {
      ALOG("No AP found\n");
      noApShown = true;
      ShowNoAp(CUSTOM_IMAGE_NOAPFOUND);
      bSpecialShown = true;
      SleepMs(120000UL);
   }

   while(1) {
      wdt10s();
      GetTempAndBattV();
      if(currentChannel) {
         struct AvailDataInfo *avail;
         avail = getAvailDataInfo();
         addAverageValue();

         if(avail == NULL) {
            ALOG("No data\n");
            nextCheckInFromAP = 0;
         }
         else {
            nextCheckInFromAP = avail->nextCheckIn;
            if(avail->dataType != DATATYPE_NOUPDATE) {
               ALOG("Got data transfer\n");
               if(!processAvailDataInfo(avail)) {
                  nextCheckInFromAP = 0;
               }
            }
            else {
               LOG("No data transfer\n");
            }
         // The AP has heard the boot / watchdog reason now. Still sent with
         // the next check-in, it made the AP log "Booting" again, clear the
         // image hash and re-render the content right after a transfer.
            wakeUpReason = WAKEUP_REASON_TIMED;
         }

         uint16_t nextCheckin = getNextSleep();
         longDataReqCounter += nextCheckin;
         if(nextCheckin == INTERVAL_AT_MAX_ATTEMPTS) {
            currentChannel = 0;
         }
      // Sleep what the AP said: it models our sleep from what it offered
      // and holds new content until our expected check-in is near.
         if(nextCheckInFromAP) {
            SleepMs(ApSleepMs(nextCheckInFromAP));
         }
         else {
            SleepMs(nextCheckin * 1000UL);
         }
      }
      else {
         wdt60s();   // a scan of all 12 channels takes ~6 s
         currentChannel = channelSelect(1);
         if(!currentChannel) {
            if(!noApShown) {
               noApShown = true;
               ALOG("AP lost\n");
               ShowNoAp(CUSTOM_IMAGE_LOST_CONNECTION);
               bSpecialShown = true;
            }
            SleepMs(15 * 60 * 1000UL);
         }
         else {
            noApShown = false;
            ALOG("AP Found\n");
            if(bSpecialShown) {
            // back from the AP lost / no AP screen
               ShowApFound();
               bSpecialShown = false;
            }
            scanAttempts = 0;
            wakeUpReason = WAKEUP_REASON_NETWORK_SCAN;
            initPowerSaving(INTERVAL_BASE);
            SleepMs(40 * 1000UL);
         }
      }
   }
}

// Read the live copy of a factory NVRAM record (<type> <len incl. 2 byte
// header> <data>, type 0 = invalidated, 0xff = end). The stock firmware
// keeps the NVRAM in sector 0 or 1. Returns true if not found.
static bool NvramRead(uint8_t Type,uint8_t *pDst,uint8_t Len)
{
   static const uint8_t Magic[4] = {0x56,0x12,0x09,0x85};
   uint8_t Hdr[4];
   uint32_t Sector;
   uint32_t Offset;
   bool bFound = false;

   for(Sector = 0; Sector < 2 * NVRAM_SECTOR_SIZE; Sector += NVRAM_SECTOR_SIZE) {
      if(eepromRead(Sector,Hdr,sizeof(Hdr)) || memcmp(Hdr,Magic,sizeof(Magic))) {
         continue;
      }
      for(Offset = sizeof(Magic); Offset + 2 <= NVRAM_SECTOR_SIZE; Offset += Hdr[1]) {
         if(eepromRead(Sector + Offset,Hdr,2) || Hdr[0] == 0xff) {
            break;
         }
         if(Hdr[1] < 2) {
            ELOG("zero length NVRAM record @ 0x%x\n",Sector + Offset);
            break;
         }
         if(Hdr[0] == Type && Hdr[1] - 2 == Len
            && !eepromRead(Sector + Offset + 2,pDst,Len))
         {
            bFound = true;
         }
      }
   }
   return !bFound;
}

static void InitSN()
{
   uint8_t SN[SN_LEN];

   if(NvramRead(SN_NVRAM_RECORD,SN,sizeof(SN))
      || SN[0] != SN_CHAR1 || SN[1] != SN_CHAR2 || SN[6] != SN_REV_CHAR)
   {
      ELOG("No valid SN in the factory NVRAM, using a default\n");
      memset(SN,0,sizeof(SN));
      SN[0] = SN_CHAR1;
      SN[1] = SN_CHAR2;
      SN[6] = SN_REV_CHAR;
   }
   mSelfMac[7] = 0x44;
   mSelfMac[6] = 0x67;
   mSelfMac[3] = SN[2];
   mSelfMac[2] = SN[3];
   mSelfMac[1] = SN[4];
   mSelfMac[0] = SN[5];
// The first 2 characters are upper case ASCII, store them as 5 bit values
// (see ChromaAeon74/main.c).
   mSelfMac[5] = SN[0] - 'A';
   mSelfMac[4] = SN[1] - 'A';

   ALOG("SN: %c%c%02x%02x%02x%02x%c\n",
        SN[0],SN[1],SN[2],SN[3],SN[4],SN[5],SN[6]);
}

// Return 0 if no APs are found
static uint8_t channelSelect(uint8_t rounds)
{
   const uint8_t ChannelList[] = {
      200,100,201,101,202,102,203,103,204,104,205,105
   };
   int8_t BestRssi = -128;
   uint8_t BestChannel = 0;
   uint8_t i;

   while(rounds-- > 0) {
      for(i = 0; i < sizeof(ChannelList); i++) {
         if(gSubGhzBand == BAND_915 && ChannelList[i] < 200) {
            continue;
         }
         if(gSubGhzBand == BAND_868 && ChannelList[i] >= 200) {
            continue;
         }
         if(radioSetChannel(ChannelList[i])) {
            break;
         }
         if(detectAP(ChannelList[i])) {
            ALOG("Chan %d RSSI %d\n",ChannelList[i],mLastRSSI);
            if(BestRssi < mLastRSSI) {
               BestRssi = mLastRSSI;
               BestChannel = ChannelList[i];
            }
         }
      }
   }

   if(BestChannel) {
      ALOG("Using ch %d\n",BestChannel);
      gSubGhzBand = BestChannel >= 200 ? BAND_915 : BAND_868;
      gChannel = BestChannel;
   }
   return BestChannel;
}

// DATATYPE_COMMAND_DATA from the AP (XferComplete already sent)
void executeCommand(uint8_t Cmd)
{
   switch(Cmd) {
      case CMD_DO_REBOOT:
         ALOG("CMD reboot\n");
         SysCtrlSystemReset();
         break;

      case CMD_DO_SCAN:
         ALOG("CMD scan\n");
         currentChannel = 0;
         break;

      case CMD_DO_DEEPSLEEP:
      // Until the batteries come out: no wake source is configured
         ALOG("CMD deep sleep\n");
         while(true) {
            SleepMs(SLEEP_CHUNK_MS);
         }

      case CMD_ERASE_EEPROM_IMAGES:
         ALOG("CMD erase images\n");
         eraseImageSlots();
         break;

      case CMD_DO_RESET_SETTINGS:   // no settings yet
      default:
         ALOG("CMD 0x%x ignored\n",Cmd);
         break;
   }
}

// Built-in screens, drawn as EPD layers (no frame buffer)

// "NO AP" badge over the current image, bottom left
static const EpdRect gNoApRects[] = {
   {0,EPD_HEIGHT - 68,232,68,EPD_INK_BLACK}
};
static const EpdText gNoApTexts[] = {
   {16,EPD_HEIGHT - 54,5,EPD_INK_YELLOW,"NO AP"}
};
static const EpdLayer gNoApLayer = {gNoApRects,1,gNoApTexts,1};

// Called by syncedproto before it draws an image: 0 = draw it with the
// "NO AP" badge, 1 = plain
void drawOnOffline(uint8_t state)
{
   EpdSetLayer(state ? NULL : &gNoApLayer);
}

// Information screen for a tag with no image to show
static void ShowInfoScreen(const char *pStatus)
{
   static const EpdRect Rects[] = {
      {0,0,EPD_WIDTH,100,EPD_INK_YELLOW},
      {0,EPD_HEIGHT - 16,EPD_WIDTH,16,EPD_INK_YELLOW}
   };
   char Mac[24];
   char Info[40];
   EpdText Texts[] = {
      {64,26,6,EPD_INK_BLACK,"OpenEPaperLink"},
      {40,140,4,EPD_INK_BLACK,BOARD_NAME},
      {40,220,3,EPD_INK_BLACK,Mac},
      {40,268,3,EPD_INK_BLACK,Info},
      {40,360,4,EPD_INK_BLACK,pStatus}
   };
   EpdLayer Layer = {Rects,sizeof(Rects) / sizeof(Rects[0]),
                     Texts,sizeof(Texts) / sizeof(Texts[0])};

   snprintf(Mac,sizeof(Mac),"MAC %02X%02X%02X%02X%02X%02X%02X%02X",
            mSelfMac[7],mSelfMac[6],mSelfMac[5],mSelfMac[4],
            mSelfMac[3],mSelfMac[2],mSelfMac[1],mSelfMac[0]);
   snprintf(Info,sizeof(Info),"FW %04X  %u mV  %d C",FW_VERSION,
            batteryVoltage,temperature);
   ALOG("info screen: %s\n",pStatus);
   EpdSetLayer(&Layer);
   wdt60s();
   EpdDrawFromFlash(EPD_NO_IMAGE,0);
   EpdSetLayer(NULL);
}

// AP lost / not found: the screen the AP preloaded for it if there is one,
// else the current image with the "NO AP" badge, else the info screen
static void ShowNoAp(uint8_t SpecialType)
{
   if(drawSpecialImage(SpecialType)) {
      return;
   }
   drawOnOffline(0);
   if(!drawCurrentImage()) {
      ShowInfoScreen("No AP found");
   }
   drawOnOffline(1);
}

// AP (re)found: the current image without the badge, else the info screen
static void ShowApFound(void)
{
   char Status[32];

   drawOnOffline(1);
   if(!drawCurrentImage()) {
      snprintf(Status,sizeof(Status),"AP found, channel %d",currentChannel);
      ShowInfoScreen(Status);
   }
}

// Watchdog NMI (first timeout): reset now instead of on the second timeout,
// so a period means what it says
static void WdtNmi(uintptr_t Arg)
{
   gWdtResetMagic = WDT_RESET_MAGIC;
   SysCtrlSystemReset();
}

// HardFault: record it, then reset. Without this the default handler spins
// until the watchdog fires; after a JTAG session the debugger's vector catch
// halts the core instead, and nothing resets it.
static void FaultHandler(uintptr_t Arg)
{
   volatile uint32_t Here = 0;
   uint32_t *pStack = (uint32_t *) &Here;
   int i;

   gFault.Cfsr = HWREG(0xE000ED28);
   gFault.Hfsr = HWREG(0xE000ED2C);
   gFault.Mmfar = HWREG(0xE000ED34);
   gFault.Bfar = HWREG(0xE000ED38);
   gFault.Sp = (uint32_t) pStack;
   for(i = 0; i < sizeof(gFault.Stack) / sizeof(gFault.Stack[0]); i++) {
      if((uint32_t) &pStack[i] >= 0x20004000) {
         break;
      }
      gFault.Stack[i] = pStack[i];
   }
   gFault.Magic = FAULT_MAGIC;
   SysCtrlSystemReset();
}

static void WdtInit(void)
{
   Watchdog_Params Params;

   if(gWdtResetMagic == WDT_RESET_MAGIC) {
      wakeUpReason = WAKEUP_REASON_WDT_RESET;
      ELOG("Watchdog reset\n");
   }
   gWdtResetMagic = 0;
   if(gFault.Magic == FAULT_MAGIC) {
   // No wake reason for a crash: report it as a watchdog reset. The record
   // stays in RAM for a JTAG read until the next fault.
      wakeUpReason = WAKEUP_REASON_WDT_RESET;
      ELOG("HardFault: CFSR 0x%x HFSR 0x%x BFAR 0x%x SP 0x%x\n",
           gFault.Cfsr,gFault.Hfsr,gFault.Bfar,gFault.Sp);
      gFault.Magic = 0;
   }
   HwiP_plug(INT_HARD_FAULT,(void *) FaultHandler);

   Watchdog_init();
   Watchdog_Params_init(&Params);
   Params.callbackFxn = WdtNmi;
   Params.resetMode = Watchdog_RESET_ON;
   Params.debugStallMode = Watchdog_DEBUG_STALL_ON;  // JTAG halts don't trip it
   gWdt = Watchdog_open(Board_WATCHDOG0,&Params);
   if(gWdt == NULL) {
      ELOG("Watchdog_open failed\n");
   }
   wdt60s();
}

// Restart the watchdog with a period of Ms (max 2863 s)
void wdtSet(uint32_t Ms)
{
   if(gWdt != NULL) {
      Watchdog_setReload(gWdt,Watchdog_convertMsToTicks(gWdt,Ms));
      Watchdog_clear(gWdt);
   }
}

void wdt10s(void)
{
   wdtSet(10000);
}

void wdt60s(void)
{
   wdtSet(60000);
}

static void SleepMs(uint32_t Ms)
{
   uint32_t Now;

   while(Ms > 0) {
      Now = Ms > SLEEP_CHUNK_MS ? SLEEP_CHUNK_MS : Ms;
      wdtSet(Now + SLEEP_WDT_MARGIN_MS);
      doSleep(Now);
      Ms -= Now;
   }
}

static void GetTempAndBattV(void)
{
   uint32_t Waited;
   uint32_t Temp;

   AONBatMonEnable();
   for(Waited = 0; !AONBatMonNewTempMeasureReady() && Waited < BATMON_TIMEOUT_MS; Waited++) {
      WaitMs(1);
   }
   temperature = (int8_t) AONBatMonTemperatureGetDegC();
   for(Waited = 0; !AONBatMonNewBatteryMeasureReady() && Waited < BATMON_TIMEOUT_MS; Waited++) {
      WaitMs(1);
   }
   Temp = AONBatMonBatteryVoltageGet();
   batteryVoltage = (Temp & 0xff) * 1000 / 256;
   batteryVoltage += (Temp >> 8) * 1000;
   AONBatMonDisable();
   LOG("Temp %d C, BatV %d mV\n",temperature,batteryVoltage);
}
#endif   // OEPL_74H
