#pragma once

extern "C" {
#include <opendroneid.h>
}

#include <string>

namespace bt
{

struct Settings {
	std::string hci_device_name;
	bool use_bt4 {};
	bool use_bt5 {};
	int bt4_set {};
	int bt5_set {};
};

class Bluetooth
{
public:
	Bluetooth(const Settings& settings);

	void initialize();

	void stop();

	void hci_le_set_extended_advertising_data_pack(uint8_t set, const struct ODID_MessagePack_encoded* pack_enc, uint8_t msg_counter);

private:

	std::string generate_random_mac_address();

	// Returns device descriptor
	int hci_open();
	void hci_reset();
	void hci_stop_transmit();

	void send_command(uint8_t ogf, uint16_t ocf, uint8_t* data, int length);

	// void hci_le_set_extended_advertising_data(uint8_t set, const union ODID_Message_encoded* encoded, uint8_t msg_counter);

	void hci_le_set_advertising_disable();
	void hci_le_set_extended_advertising_disable();
	void hci_le_remove_advertising_set(uint8_t set);
	void hci_le_read_local_supported_features();

	void hci_le_set_extended_advertising_parameters(uint8_t set, int interval_ms, bool long_range);
	void hci_le_set_advertising_set_random_address(uint8_t set);
	void hci_le_set_extended_advertising_enable();

private:
	Settings _settings {};
	int _dd = 0;

	std::string _mac;

};

} // end namespace bt