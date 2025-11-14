#!/bin/bash
# init.sh - Script to set up and uninstall an i386-elf cross-compiler

set -e  # Exit on error

# Define the target and installation directories
TARGET="i386-elf"
PREFIX="$HOME/opt/cross"
PATH="$PREFIX/bin:$PATH"
SRC_DIR="$HOME/src"
BINUTILS_VERSION="2.40"
GCC_VERSION="14.2.0"

# Check if script is run as root (not required)
if [ "$EUID" -eq 0 ]; then
    echo "Warning: Running as root is not recommended."
fi

# Function to install dependencies
install_dependencies() {
    echo "Installing dependencies..."
    sudo apt update
    sudo apt install -y build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo \
                        grub-pc grub-efi-amd64-signed libisl-dev wget
}

# Function to download sources
download_sources() {
    mkdir -p "$SRC_DIR"
    cd "$SRC_DIR"

    echo "Downloading Binutils..."
    [ -f "binutils-$BINUTILS_VERSION.tar.gz" ] || wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz"

    echo "Downloading GCC..."
    [ -f "gcc-$GCC_VERSION.tar.gz" ] || wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"

    echo "Extracting sources..."
    tar -xf "binutils-$BINUTILS_VERSION.tar.gz"
    tar -xf "gcc-$GCC_VERSION.tar.gz"
}

# Function to build and install Binutils
build_binutils() {
    echo "Building Binutils..."
    cd "$SRC_DIR"
    mkdir -p build-binutils
    cd build-binutils
    ../binutils-$BINUTILS_VERSION/configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --disable-werror
    make -j$(nproc)
    make install
}

# Function to build and install GCC
build_gcc() {
    echo "Building GCC..."
    cd "$SRC_DIR"
    mkdir -p build-gcc
    cd build-gcc
    ../gcc-$GCC_VERSION/configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers --disable-multilib --disable-libssp
    make -j$(nproc) all-gcc
    make -j$(nproc) all-target-libgcc
    make install-gcc
    make install-target-libgcc
}

# Function to update PATH
update_path() {
    if ! grep -q "$PREFIX/bin" ~/.bashrc; then
        echo "export PATH=\"$PREFIX/bin:\$PATH\"" >> ~/.bashrc
        echo "Path updated in ~/.bashrc. Run 'source ~/.bashrc' to apply changes."
    fi
}

# Function to uninstall
uninstall() {
    echo "Uninstalling i386-elf cross-compiler..."
    rm -rf "$PREFIX"
    rm -rf "$SRC_DIR/build-binutils" "$SRC_DIR/build-gcc"
    sed -i "/$PREFIX\/bin/d" ~/.bashrc
    echo "Uninstallation complete."
}

# Main menu
if [ "$1" == "uninstall" ]; then
    uninstall
    exit 0
fi

install_dependencies
download_sources
build_binutils
build_gcc
update_path

echo "Cross-compiler for $TARGET installed in $PREFIX"
