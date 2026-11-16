# Command-line build of oepl_CC1310 with TI ARM-CGT, mirroring the CCS
# project (.cproject). Runs in WSL/Linux:
#
#   make [CONFIG=Debug|Proxy|EpdTest|Release]   ->  build/<CONFIG>/oepl_CC1310.out + .map
#
# EpdTest is a Chroma 74H bring-up build: it draws the stock screen stored in
# SPI flash at 0x15000 using the panel's factory script, then idles.
#   make clean [CONFIG=...]
#
# SDK and compiler locations can be overridden on the command line.

SDK ?= $(HOME)/ti/simplelink_cc13x0_sdk_4_20_02_07
CGT ?= $(HOME)/ti/ti-cgt-arm_18.12.8.LTS
CONFIG ?= Debug

CC = $(CGT)/bin/armcl
BUILD = build/$(CONFIG)
NAME = oepl_CC1310

DEFINES = -DDeviceFamily_CC13X0 -DCCFG_FORCE_VDDR_HH=0 \
          -DSUPPORT_PHY_CUSTOM -DSUPPORT_PHY_50KBPS2GFSK \
          -DSUPPORT_PHY_625BPSLRM -DSUPPORT_PHY_5KBPSSLLR

# Files the CCS Debug/Proxy configurations exclude from ../common
COMMON_EXCLUDE = syncedproto.c RFQueue.c radio.c powermgt.c

ifeq ($(CONFIG),Debug)
DEFINES += -DSERIAL_LOG=1 -DDEBUG_LOG=0 -DCC1310_128K -D_CUSTOM_IO_=oepl_io.inl
else ifeq ($(CONFIG),Proxy)
DEFINES += -DSERIAL_LOG=1 -DDEBUG_LOG=0 -DCC1310_128K -D_CUSTOM_IO_=oepl_io.inl \
           -DCHROMA_PROXY
else ifeq ($(CONFIG),EpdTest)
DEFINES += -DSERIAL_LOG=1 -DDEBUG_LOG=0 -DCHROMA_74H -D_CUSTOM_IO_=oepl_io.inl \
           -DEPD_TEST
else ifeq ($(CONFIG),Release)
COMMON_EXCLUDE =
else
$(error CONFIG must be Debug, Proxy or Release)
endif

SOURCES = $(wildcard *.c easylink/*.c smartrf_settings/*.c) \
          $(filter-out $(addprefix ../common/,$(COMMON_EXCLUDE)),$(wildcard ../common/*.c))
OBJECTS = $(addprefix $(BUILD)/,$(notdir $(SOURCES:.c=.obj)))
vpath %.c . easylink smartrf_settings ../common

CFLAGS = --silicon_version=7M3 --code_state=16 --little_endian \
         --float_support=none --symdebug:dwarf --gen_func_subsections=on \
         --diag_warning=225 --diag_warning=255 --diag_wrap=off \
         --display_error_number $(DEFINES) \
         -I. -I../common -I../shared -I../bb_epaper/src \
         -I$(SDK)/source -I$(SDK)/kernel/nortos -I$(SDK)/kernel/nortos/posix \
         -I$(CGT)/include

LFLAGS = -i$(SDK)/source -i$(SDK)/kernel/nortos -i$(CGT)/lib \
         -lti/display/lib/display.aem3 \
         -lti/drivers/rf/lib/rf_multiMode_cc13x0.aem3 \
         -lti/drivers/lib/drivers_cc13x0.aem3 \
         -llib/nortos_cc13x0.aem3 \
         -lti/devices/cc13x0/driverlib/bin/ccs/driverlib.lib \
         -llibc.a \
         --rom_model --warn_sections --display_error_number --diag_wrap=off \
         -m$(BUILD)/$(NAME).map

all: $(BUILD)/$(NAME).out

$(BUILD)/%.obj: %.c | $(BUILD)
	@echo "CC $<"
	@$(CC) $(CFLAGS) --preproc_with_compile \
	   --preproc_dependency=$(@:.obj=.d) $< --output_file=$@

$(BUILD)/$(NAME).out: $(OBJECTS) Chroma_CC1310.cmd
	@echo "LINK $@"
	@$(CC) $(CFLAGS) -z $(OBJECTS) Chroma_CC1310.cmd $(LFLAGS) -o $@

$(BUILD):
	@mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean

-include $(OBJECTS:.obj=.d)
