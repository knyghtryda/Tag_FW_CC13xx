# Tag_FW_CC13xx — notes for AI assistants

OEPL firmware for TI CC13xx based Chroma tags. Current work: the **Chroma 74H**
(CC1310, SN prefix `MS`) in `oepl_CC1310/`. `PLAN.md` tracks what is in
flight; read it first. Work one verified step at a time.

Related checkouts next to this one, used as reference:

- `../Tag_FW_Chroma` — the CC1110 Chroma firmware. Factory NVRAM / waveform
  decoders and notes (`chroma_60_files/`, `chroma_21_files/`).
- `../chroma74h_reverse_engineering` — static analysis (Rizin + rz-ghidra) of
  the 74H stock firmware dump, run as its own Claude session. Its
  `docs/firmware.md` is the source for the pin map, peripheral use and (in
  progress) the stock radio protocol, EPD engine, OTA and power management.
  Check it before reverse engineering anything here.
- `../oepl-cc2630-firmware` — a finished bare-metal OEPL port to a CC2630
  (same CC26xx core family, 2.4 GHz IEEE radio, UC8159 panel). Its
  `DEVELOPMENT.md` and `ARCHITECTURE.md` are the model for how a port gets to
  "works on batteries": AP-driven verification, crash reporting, OTA safety,
  sleep current measured pin by pin. It is GPL-3.0 and by another author —
  carry over facts and techniques, don't copy code into this repo.

## Repo layout

| Path | What |
|---|---|
| `ChromaAeon74/` | CC1311 port (Aeon 74), the most complete one. TI-RTOS-free (NoRTOS) |
| `oepl_CC1310/` | CC1310 port. Today only a ping test and the `CHROMA_PROXY` build |
| `common/` | Shared by both: radio, syncedproto, eeprom (SPI flash), powermgt, proxy |
| `proxy_CC1311/`, `rfEchoTx*`, `uartecho_CC1310/` | Bring-up / test projects |
| `shared/`, `uzlib/`, `bb_epaper/` | Submodules |
| `tools/` | Host tools (factory-data decoder, XDS110 target config) |
| `dumps/` | Local vendor dumps — **gitignored, never commit** |

## Build

CCStudio 20 projects (see README): SIMPLELINK_CC13X0_SDK 4.20.02.07 and
ARM-CGT 18.12.8 for CC1310, the F2 SDK 8.30.01.01 for CC1311.

**Command-line build (WSL, TI ARM-CGT 18.12.8)** — `oepl_CC1310/makefile`
mirrors the CCS project's options; run it through the wrapper:

```
MSYS_NO_PATHCONV=1 wsl -d Ubuntu -- bash /mnt/c/Users/yancorp/Documents/GitHub/Tag_FW_CC13xx/tools/wsl_build.sh Debug   # or Proxy
```

Output: `oepl_CC1310/build/<CONFIG>/oepl_CC1310.out` + `.map` (gitignored).
The wrapper builds from an rsync'd copy in `~/build/Tag_FW_CC13xx`, because
`/mnt/c` is case-insensitive: `#include "Board.h"` from `common/` would pick
up `common/board.h` instead of `oepl_CC1310/Board.h`. `CONFIG=Release` doesn't
build (no board define, stale in the CCS project too). As of 2026-11-15, with
no optimisation (as in CCS Debug): Debug uses 52.4 KB flash / 6.3 KB RAM,
Proxy 59.4 KB / 7.0 KB, `.ccfg` at `0xFFA8` in both.

WSL already has (`~/ti`): **CC13X0 SDK 4.20.02.07** (installed 2026-11-15; it
ships both TI-compiler `.aem3` and GCC `.am3g` libraries, plus `ccs/` and
`gcc/` makefiles for `rfEasyLinkEchoTx`, which `oepl_CC1310` was created
from), `arm-none-eabi-gcc` 13.2, and the CC13xx/CC26xx SDK 8.32 (CC13x1 and
newer only — usable for the Aeon, **not** for the CC1310).

## Hardware access on this machine

**SPI flash (CH341A clip).** The CH341A is shared with usbipd-win (busid
`1-3`) and used from WSL Ubuntu, where flashrom is installed:

```
usbipd attach --wsl --busid 1-3        # PowerShell; usbipd detach --busid 1-3 to give it back
MSYS_NO_PATHCONV=1 wsl -d Ubuntu -- flashrom -p ch341a_spi -r /mnt/c/.../dump.bin
```

