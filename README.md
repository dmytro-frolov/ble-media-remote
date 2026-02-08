# BLE Media Remote for iOS

A Bluetooth Low Energy (BLE) remote control application for iOS devices, built using the nRF5 SDK for the NRF52840 microcontroller.

## Prerequisites

### Required Software (macOS Intel)

1. **nRF5 SDK v17.1.0** (or later)
   - Download from: https://www.nordicsemi.com/Software-and-tools/Software/nRF5-SDK
   Note: Download `nrf5_sdk_17.1.0_ddde560.zip`
   - Extract to a location of your choice (e.g., `~/nrf5_sdk`)

2. **GNU ARM Embedded Toolchain**
   - Download from: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm
   Note: Download `gcc-arm-none-eabi-10.3-2021.10-mac.tar.bz2`
   - Install to `/usr/local/gcc-arm-none-eabi-10.3-2021.10` (or update path in Makefile)
   - Or install via Homebrew: `brew install arm-none-eabi-gcc`

3. **nRF Command Line Tools**
   - Download from: https://www.nordicsemi.com/Software-and-tools/Development-Tools/nRF-Command-Line-Tools
   - Install nrfjprog and mergehex

4. **Make** (usually pre-installed on macOS)

5. **Segger J-Link Software** (for debugging)
   - Download from: https://www.segger.com/downloads/jlink/

## Setup Instructions

### Quick Setup (Recommended)

Run the setup script to check and install dependencies:

```bash
./setup.sh
```

### Manual Setup

1. **Install the nRF5 SDK:**
   ```bash
   # Download nRF5 SDK v17.1.0 or later from:
   # https://www.nordicsemi.com/Software-and-tools/Software/nRF5-SDK
   
   # Extract to ~/nrf5_sdk/
   mkdir -p ~/nrf5_sdk
   # Extract the SDK archive here
   ```

2. **Set up the toolchain:**
   ```bash
   # Option 1: Using Homebrew (recommended)
   brew install arm-none-eabi-gcc
   
   # Option 2: Manual installation
   # Download from: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm
   # Extract and update GCCPATH in Makefile
   ```

3. **Install nRF Command Line Tools:**
   ```bash
   # Download from:
   # https://www.nordicsemi.com/Software-and-tools/Development-Tools/nRF-Command-Line-Tools
   # Install the .pkg file
   ```

4. **Configure the project:**
   ```bash
   # Edit Makefile and update SDK_ROOT to your nRF5 SDK path
   # Example: SDK_ROOT := ~/nrf5_sdk/nRF5_SDK_17.1.0_ddde560
   
   # Also update GCCPATH if you installed the toolchain manually
   ```

5. **Copy SDK configuration:**
   ```bash
   # The config/sdk_config.h is minimal. For a complete configuration,
   # copy from an nRF5 SDK example:
  cp <SDK_ROOT>/examples/ble_peripheral/ble_app_uart/pca10056/s140/config/sdk_config.h config/
   ```

6. **Build the project:**
   ```bash
   make
   ```

7. **Flash to device:**
   ```bash
   # Connect your nRF52840 DK via USB
   make flash
   ```

## Project Structure

```
.
├── Makefile              # Build configuration
├── src/                  # Application source files
│   ├── main.c           # Main application entry point
│   ├── ble_app.c        # BLE application logic
│   └── media_remote.c   # Media remote control implementation
├── config/              # Configuration files
│   └── sdk_config.h     # SDK configuration
└── README.md            # This file
```

## Features

- BLE HID (Human Interface Device) profile for media control
- iOS-compatible BLE services
- Media control buttons (Play/Pause, Next, Previous, Volume)
- Low power operation

## Erasing Bonds (Pairing Reset)

The firmware uses bonding (Peer Manager). If iOS refuses to reconnect, or you want to pair a different phone, erase the stored bonds:

1. Press and hold the **Play/Pause** button (`P0.22`, active-low).
2. While holding it, **reset/power-cycle** the nice!nano.
3. Release the button after boot. The device will delete bonds and start advertising again.

Tip: on iOS you may also need to go to Bluetooth settings and tap **“Forget This Device”** for the old pairing.

## Battery Level (iOS)

The firmware exposes the standard **Battery Service (BAS)** and periodically updates the battery level to iOS.

To measure the actual LiPo voltage (so iOS battery % makes sense), wire a **battery sense divider** to an SAADC input.

Current default wiring in firmware (`src/main.c`):

- `BAT+` -> **1M** -> `P0.02 (AIN0)` -> **1M** -> `GND`
- (Optional) add **0.1uF** from `P0.02` to `GND` for steadier readings.

The divider is required because a full LiPo is ~4.2V, while the nRF52840 analog input must stay <= `VDD` (~3.3V).

## Building

```bash
# Clean build
make clean

# Build
make

# Flash to device
make flash_nicenano

# Erase device
make erase
```

## Hardware

- **MCU:** NRF52840
- **Development Board:** nRF52840 DK (or compatible)

## License

[Add your license here]
