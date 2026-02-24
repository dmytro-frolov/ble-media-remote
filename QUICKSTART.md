# Quick Start Guide

## Prerequisites Checklist

- [ ] macOS Intel
- [ ] nRF52840 Development Kit (DK)
- [ ] USB cable to connect the DK

## Installation Steps

### 1. Run Setup Script

```bash
./setup.sh
```

This will:
- Check for required tools
- Install ARM GCC toolchain via Homebrew (if needed)
- Provide instructions for missing components

### 2. Download nRF5 SDK

1. Visit: https://www.nordicsemi.com/Software-and-tools/Software/nRF5-SDK
2. Download nRF5 SDK v17.1.0 or later
3. Extract to `~/nrf5_sdk/`

### 3. Install nRF Command Line Tools

1. Visit: https://www.nordicsemi.com/Software-and-tools/Development-Tools/nRF-Command-Line-Tools
2. Download and install the macOS package
3. Verify installation: `nrfjprog --version`

### 4. Configure Project

Edit `Makefile` and update:

```makefile
SDK_ROOT := ~/nrf5_sdk/nRF5_SDK_17.1.0_ddde560
```

Replace `17.1.0_ddde560` with your actual SDK version.

### 5. Copy SDK Configuration

The `config/sdk_config.h` file is minimal. For a complete configuration:

```bash
cp ~/nrf5_sdk/nRF5_SDK_17.1.0_ddde560/examples/ble_peripheral/ble_app_uart/pca10056/s140/armgcc/config/sdk_config.h config/
```

### 6. Build and Flash

```bash
# Build the project
make

# Build with UART logs enabled (default)
make UART_LOG=1

# Build with UART logs disabled
make UART_LOG=0

# If you previously built with logs enabled, do a clean build
make clean
make UART_LOG=0

# Final release build (recommended)
make UART_LOG=0

# Enable DEBUG_NRF define (optional)
make DEBUG_NRF=1

# Flash nice!nano (logs stay enabled unless you built with UART_LOG=0)
make UART_LOG=0 flash_nicenano

# Connect your nRF52840 DK via USB
# Flash the firmware
make flash
```

## Project Structure

```
ble-media-remote/
├── Makefile              # Build configuration
├── setup.sh              # Setup script
├── README.md             # Full documentation
├── QUICKSTART.md         # This file
├── config/
│   └── sdk_config.h      # SDK configuration (minimal - copy from example)
└── src/
    ├── main.c            # Main application
    ├── ble_app.c/h       # BLE application layer
    ├── ble_stack.c/h     # BLE stack initialization
    └── media_remote.c/h  # Media remote control (TODO: implement HID)
```

## Next Steps

1. **Implement BLE HID Service**: The `media_remote.c` file has TODO comments for implementing the HID service for media control.

2. **Add Button Input**: Connect physical buttons to GPIO pins and implement button handlers.

3. **Test with iOS**: Once HID is implemented, the device should appear as a media remote on iOS devices.

## Troubleshooting

### Build Errors

- **"SDK_ROOT not found"**: Update `SDK_ROOT` in `Makefile` to point to your SDK location
- **"arm-none-eabi-gcc not found"**: Install via `brew install arm-none-eabi-gcc`
- **"nrfjprog not found"**: Install nRF Command Line Tools

### Flash Errors

- **"No debugger found"**: Make sure the nRF52840 DK is connected via USB
- **"Device not found"**: Try `nrfjprog --ids` to list connected devices

### Runtime Issues

- Check UART output for debug messages (if configured)
- Verify SoftDevice is flashed: `nrfjprog --readregs`

## Resources

- [nRF5 SDK Documentation](https://infocenter.nordicsemi.com/topic/sdk_nrf5_v17.1.0/index.html)
- [nRF52840 Product Page](https://www.nordicsemi.com/Products/nRF52840)
- [nRF Connect SDK](https://www.nordicsemi.com/Software-and-tools/Software/nRF-Connect-SDK) (alternative SDK)
