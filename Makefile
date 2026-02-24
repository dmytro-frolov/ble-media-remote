################################################################################
# Makefile for nRF5 SDK BLE Media Remote Project
# Target: NRF52840
# Platform: macOS Intel
################################################################################

# Project name
PROJECT_NAME := ble_media_remote

# SDK version and path (SDK 15.3.0 for S140 v6.1.1 compatibility)
SDK_VERSION := 15.3.0
SDK_ROOT := $(HOME)/nrf5_sdk/nRF5_SDK_$(SDK_VERSION)_59ac345

# Toolchain path (update this to match your installation)
# Set this to the directory that contains arm-none-eabi-gcc.
GCCPATH := /usr/local/gcc-arm-none-eabi-10.3-2021.10/bin
# Alternative: if installed via Homebrew, use:
# GCCPATH := /usr/local/bin
# GCCPATH := /opt/homebrew/bin

# Toolchain configuration (required by SDK Makefile.common)
TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc
GNU_INSTALL_ROOT := $(GCCPATH)/
GNU_PREFIX := arm-none-eabi

# Output directory
OUTPUT_DIRECTORY := _build

# Source files
	SRC_FILES += \
	  $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52840.S \
	  $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52840.c \
	  $(SDK_ROOT)/components/libraries/util/app_error.c \
	  $(SDK_ROOT)/components/libraries/util/app_error_handler_gcc.c \
	  $(SDK_ROOT)/components/libraries/util/app_error_weak.c \
	  $(SDK_ROOT)/components/libraries/util/app_util_platform.c \
	  $(SDK_ROOT)/components/libraries/util/nrf_assert.c \
  $(SDK_ROOT)/components/libraries/timer/app_timer.c \
  $(SDK_ROOT)/components/libraries/atomic/nrf_atomic.c \
  $(SDK_ROOT)/components/libraries/atomic_fifo/nrf_atfifo.c \
  $(SDK_ROOT)/components/libraries/atomic_flags/nrf_atflags.c \
  $(SDK_ROOT)/components/libraries/balloc/nrf_balloc.c \
  $(SDK_ROOT)/components/libraries/memobj/nrf_memobj.c \
  $(SDK_ROOT)/components/libraries/pwr_mgmt/nrf_pwr_mgmt.c \
  $(SDK_ROOT)/components/libraries/ringbuf/nrf_ringbuf.c \
  $(SDK_ROOT)/components/libraries/experimental_section_vars/nrf_section_iter.c \
  $(SDK_ROOT)/components/libraries/sortlist/nrf_sortlist.c \
  $(SDK_ROOT)/components/libraries/strerror/nrf_strerror.c \
  $(SDK_ROOT)/components/boards/boards.c \
	  $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_clock.c \
	  $(SDK_ROOT)/modules/nrfx/soc/nrfx_atomic.c \
	  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_clock.c \
	  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_saadc.c \
	  $(SDK_ROOT)/external/fprintf/nrf_fprintf.c \
	  $(SDK_ROOT)/external/fprintf/nrf_fprintf_format.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_Syscalls_GCC.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_printf.c \
	  $(SDK_ROOT)/components/ble/common/ble_advdata.c \
	  $(SDK_ROOT)/components/ble/common/ble_conn_params.c \
	  $(SDK_ROOT)/components/ble/common/ble_conn_state.c \
	  $(SDK_ROOT)/components/ble/common/ble_srv_common.c \
	  $(SDK_ROOT)/components/ble/ble_link_ctx_manager/ble_link_ctx_manager.c \
	  $(SDK_ROOT)/components/ble/ble_services/ble_bas/ble_bas.c \
	  $(SDK_ROOT)/components/ble/ble_services/ble_dis/ble_dis.c \
	  $(SDK_ROOT)/components/ble/ble_services/ble_hids/ble_hids.c \
	  $(SDK_ROOT)/components/ble/ble_advertising/ble_advertising.c \
	  $(SDK_ROOT)/components/ble/nrf_ble_gatt/nrf_ble_gatt.c \
	  $(SDK_ROOT)/components/ble/nrf_ble_qwr/nrf_ble_qwr.c \
	  $(SDK_ROOT)/components/ble/peer_manager/peer_manager.c \
	  $(SDK_ROOT)/components/ble/peer_manager/peer_manager_handler.c \
  $(SDK_ROOT)/components/ble/peer_manager/peer_database.c \
  $(SDK_ROOT)/components/ble/peer_manager/peer_data_storage.c \
  $(SDK_ROOT)/components/ble/peer_manager/peer_id.c \
  $(SDK_ROOT)/components/ble/peer_manager/id_manager.c \
  $(SDK_ROOT)/components/ble/peer_manager/gatt_cache_manager.c \
  $(SDK_ROOT)/components/ble/peer_manager/gatts_cache_manager.c \
  $(SDK_ROOT)/components/ble/peer_manager/security_dispatcher.c \
  $(SDK_ROOT)/components/ble/peer_manager/security_manager.c \
  $(SDK_ROOT)/components/ble/peer_manager/pm_buffer.c \
  $(SDK_ROOT)/components/ble/peer_manager/auth_status_tracker.c \
  $(SDK_ROOT)/components/ble/peer_manager/nrf_ble_lesc.c \
	  $(SDK_ROOT)/components/libraries/fds/fds.c \
	  $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage.c \
	  $(SDK_ROOT)/components/libraries/fstorage/nrf_fstorage_sd.c \
	  $(SDK_ROOT)/components/softdevice/common/nrf_sdh.c \
	  $(SDK_ROOT)/components/softdevice/common/nrf_sdh_ble.c \
	  $(SDK_ROOT)/components/softdevice/common/nrf_sdh_soc.c \
	  src/main.c \
	  src/uart_log.c

