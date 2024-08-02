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
};

enum class BluetoothMode {
	BT4,
	BT5,
};

static constexpr uint8_t BT4_SET = 0;
static constexpr uint8_t BT5_SET = 1;

class Bluetooth
{
public:
	Bluetooth(const Settings& settings);

	bool initialize();

	void stop();

	void hci_le_set_extended_advertising_data(BluetoothMode mode, const ODID_Message_encoded* data, uint8_t count);

	void hci_le_set_extended_advertising_data(BluetoothMode mode, const struct ODID_MessagePack_encoded* encoded_data, uint8_t msg_counter);

private:

	std::string generate_random_mac_address();

	// Returns device descriptor
	int hci_open();
	void hci_reset();
	void hci_stop_transmit();

	bool send_command(uint8_t ogf, uint16_t ocf, uint8_t* data, int length);

	// Returns bytes read
	enum class CommandResponse {
		Error,
		NoData,
		MissingData,
		InvalidPacketType,
		InvalidOpCode,
		Success,
	};
	CommandResponse read_command_response(uint16_t opcode, uint8_t* response, int response_size);

	uint16_t hci_le_read_maximum_advertising_data_length();

	void hci_le_set_advertising_disable();
	void hci_le_set_extended_advertising_disable();
	void hci_le_remove_advertising_set(BluetoothMode mode);
	void hci_le_read_local_supported_features();

	void hci_le_set_extended_advertising_parameters(BluetoothMode mode, int interval_ms);
	void hci_le_set_advertising_set_random_address(BluetoothMode mode);
	void hci_le_set_extended_advertising_enable();

private:
	Settings _settings {};
	int _dd = 0;

	std::string _mac;

	uint16_t _max_adv_data_len = 0;

};

} // end namespace bt