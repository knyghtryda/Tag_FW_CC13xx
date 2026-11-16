#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef  CHROMA_AEON_74
#define BOARD_NAME                  "ChromaAeon74"
#define HW_TYPE                     0x81
#define FLASH_MX25V8006

#define EEPROM_SETTINGS_AREA_START  0x0
#define EEPROM_SETTINGS_AREA_LEN    EEPROM_ERZ_SECTOR_SZ

#define EEPROM_OTA_START            (0x94000)
#define EEPROM_OTA_LEN              (352*1024)

#define EEPROM_IMG_START            (0x10000)
#define EEPROM_IMG_EACH             (0x20000)
#define EEPROM_IMG_SECTORS          (EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ)
#define IMAGE_SLOTS                 ((EEPROM_SIZE - EEPROM_IMG_START)/EEPROM_IMG_EACH)

#define SN_LEN                      7
#define SN_OFFSET                   0x2000
#define SN_CHAR1                    'S'
#define SN_CHAR2                    'R'
#define SN_REV_CHAR                 'C'
#endif

#ifdef  CHROMA_74H
// Chroma 74H(+): CC1310F64, 800x480 BWY panel, 1 MB Macronix SPI flash.
// SPI flash 0x0000-0x2FFF holds the factory NVRAM (SN, panel serial,
// resolution) and the panel command script: never erase or write it.
#define BOARD_NAME                  "Chroma74H"
// Provisional: no OEPL tagtype exists for an 800x480 BWY Chroma yet.
// 0x80-0x86 are taken: 0x81 (CHROMA_AEON_74, "also Chroma 74H+") is
// 800x480 BWR, 0x86 (CHROMA_74_BWY) is the 640x384 CC1110 Chroma74 BWY.
#define HW_TYPE                     0x87
#define FLASH_MX25V8006
#define SCREEN_WIDTH                800
#define SCREEN_HEIGHT               480
// G5 compressed images (DATATYPE_IMG_G5), decoded by g5/ into a raw slot
#define G5_SUPPORT

#define EEPROM_FACTORY_START        0x0
#define EEPROM_FACTORY_LEN          0x3000
#define EEPROM_PANEL_SCRIPT         0x2000

#define EEPROM_SETTINGS_AREA_START  0x3000
#define EEPROM_SETTINGS_AREA_LEN    EEPROM_ERZ_SECTOR_SZ

#define EEPROM_OTA_START            (0x10000)
#define EEPROM_OTA_LEN              (64*1024)

// An 800x480 two plane image is 96000 bytes + header
#define EEPROM_IMG_START            (0x20000)
#define EEPROM_IMG_EACH             (0x18000)
#define EEPROM_IMG_SECTORS          (EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ)
#define IMAGE_SLOTS                 ((EEPROM_SIZE - EEPROM_IMG_START)/EEPROM_IMG_EACH)

// The SN is factory NVRAM record 0x80, not in internal flash as on the Aeon
#define SN_LEN                      7
#define SN_NVRAM_RECORD             0x80
#define SN_CHAR1                    'M'
#define SN_CHAR2                    'S'
#define SN_REV_CHAR                 'C'
#endif

#ifdef  CC1310_128K
#define BOARD_NAME                  "CC1310 128K"
#define FLASH_MX25V1006
#endif

#ifdef FLASH_MX25V8006
// Macronix 1 mbyte SPI flash parameters
#define EEPROM_SIZE                 0x100000
#define EEPROM_ERZ_SECTOR_SZ        0x1000   // erase size and alignment
#define EEPROM_WRITE_PAGE_SZ 256   // max write size & alignment
// #define CMD_ERASE_32K
// #define CMD_ERASE_64K
#endif

#ifdef FLASH_MX25V1006
// Macronix 128K SPI flash parameters
#define EEPROM_SIZE                 0x20000
#define EEPROM_ERZ_SECTOR_SZ        0x1000   // erase size and alignment
#define EEPROM_WRITE_PAGE_SZ 256   // max write size & alignment
// #define CMD_ERASE_32K
// #define CMD_ERASE_64K   0xd8
#endif

#endif   // _BOARD_H_
