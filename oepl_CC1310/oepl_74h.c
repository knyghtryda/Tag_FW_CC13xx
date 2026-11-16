/*
 * OEPL main loop for the Chroma 74H (CC1310F64), built with -DOEPL_74H
 * (makefile CONFIG=Chroma74H). Follows ChromaAeon74/main.c.
 */
#include "board.h"

#ifdef OEPL_74H
#include <stdint.h>
#include <string.h>
#include <NoRTOS.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/aon_batmon.h)

#include "Board.h"
#include "main.h"
#include "radio.h"
#include "syncedproto.h"
#include "oepl-proto.h"
#include "oepl-definitions.h"
#include "powermgt.h"
#include "eeprom.h"
#include "logging.h"

#define NVRAM_SECTOR_SIZE  0x1000
#define BATMON_TIMEOUT_MS  100

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
   Board_initGeneral();
   NoRTOS_start();
   InitLogging();
   ALOG(BOARD_NAME " OEPL Ver %d compiled " __DATE__ " " __TIME__ "\n",FW_VERSION);

   InitSN();
   GetTempAndBattV();
   if(radioInit()) {
      ELOG("radioInit failed\n");
   }

   initializeProto();
   currentChannel = channelSelect(1);

   if(currentChannel) {
      ALOG("AP Found\n");
      doSleep(5000UL);
   }
   else {
      ALOG("No AP found\n");
      noApShown = true;
      doSleep(120000UL);
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
               wakeUpReason = WAKEUP_REASON_TIMED;
            }
         }

         uint16_t nextCheckin = getNextSleep();
         longDataReqCounter += nextCheckin;
         if(nextCheckin == INTERVAL_AT_MAX_ATTEMPTS) {
            currentChannel = 0;
         }
      // Sleep what the AP said: it models our sleep from what it offered
      // and holds new content until our expected check-in is near.
         if(nextCheckInFromAP) {
            doSleep(ApSleepMs(nextCheckInFromAP));
         }
         else {
            doSleep(nextCheckin * 1000UL);
         }
      }
      else {
         currentChannel = channelSelect(1);
         if(!currentChannel) {
            wdt60s();
            noApShown = true;
            doSleep(15 * 60 * 1000UL);
         }
         else {
            noApShown = false;
            ALOG("AP Found\n");
            scanAttempts = 0;
            wakeUpReason = WAKEUP_REASON_NETWORK_SCAN;
            initPowerSaving(INTERVAL_BASE);
            doSleep(40 * 1000UL);
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

// AP lost / found indication: not implemented yet (README UI to-do)
void drawOnOffline(uint8_t state)
{
   LOG("State %d\n",state);
}

void wdt10s(void)
{
}

void wdt60s(void)
{
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