# Include paths - explicit list with SoftDevice headers first
	INC_FOLDERS += \
	  config \
	  src \
	  $(SDK_ROOT)/components/softdevice/s140/headers \
	  $(SDK_ROOT)/components/softdevice/s140/headers/nrf52 \
	  $(SDK_ROOT)/components/softdevice/common \
	  $(SDK_ROOT)/components/ble/common \
	  $(SDK_ROOT)/components/ble/ble_link_ctx_manager \
	  $(SDK_ROOT)/components/ble/ble_advertising \
	  $(SDK_ROOT)/components/ble/ble_services/ble_bas \
	  $(SDK_ROOT)/components/ble/ble_services/ble_dis \
	  $(SDK_ROOT)/components/ble/ble_services/ble_hids \
	  $(SDK_ROOT)/components/ble/nrf_ble_gatt \
	  $(SDK_ROOT)/components/ble/nrf_ble_qwr \
	  $(SDK_ROOT)/components/ble/peer_manager \
	  $(SDK_ROOT)/components/boards \
  $(SDK_ROOT)/components/libraries/atomic \
  $(SDK_ROOT)/components/libraries/atomic_fifo \
  $(SDK_ROOT)/components/libraries/atomic_flags \
  $(SDK_ROOT)/components/libraries/balloc \
  $(SDK_ROOT)/components/libraries/delay \
  $(SDK_ROOT)/components/libraries/experimental_section_vars \
  $(SDK_ROOT)/components/libraries/log \
  $(SDK_ROOT)/components/libraries/log/src \
  $(SDK_ROOT)/components/libraries/fds \
  $(SDK_ROOT)/components/libraries/fstorage \
  $(SDK_ROOT)/components/libraries/memobj \
  $(SDK_ROOT)/components/libraries/mutex \
  $(SDK_ROOT)/components/libraries/pwr_mgmt \
  $(SDK_ROOT)/components/libraries/ringbuf \
  $(SDK_ROOT)/components/libraries/sortlist \
  $(SDK_ROOT)/components/libraries/strerror \
  $(SDK_ROOT)/components/libraries/timer \
  $(SDK_ROOT)/components/libraries/util \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/external/fprintf \
  $(SDK_ROOT)/external/segger_rtt \
  $(SDK_ROOT)/integration/nrfx \
  $(SDK_ROOT)/integration/nrfx/legacy \
  $(SDK_ROOT)/modules/nrfx \
  $(SDK_ROOT)/modules/nrfx/drivers/include \
  $(SDK_ROOT)/modules/nrfx/hal \
  $(SDK_ROOT)/modules/nrfx/mdk

