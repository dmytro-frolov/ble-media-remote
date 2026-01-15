#!/bin/bash

################################################################################
# Setup script for nRF5 SDK BLE Media Remote Project
# This script helps set up the development environment
################################################################################

set -e

echo "=========================================="
echo "nRF5 SDK BLE Media Remote Setup"
echo "=========================================="
echo ""

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: This setup script is for macOS only."
    exit 1
fi

# Check for Homebrew
if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

echo "Checking for required tools..."

# Check for ARM GCC toolchain
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "ARM GCC toolchain not found. Installing via Homebrew..."
    brew install arm-none-eabi-gcc
else
    echo "✓ ARM GCC toolchain found"
fi

# Check for nrfjprog
if ! command -v nrfjprog &> /dev/null; then
    echo ""
    echo "⚠ nrfjprog not found. Please install nRF Command Line Tools:"
    echo "   https://www.nordicsemi.com/Software-and-tools/Development-Tools/nRF-Command-Line-Tools"
    echo ""
else
    echo "✓ nrfjprog found"
fi

# Check for Make
if ! command -v make &> /dev/null; then
    echo "Make not found. Installing via Homebrew..."
    brew install make
else
    echo "✓ Make found"
fi

echo ""
echo "=========================================="
echo "Next Steps:"
echo "=========================================="
echo ""
echo "1. Download and install nRF5 SDK v17.1.0 or later:"
echo "   https://www.nordicsemi.com/Software-and-tools/Software/nRF5-SDK"
echo ""
echo "2. Extract the SDK to ~/nrf5_sdk/"
echo ""
echo "3. Update the SDK_ROOT path in Makefile if needed:"
echo "   SDK_ROOT := ~/nrf5_sdk/nRF5_SDK_17.1.0_ddde560"
echo ""
echo "4. Copy a complete sdk_config.h from an nRF5 SDK example:"
echo "   cp <SDK_ROOT>/examples/ble_peripheral/ble_app_uart/pca10056/s140/config/sdk_config.h config/"
echo ""
echo "5. Build the project:"
echo "   make"
echo ""
echo "6. Flash to device:"
echo "   make flash"
echo ""
echo "=========================================="
