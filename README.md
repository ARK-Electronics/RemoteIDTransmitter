# RemoteIDTransmitter

https://youtu.be/VN_R9-af3zg

### Introduction
Transmit RemoteID messages via Bluetooth on Linux. The software rapidly toggles between standard (legacy) advertisements and extended (LE) advertisements to meet the requirements of simultaneous broadcast as specified in ASTM3411. The `Basic ID`, `Location/Vector` and `System` messages are sent individually. The data is received via MAVLink as OPEN_DRONE_ID_BASIC_ID, OPEN_DRONE_ID_LOCATION, and OPEN_DRONE_ID_SYSTEM messages and encoded into the bluetooth advertisement data.

You can decode these messages by installing the [Wireshark dissector plugin](https://github.com/opendroneid/wireshark-dissector) and using a supported BLE sniffer such as the [NR52840 Dongle](https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dongle). You can also use a mobile app like [Drone Scanner](https://play.google.com/store/apps/details?id=cz.dronetag.dronescanner&hl=en_US) to validate the data being broadcast.

If you are new to RemoteID you will need to familiarize yourself with ASTM3411 and ASTM3586. This software does not guarantee compliance. Testing and validation is the responsibility of the airframe manufacturer. If you have any issues or questions please do not hesitate to ask.

---
### Running the application
Pre-requisites
```
sudo apt-get install -y \
	astyle \
	bluez \
	bluez-tools \
	libbluetooth-dev
```
Build
```
make
```
Run
```
sudo ./build/rid-transmitter --mavlink-url udp://0.0.0.0:14540
```
You must be root to use bluetooth. Or you can give the binary capabilities to use bluetooth
```
sudo setcap 'cap_net_raw,cap_net_admin+eip' build/rid-transmitter
```

---

### Tested hardware
- [Advantech AIW-170BQ-001](https://buy.advantech.com/AIW-170BQ-001-AIW-170BQ-001/AIW-170BQ-001/model-AIW-170BQ-001.htm)
- [Intel AX210](https://www.intel.com/content/www/us/en/products/sku/204836/intel-wifi-6e-ax210-gig/specifications.html)

### Notes

- We do not use message packs and instead send messages individually due to limitations with advertisement data packet size that varies between hardware.

- BlueZ cannot simultaneously broadcast standard and extended advertisement, so we must toggle between the two modes.

- The minimum advertising interval is 20ms per bluetooth spec, so we space advertisements 30ms apart.

- We rely on the mavlink data to contain accurate information. We always publish the RemoteID messages and do not check that all the necessary data is present before transmitting.

- If things aren't working use `sudo btmon` to help debug.

- Check if your device shows up as an hci device using `hciconfig`.
