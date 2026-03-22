#!/bin/bash
set -e

echo "=== Sniper Simulator Bootstrap ==="

REPAIR=0
if [[ "$1" == "--repair" ]]; then
    echo "[INFO] Repair mode enabled. Will re-fetch and re-verify everything."
    REPAIR=1
fi

# 1. Check for basic tools
for cmd in cmake make git curl wget python3; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: $cmd is required."
        exit 1
    fi
done

echo "Verifying system libraries..."
LIBS_MISSING=0
pkg-config --exists sqlite3 || LIBS_MISSING=1
pkg-config --exists libcurl || LIBS_MISSING=1
pkg-config --exists zlib || LIBS_MISSING=1

if [ $LIBS_MISSING -eq 1 ]; then
    echo "Some development libraries are missing."
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command -v apt-get &> /dev/null; then
            if [[ -t 0 ]]; then
                echo "Detected Ubuntu/Debian. Would you like to install them now? (y/n)"
                read -r -n 1 install_confirm
                echo
                if [[ $install_confirm == "y" ]]; then
                    sudo apt-get update
                    sudo apt-get install -y libsqlite3-dev libcurl4-openssl-dev zlib1g-dev pkg-config
                fi
            else
                echo "Non-interactive environment detected. Please install missing libraries manually:"
                echo "sudo apt-get install -y libsqlite3-dev libcurl4-openssl-dev zlib1g-dev pkg-config"
            fi
        fi
    fi
fi

# 2. Check for latest Clang (optional but recommended for APX)
if command -v clang++ &> /dev/null; then
    CLANG_VER=$(clang++ --version | head -n 1)
    echo "Found $CLANG_VER"
else
    echo "Warning: clang++ not found. APX features require a modern Clang."
fi

# 3. Fetch dependencies (SDE, Pin)
echo "Setting up build directory..."
mkdir -p build
cd build
if [[ $REPAIR -eq 1 ]]; then
    rm -rf CMakeCache.txt CMakeFiles/
fi
cmake ..
make -j$(nproc) sniper
cd ..

if [[ $REPAIR -eq 1 ]] || [[ ! -d "sde_kit" ]] || [[ ! -d "pin" ]]; then
    echo "Fetching dependencies..."
    ./build/sniper fetch pin
    ./build/sniper fetch sde
fi

# 4. Run Doctor to verify
echo "Verifying environment..."
LD_LIBRARY_PATH=./xed_kit/lib:./libtorch/lib ./build/sniper doctor

# 5. Full Build
echo "Building full Sniper suite..."
cd build
make -j$(nproc)
cd ..

echo "=== Bootstrap Complete ==="
echo "You can now run simulations using: ./build/sniper sim -- <binary>"