# SoftDevice (SDK 15.3.0 ships with S140 v6.1.1)
SOFTDEVICE_MODEL := s140
SOFTDEVICE_VERSION := 6.1.1
SOFTDEVICE_HEX := $(SDK_ROOT)/components/softdevice/$(SOFTDEVICE_MODEL)/hex/$(SOFTDEVICE_MODEL)_nrf52_$(SOFTDEVICE_VERSION)_softdevice.hex

# Target MCU
TARGET_MCU := NRF52840_XXAA
TARGETS := nrf52840_xxaa

# Linker script (nice!nano with S140 v6.1.1)
LINKER_SCRIPT := nicenano.ld

# Optional features (override on command line, e.g. `make UART_LOG=0`)
UART_LOG ?= 1

# Compiler flags
CFLAGS += -DNRF52840_XXAA
CFLAGS += -DNRF_SD_BLE_API_VERSION=6
CFLAGS += -DS140
CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -DBLE_STACK_SUPPORT_REQD
CFLAGS += -DBOARD_PCA10056
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
CFLAGS += -DFLOAT_ABI_HARD
# No APP_TIMER_V2 in SDK 15.3
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -Wall -Werror
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums
CFLAGS += -O3 -g3
CFLAGS += -DDEBUG

# Optional debug define (set DEBUG_NRF=1 to enable)
DEBUG_NRF ?= 0
ifeq ($(DEBUG_NRF),1)
CFLAGS += -DDEBUG_NRF
endif
CFLAGS += -DAPP_UART_LOG_ENABLED=$(UART_LOG)

# Assembler flags
ASMFLAGS += -DNRF52840_XXAA
ASMFLAGS += -DNRF_SD_BLE_API_VERSION=6
ASMFLAGS += -DS140
ASMFLAGS += -DSOFTDEVICE_PRESENT
ASMFLAGS += -DBLE_STACK_SUPPORT_REQD
ASMFLAGS += -DBOARD_PCA10056
ASMFLAGS += -DCONFIG_GPIO_AS_PINRESET
ASMFLAGS += -DFLOAT_ABI_HARD
# No APP_TIMER_V2 in SDK 15.3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16

# Linker flags
LDFLAGS += -mthumb -mabi=aapcs -L$(SDK_ROOT)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS += -Wl,--gc-sections
LDFLAGS += --specs=nano.specs

# Standard libraries
LIB_FILES += -lc -lnosys -lm

# Default target
.PHONY: default
default: nrf52840_xxaa

# Include common Makefile
include $(SDK_ROOT)/components/toolchain/gcc/Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))

# Flash target
flash: default
	@echo Flashing: $(OUTPUT_DIRECTORY)/$(firstword $(TARGETS)).hex
	nrfjprog -f nrf52 --program $(OUTPUT_DIRECTORY)/$(firstword $(TARGETS)).hex --sectorerase
	nrfjprog -f nrf52 --reset

# Flash softdevice
flash_softdevice:
	@echo Flashing: $(SOFTDEVICE_HEX)
	nrfjprog -f nrf52 --program $(SOFTDEVICE_HEX) --sectorerase
	nrfjprog -f nrf52 --reset

# Erase target
erase:
	nrfjprog -f nrf52 --eraseall

# Clean target
clean:
	rm -rf $(OUTPUT_DIRECTORY)

# nice!nano UF2 flash target
# 1. Put nice!nano in bootloader mode (double-tap reset)
# 2. Run: make flash_nicenano
NICENANO_VOLUME := /Volumes/NICENANO
UF2_FILE := $(OUTPUT_DIRECTORY)/$(PROJECT_NAME).uf2

uf2: default
	@echo "Converting to UF2 format..."
	python3 uf2conv.py $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex -c -f 0xADA52840 -o $(UF2_FILE)
	@echo "Created: $(UF2_FILE)"

flash_nicenano: uf2
	@echo "Looking for nice!nano bootloader volume..."
	@if [ -d "$(NICENANO_VOLUME)" ]; then \
		echo "Copying $(UF2_FILE) to $(NICENANO_VOLUME)..."; \
		cp $(UF2_FILE) $(NICENANO_VOLUME)/; \
		echo "Done! Device will reboot automatically."; \
	else \
		echo "ERROR: $(NICENANO_VOLUME) not found."; \
		echo "Put your nice!nano in bootloader mode (double-tap reset button)."; \
		exit 1; \
	fi

.PHONY: flash flash_softdevice flash_nicenano uf2 erase clean
