#!/bin/bash
set -euo pipefail

THIS_DIR="$(dirname "$(realpath "$BASH_SOURCE")")"

# Build
pushd "$THIS_DIR" > /dev/null
make
popd > /dev/null

# Install binary
sudo mkdir -p /opt/ark/bin
sudo cp "$THIS_DIR/build/rid-transmitter" /opt/ark/bin/

# Give the binary capabilities for Bluetooth
sudo setcap 'cap_net_raw,cap_net_admin+eip' /opt/ark/bin/rid-transmitter

# Install default config
sudo mkdir -p /opt/ark/share/rid-transmitter
sudo cp "$THIS_DIR/config.toml" /opt/ark/share/rid-transmitter/