From Git Bash, set `MSYS_NO_PATHCONV=1` or `/mnt/c/...` paths get rewritten,
and put loops in a script file: `wsl` re-expands `$vars` on the command line.
Read at least twice and compare hashes before trusting a dump. The 74H chip
reads as an "SFDP-capable chip", JEDEC `C2 2014` (Macronix MX25V80xx, 1 MB).

**CC1310 internal flash (XDS110, cJTAG 2-pin).** UniFlash 9.5 is at
`C:\ti\uniflash_9.5.0`. Use the `tools/cc1310f*_xds110.ccxml` matching the
chip marking (OSCAN1, 500 kHz). **The 74H is a CC1310F64**: 64 KB flash
(`0x10000`, CCFG in the last 88 bytes at `0xFFA8`), 16 KB RAM. The F128 has
128 KB / 20 KB.

```
dslite.bat --mode memory --config=tools\cc1310f64_xds110.ccxml --range=0,0x10000 --output=dumps\chroma74h\cc1310_flash.bin
dslite.bat --mode flash  --config=tools\cc1310f64_xds110.ccxml -e -f -v image.bin,0x0
```

`oepl_CC1310/Chroma_CC1310.cmd` (originally an unmodified copy of TI's F128
LaunchPad file) now defaults to the F64: `FLASH_SIZE 0x10000`, `RAM_SIZE
0x4000`, so `.ccfg` lands at `0xFFA8`. An F128 tag (the Solum 2.9") needs
`--define=CC1310F128` in the **linker** options (the linker command file only
sees linker defines). Don't confuse it with the compiler define `CC1310_128K`,
which Debug/Proxy set: in `common/board.h` that selects the 128 KB **SPI flash**
(MX25V1006) board config, not the chip. Before it was fixed, `.ccfg`
went to 0x1FFA8 — nonexistent on an F64 — so earlier F64 builds can only have
booted on the stock CCFG left in the last sector.

A `dslite --mode memory` read of a running tag returns live RAM (useful for
diagnostics, e.g. `gEpdDiag` in `epd_74h.c`; find the address in the
`.map`), but **the firmware restarts afterwards** — seen as the EPD test
redrawing after each read.

Errors -242 / -2131 / -275 are a marginal TMS/TCK connection or the wrong scan
format: re-check the two JTAG wires, then drop TCLK to 100 kHz. On the 74H+
the back-side pads are unlabelled, and **the OEPL wiki's TMS/TCK assignment
is swapped**: wired as the wiki shows, the connect fails with -2131; with TMS
and TCK exchanged it works (user-verified 2026-11-15). RESET as on the wiki. The
`dbgjtag -S integrity` scan test reports `SC_ERR_PATH_MEASURE` (-230) on this
tag even when wired correctly, so test with a small `dslite --mode memory`
read instead. dslite reads past the end of an F64 (it returned 0x1FFC0 bytes
for `--range=0,0x10000`); everything above 0x10000 is `BA D0` fill, so keep
only the first 64 KB. After flashing,
**power cycle** the tag; the XDS110 reset pulse doesn't start the new image.

**Take the vendor dumps before flashing anything.** They are the only way back
to stock. If the debug port turns out to be locked, getting in means a mass
erase that destroys the stock firmware — stop and ask the user first.

## Chroma 74H facts (from the SPI dump, 2026-10-30)

`tools/decode_factory.py dumps/chroma74h/chroma74h_spi_flash.bin [--png out.png]`
prints all of this.

| SPI address | Contents |
|---|---|
| `0x0000`, `0x1000` | Factory NVRAM, magic `56 12 09 85`. Stock firmware alternates between the two sectors |
| `0x2000` | Panel command script, magic `0xBEEFFACE`, format `04 03`, name `D21Y10041BSMY09Z_V4 686 74OTP` |
| `0x8000` | Flash self-test pattern |
| `0x15000`, `0x2D000` | Stock screen slots: 0x20-byte header (u32 size = 96000), then two 48000-byte planes |

- NVRAM records: `<type> <len incl. 2-byte header> <data>`, type 0 =
  invalidated, `0xFF` = end, later records win. On CC1310 the types differ from
  CC1110: `0x80` tag SN (7 bytes: 2 chars, 4 BCD bytes, revision char — same
  layout as the Aeon `InitSN()` expects), `0x81` panel serial, `0x88`
  resolution (800 × 480). VCOM and temperature calibration records are not
  identified yet.
- Panel: **800×480 BWY** (black/white/yellow; user confirmed, and the `-Y-` in
  the panel serial matches the Chroma21's yellow `Y005`). The second plane in
  the screen slots is the yellow plane. The waveform is in the controller's OTP
  (`PSR 0x0F`, the "74OTP" in the name), so it ships with the panel and the
  firmware needs no colour-specific LUTs; a BWR 74H, if one exists, would
  differ only in the AP tagtype's colour table. The script has three
  segments: init `0x2047–0x20A0`, image `0x20A1–0x20A6`, refresh + off
  `0x20A7–0x20BE`. Init: panel power on (`09`), reset + hidden registers
  (`06`), `03 00`, eight `F8 <reg> <val>` hidden-register writes,
  `PSR 0F`, `BTST 57 2B 38`, `TSE 00`, `CDI 77`, `E8 36`, `PON`. Then: wait
  busy, image record (`07`), `11 00`, wait busy, `POF`, wait busy, 5 s delay,
  panel power off (`09`), end (`00`). Record meanings come from the stock
  interpreter (see "EPD interface" below).
- bb_epaper's `EP75R_800x480` (UC8179) left this panel blank: it uses partial
  window opcodes `0x91`/`0x92` that this controller doesn't accept. "686" in the
  script name probably names the actual controller.
- The SPI flash holds no firmware copy; the stock firmware only exists in the
  CC1310's internal flash.
- **Board pin map** (from the stock `BoardGpioInitTable` at `0xF164`, see
  `../chroma74h_reverse_engineering/docs/firmware.md` §3). It matches
  `oepl_CC1310/Chroma_CC1310.h` exactly, so that header's "Chroma 21" pinout
  is the 74H pinout too:

  | DIO | Net | Stock reset state |
  |---|---|---|
  | 0 | EPD_PWR | output high |
  | 1 | EPD_BUSY | input, no pull |
  | 2, 3, 4 | EPD CS, SCLK, SDI (stock drives the panel with hardware **SSI1**) | output low |
  | 5 | EPD_RST | output low |
  | 6, 7 | UART0 RX (pull-down) / TX (high) — jig/debug port | |
  | 8, 9, 10, 11 | SPI flash MOSI, CLK, MISO (pull-up), CS (high) on **SSI0** | |

- **EPD interface** (stock driver at `0xB130–0xBB00`, write-up in
  `../chroma74h_reverse_engineering/docs/epd.md`; items marked ✓ re-checked
  against the dump here):
  - 3-wire SPI on SSI1, **9-bit frames, D/C = bit 8**: commands sent raw, data
    as `byte | 0x100` ✓ (`orr.w #0x100` at `0xB6D4`). Same convention as the
    Aeon's `write9bits()`. CS (DIO2) driven by hand. Clock rate and SPI mode
    not yet found (mode 0 assumed).
  - **EPD_PWR (DIO0) is active-low: HIGH = off** ✓ (power-down sets DIO0 via
    DOUTSET). RST (DIO5) active-low. **BUSY (DIO1) is LOW while busy** —
    measured on hardware 2026-11-16; the RE notes' "active-high" is wrong.
    BUSY asserts within 1 ms of the command; waits after PON ~60 ms, after
    POF ~30 ms, and the **full refresh ~52 s** (room temperature).
  - **First light 2026-11-16:** `oepl_CC1310/epd_74h.c` (`make` config
    `EpdTest`) drew the stock checkmark screen from SPI flash `0x15000` by
    running the factory script. Bit-banged 9-bit SPI works.
  - **Plane polarity** (same test: a yellow plane of all `0xFF` showed no
    yellow; the B/W plane's `0xFF` background showed white): B/W plane
    (`0x10`) `1` = white, `0` = black; yellow plane (`0x13`) `1` = no
    yellow, `0` = yellow.
  - Power-up: RST low, PWR on, 25 ms, CS high, reset pulse (2 ms low, 5 ms
    settle), then the eight `F8` hidden-register writes (table at `0xBB30` ✓,
    same values as the script) and BTST `57 2B 38` (`0xBB40` ✓).
  - Power-down: PWR off, RST and CS driven **low**, not left floating.
  - Planes go out with `0x10` (B/W) and `0x13` (yellow), streamed from the
    SPI flash slots in small chunks, bytes sent exactly as stored.
  - Stock runs the 0x2000 script with an interpreter (`0xB330`); record types
    are decoded by `tools/decode_factory.py`.
  - **Refresh is `11 00`, then wait for busy** — stock never sends `0x12`
    (no `movs #0x12` or `#0x11` anywhere in the EPD code ✓; `0x11` only comes
    from the script). Draw tail: planes → `11 00` → wait busy → `POF 02` →
    wait busy → 5 s → panel power off. That `0x11` is the refresh opcode on
    this controller is an inference from the sequence; confirm on hardware.
  - Image record (`07`): `TRES 61 03 20 01 E0` (✓ `0x61` at `0xB89E`), then
    `VDCS 82 <vcom>` if a VCOM value is stored (✓ `0xB8C2`), then planes by
    record **byte 2**: `0` = both planes, `0x10` (B/W) then `0x13` (yellow);
    `1` = TRES/VCOM only; `≥2` = one plane, `0x10` if byte 1 is 2 or 5 else
    `0x13` (✓ selector at `0xB31E`). The factory script's `07 00 00` therefore
    sends both planes.
  - VCOM comes from stock's settings lookup, key `0x17`. If absent, `VDCS` is
    skipped. Guess (unconfirmed): NVRAM type = key + `0x80`, and this tag has
    no `0x97` record, so stock sends no VCOM to this OTP panel.
  - A full draw runs three script segments chosen by temperature from a table
    of 9-byte entries at SPI `0x202A` (layout being decoded).
  - Other readbacks: `0x81` (status, 1 byte, script record `08`, ✓ `0xB8E4`)
    and `0xA2` (OTP/VCOM, N bytes). Reads need the SDI line turned around.
- **SPI flash power** (✓ `0xB9` at `0xE49C`, `0xAB` at `0xE5F6`): stock wakes
  the flash with `0xAB` before every access and puts it back in deep
  power-down with `0xB9` after, with SSI0 power-gated in between.
- **Standby pin states** (RE project `firmware.md` §8.3.1): stock never
  reconfigures pins for sleep; each keeps its boot config. EPD_PWR high (off),
  EPD CS/CLK/SDI/RST low, BUSY input; UART RX pull-down, TX high; flash
  MOSI/CLK low, MISO pull-up, CS high (flash in deep power-down); **every
  unused DIO is an input with pull-down**. Nothing floats — the target for
  our sleep current.
- **CCFG:** stock has the internal DC/DC converter **off** (`MODE_CONF`
  bits 27:26 set); `oepl_CC1310/ccfg.c` overrides TI's default to match, so
  our `.ccfg` is byte-identical to stock.

## Chroma 74H stock firmware (internal flash, 2026-11-15)

`dumps/chroma74h/cc1310f64_stock_flash.bin` (64 KB, two identical reads) and
`cc1310f64_stock_ccfg.bin` (its last 88 bytes). All 16 sectors are used. No
text strings. Initial SP `0x20004000`, reset handler `0xF535`, exception
handlers in ROM at `0x1001C9FD` (TI-RTOS kernel in ROM).

Stock CCFG (at `0xFFA8`):

| Register | Value | Meaning |
|---|---|---|
| `BL_CONFIG` | `0x00FFFFFF` | `BOOTLOADER_ENABLE = 0x00`: **ROM serial bootloader disabled**, so a stock tag can't be read or flashed over UART |
| `CCFG_TAP_DAP_0` | `0xFFC500C5` | CPU DAP enabled (`0xC5`), so JTAG works; PRCM TAP off |
| `CCFG_TAP_DAP_1` | `0xFF000000` | PBIST / WUC TAPs off |
| `CCFG_TI_OPTIONS` | `0xFFFFFF00` | TI failure analysis off |
| `IMAGE_VALID_CONF` | `0x00000000` | Boot the image at 0x0 |
| `MODE_CONF` / `MODE_CONF_1` | `0xFFBFFF3A` / `0xFF880010` | Oscillator/DC-DC settings; compare with ours before flashing |
| `EXT_LF_CLK`, `SIZE_AND_DIS_FLAGS` | `0x01800000`, `0x0058FFFD` | |

Our own CCFG may set `BOOTLOADER_ENABLE = 0xC5` with a reachable backdoor pin
to allow serial re-flashing later; enabling it is safe, disabling it is what
bricked a CC2630.

## Radio: sub-GHz, compatible with the CC1110 Chroma tags

The CC1310 radio is sub-1 GHz only, so OEPL's 2.4 GHz 802.15.4 mode (as on
the CC2630 TG-GR6000N, `CMD_IEEE_RX`, channels 11–26) is not an option on
these tags.

OEPL here is **sub-GHz proprietary, not 802.15.4 PHY**, matching the CC1110
Chroma firmware (`../Tag_FW_Chroma/Chroma_Tag_FW/soc/cc111x/radio.c`) setting
for setting: 250 kbps 2-GFSK, 165 kHz deviation, sync `0xD391D391`,
CC1101/CC2500-compatible whitening, radio CRC16, variable length; channels
100–105 (864–869 MHz) and 200–205 (903–923 MHz) with identical synth values.
Only the frame *layout* is 802.15.4 MAC style (`MacFrameBcast` etc. from
`shared/oepl-proto.h`), with PAN ID `PROTO_PAN_ID_SUBGHZ` (`0x1337`), as on the
CC1110 tags. The CC1311 Aeon 74 uses this path (`common/radio.c`, sysconfig
`custom868`) and has downloaded images from a sub-GHz AP.

The CC1310 build does **not** use it yet: `oepl_CC1310/.cproject` excludes
`common/radio.c`, `syncedproto.c`, `RFQueue.c` and `powermgt.c`. Its main is an
EasyLink ping test on channel 203 with the same PHY
(`oepl_CC1310/smartrf_settings/`). `common/radio.c` expects sysconfig's
`ti_radio_config.h` (`RF_*_custom868_0` names), which the CC13x0 SDK project
doesn't generate, so porting it means mapping onto `smartrf_settings.c`. The
SmartRF settings ask for 14 dBm TX, which needs `CCFG_FORCE_VDDR_HH = 1` in the
CCFG.

## Factory NVRAM: don't destroy it

Learned on the CC1110 Chroma tags (`../Tag_FW_Chroma/chroma_60_files/pr_nvram_fixes.md`):

- Never erase/rewrite the factory NVRAM because one read failed. A lost SN
  turns the MAC into `…00000000`, and on Chroma29 an all-zero SN made the
  firmware back-power the switched-off EPD and killed panel rows.
- The live copy can be in either sector; any reset must handle both.
- Guard against zero-length records when scanning (one hung the CC1110 parser).

## CC13xx/CC26xx traps (from the CC2630 port)

- **CCFG:** setting `BOOTLOADER_ENABLE` (BL_CONFIG byte 51) to anything but
  `0xC5` disables the ROM bootloader; on a CC2630 with no JTAG access that was
  a brick. Never disable the CPU DAP (`CCFG_TAP_DAP_0`) either — JTAG is the
  only way into a tag whose bootloader is off, as the 74H's stock one is.
- **A JTAG connection keeps the debug power domain on until power is cut**, so
  standby current measured after attaching a debugger is meaningless. Power
  cycle with the probe detached before measuring.
- **Sleep current is mostly pin configuration.** Floating panel control lines
  cost tens of µA each (crowbar current in the panel's input stages); pulling
  them up during sleep took the CC2630 tag from 60 µA to 9 µA. Put the SPI
  flash in deep power-down with CS held high. Relevant to the README's 4 mA
  sleep to-do.
- A short power gap doesn't reset the tag; the board caps carry standby for
  seconds. Leave it off ≥ 15 s for a real cold boot.
- Bound every hardware wait loop. An infinite wait before logging is up looks
  exactly like a dead chip.

## OEPL / AP behaviour (applies to any tag)

- **The AP is the test oracle.** `http://<AP>/get_db?pos=N` (paginated) gives
  per-tag `ver`, `hash` (last image confirmed by XferComplete, all-zero =
  never) and `pending`. A check-in alone proves nothing about the receive path;
  always run an image push end to end.
- `AvailDataInfo.nextCheckIn` is minutes, or seconds in the low 15 bits when
  bit 15 is set.
- The AP picks the image format from the tag's **reported firmware version**,
  not its capabilities: a tagtype with `zlib_compression` serves
  `DATATYPE_IMG_ZLIB` once the reported version reaches that value. Keep the
  reported version below it until zlib decoding works.
- Report the real channel number in `currentChannel`; the AP drops check-ins
  from unknown MACs whose channel disagrees with its own.
- Send `XferComplete` after the refresh has finished and the panel is powered
  off, and retry it until acked. Sent during the refresh current spike it was
  lost about half the time.
- Retry a partly-received block with `BLOCK_PARTIAL_REQUEST` (served from the
  AP's buffer) rather than `BLOCK_REQUEST` (re-fetched from the ESP32).
- Keep AP `maxsleep` below 20 minutes: the AP drops pending data after 20
  housekeeping minutes. Queueing an OTA makes the AP hand out a huge
  `nextCheckIn`, so a tag that misses the offer sleeps its cap (an hour) and
  looks dead.
- The AP only renders content when the tag's expected check-in is within ~5
  minutes, and reboots now and then (a DB record jumping back is that).

## Conventions

- Vendor binaries (dumps, stock firmware) stay local in `dumps/`. Commit facts
  learned from them (tables, init sequences, decoders), not the blobs.
- Record what was measured and how; mark inferences as inferences.
- Match the surrounding code style (3-space indent, `LOG`/`ELOG`/`VLOG`
  logging from `common/logging.h`).
- Don't commit unless asked; the user's fork is `knyghtryda/Tag_FW_CC13xx`.
