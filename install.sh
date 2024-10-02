#!/bin/bash

# Setup XDG default paths
DEFAULT_XDG_CONF_HOME="$HOME/.config"
DEFAULT_XDG_DATA_HOME="$HOME/.local/share"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$DEFAULT_XDG_CONF_HOME}"
export XDG_DATA_HOME="${XDG_DATA_HOME:-$DEFAULT_XDG_DATA_HOME}"
THIS_DIR="$(dirname "$(realpath "$BASH_SOURCE")")"

sudo true

# Install dependencies
sudo apt-get install -y astyle bluez bluez-tools libbluetooth-dev

make

# Setup project directory
sudo cp $THIS_DIR/build/rid-transmitter ~/.local/bin
mkdir -p $XDG_DATA_HOME/rid-transmitter
cp $THIS_DIR/config.toml $XDG_DATA_HOME/rid-transmitter/

# Give the binary capabilities so it doesn't require root
sudo setcap 'cap_net_raw,cap_net_admin+eip' ~/.local/bin/rid-transmitter

# Modify config file if ENV variables are set
CONFIG_FILE="$XDG_DATA_HOME/rid-transmitter/config.toml"

if [ -n "$MANUFACTURER_CODE" ]; then
	echo "Setting manufacturer_code to: $MANUFACTURER_CODE"
	sed -i "s/^manufacturer_code = \".*\"/manufacturer_code = \"$MANUFACTURER_CODE\"/" "$CONFIG_FILE"
fi

if [ -n "$SERIAL_NUMBER" ]; then
	echo "Setting serial_number to: $SERIAL_NUMBER"
	sed -i "s/^serial_number = \".*\"/serial_number = \"$SERIAL_NUMBER\"/" "$CONFIG_FILE"
fi
