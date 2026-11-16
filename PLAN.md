# Plan

Status legend: `[ ]` todo · `[~]` in progress · `[x]` done (verified)

## Chroma 74H (CC1310) port

Bench tag: SN `MS10085924C`, 800×480 BWY (black/white/yellow).

### 1. Extraction

- [x] SPI flash dumped with CH341A + flashrom, three identical reads (2026-10-30).
      `dumps/chroma74h/chroma74h_spi_flash.bin`
- [x] Factory data decoded: NVRAM (SN, panel serial, resolution), panel command
      script, stock screen slots. `tools/decode_factory.py`; facts in CLAUDE.md
- [x] Chip identified from its marking: **CC1310F64** (64 KB flash, 16 KB RAM)
- [x] CC1310 internal flash dumped over XDS110, two identical reads, debug port
      not locked (2026-11-15). `dumps/chroma74h/cc1310f64_stock_flash.bin`
- [x] CCFG saved and decoded: ROM bootloader disabled, CPU DAP enabled.
      `dumps/chroma74h/cc1310f64_stock_ccfg.bin`; table in CLAUDE.md

### 2. Board bring-up

- [x] `oepl_CC1310/Chroma_CC1310.cmd` sized for the F64 by default (flash
      0x10000, RAM 0x4000), F128 via linker `--define=CC1310F128`. Verified
      2026-11-15 with the WSL build: Debug and Proxy link, `.ccfg` at 0xFFA8
- [x] WSL command-line build: `oepl_CC1310/makefile` + `tools/wsl_build.sh`,
      TI ARM-CGT 18.12.8 and CC13X0 SDK 4.20.02.07 in `~/ti`
- [ ] **Code size:** the unoptimised ping test already uses 80% of the 64 KB
      flash (Proxy 91%). Try `--opt_level` / `--opt_for_speed=0` and see what
      the EasyLink + display libraries cost before the OEPL code goes in. The
      OEPL build won't need EasyLink (it uses the RF driver directly)
- [ ] Release configuration is stale (no board define); fix or drop it
- [x] CCFG matches stock byte for byte (2026-11-15). TI's default differed only
      in `MODE_CONF`: it enabled the DC/DC converter, which stock has off
      (no inductor on the board, presumably). `oepl_CC1310/ccfg.c` now
      overrides `DCDC_ACTIVE`/`DCDC_RECHARGE`. CPU DAP stays enabled
- [ ] RAM budget: an 800×480 two-plane frame is 96 KB against 16 KB of RAM, so
      the display path must stream from SPI flash (as the CC2630 port does)
- [x] Pin map recovered from the stock firmware by `../chroma74h_reverse_engineering`
      (2026-11-15): matches `oepl_CC1310/Chroma_CC1310.h` already, no change needed.
      Stock drives the EPD on hardware SSI1 and the SPI flash on SSI0
- [ ] Identify the remaining NVRAM records (VCOM? temperature calibration?)
- [ ] Identify the panel controller ("686" in the script name)
- [x] Stock EPD driver reverse engineered (RE project `docs/epd.md`, key points
      re-checked against the dump): 9-bit SPI with D/C = bit 8, EPD_PWR
      active-low, power/reset sequence, planes via `0x10`/`0x13` streamed from
      flash. Details in CLAUDE.md, "EPD interface"
- [x] Refresh trigger: `11 00` + wait busy, no `0x12` (RE project, spot-checked);
      script records `08` (status read) and `0a` (conditional power) decoded
- [x] B/W plane: the script's `07 00 00` (byte 2 = 0) uploads both planes,
      `0x10` then `0x13` (RE project)
- [ ] Open, sent to the RE project: VCOM key `0x17` → NVRAM record mapping (and
      the other boot-time records), the temperature/segment table at SPI
      `0x202A`, SSI1 clock rate and SPI mode, standby entry path and per-pin
      sleep states
- [x] EPD driver, first light (2026-11-16): `oepl_CC1310/epd_74h.c` runs the
      factory script from SPI flash with bit-banged 9-bit SPI and drew the stock
      checkmark screen. BUSY found to be active-low; refresh takes ~52 s.
      EPD control lines now idle low at boot (were high: back-powering risk)
- [ ] Confirm yellow-plane polarity on hardware
- [ ] Move the EPD to SSI1 9-bit hardware SPI (README to-do; stock does this)
- [ ] Decode the script's temperature/segment table instead of starting at the
      fixed offset 0x47
- [ ] Battery voltage and temperature reporting

### 3. OEPL

- [x] Radio target confirmed (2026-11-15): sub-GHz proprietary PHY identical to
      the CC1110 Chroma tags (250 kbps GFSK, sync D391D391, channels 100–105 /
      200–205), not 802.15.4. See CLAUDE.md, "Radio"
- [ ] Bring `common/radio.c`, `syncedproto.c`, `RFQueue.c`, `powermgt.c` into the
      CC1310 build (excluded today); map `ti_radio_config.h` names onto
      `oepl_CC1310/smartrf_settings/`
- [ ] 74H board config in `common/board.h`. Don't reuse the Aeon layout: it puts
      `EEPROM_SETTINGS_AREA_START` at SPI 0x0, which on the 74H is the factory
      NVRAM (SN!) — 0x0000–0x2FFF must never be written. Free on the 74H:
      0x3000–0x7FFF, 0x9000–0x14FFF, 0x45000+ (stock slots 0x15000–0x44FFF
      become reusable once OEPL owns the display). SN from NVRAM record 0x80,
      not internal flash 0x2000 as on the Aeon
- [ ] RAM: the Aeon's zlib state (8 KB window + 4 KB block buffer) is too big
      for 16 KB; use a 4 KB window (the AP compresses with a 4 KB dictionary)
- [ ] Main loop on CC1310 (port `ChromaAeon74/main.c` flow onto `common/`):
      SN → MAC, channel scan, check-in
- [ ] Image download into the SPI flash slots and display
- [ ] HW_TYPE and AP tagtype file for the 74H (BWY colour table)
- [ ] End-to-end image push verified at the AP (`hash` updates, `pending` clears)

## README to-do list (all CC13xx tags)

- [x] gzip support for image downloads
- [x] Battery voltage and temperature measurement and reporting
- [ ] UI: startup info screens and AP-lost screen
- [ ] Status icons on normal screens (AP lost, low battery)
- [ ] Static screens
- [ ] EPD interface on SPI hardware instead of bit banging
- [ ] Replace the TI NVS driver with something simpler (probably needed for OTA)
- [ ] OTA
- [ ] Check Tx power
- [ ] Audit current consumption (Aeon 74 sleeps at 4 mA). Start with pin
      states during sleep; see CLAUDE.md, "CC13xx/CC26xx traps"
