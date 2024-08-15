#!/bin/bash

# Setup XDG default paths
DEFAULT_XDG_CONF_HOME="$HOME/.config"
DEFAULT_XDG_DATA_HOME="$HOME/.local/share"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$DEFAULT_XDG_CONF_HOME}"
export XDG_DATA_HOME="${XDG_DATA_HOME:-$DEFAULT_XDG_DATA_HOME}"

sudo true

pushd .

echo "Installing remote-id"

# clean up legacy if it exists
sudo systemctl stop rid-transmitter &>/dev/null
sudo systemctl disable rid-transmitter &>/dev/null
sudo rm -rf ~/code/RemoteIDTransmitter &>/dev/null

# Install dependencies
sudo apt-get install -y astyle bluez bluez-tools libbluetooth-dev

# Clone, build, and install
git clone --recurse-submodules --depth=1 --shallow-submodules https://github.com/ARK-Electronics/RemoteIDTransmitter.git ~/code/RemoteIDTransmitter
cd ~/code/RemoteIDTransmitter
make install

# Give the binary capabilities so it doesn't require root
sudo setcap 'cap_net_raw,cap_net_admin+eip' /usr/local/bin/rid-transmitter

sudo ldconfig
popd

# Install the service
sudo cp rid-transmitter.service $XDG_CONFIG_HOME/systemd/user/
systemctl --user daemon-reload
systemctl --user enable rid-transmitter.service
systemctl --user restart rid-transmitter.service
