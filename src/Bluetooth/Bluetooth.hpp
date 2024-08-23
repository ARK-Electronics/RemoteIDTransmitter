#pragma once

#include <opendroneid.h>

#include <string>

namespace bt
{

class Bluetooth
{
public:
	Bluetooth(const std::string& device_name);

	bool initialize();

	void stop();

	// BT Legacy
	void legacy_set_advertising_data(const ODID_Message_encoded* data, uint8_t count);
	// BT LE
	void hci_le_set_extended_advertising_data(const ODID_Message_encoded* data, uint8_t count);

	void enable_legacy_advertising();
	void enable_le_extended_advertising();

	void disable_legacy_advertising();
	void disable_le_extended_advertising();

private:

	std::string generate_random_mac_address();

	void read_le_host_support();
	void write_le_host_support();

	int hci_open();
	void hci_reset();

	bool send_command(uint8_t ogf, uint16_t ocf, uint8_t* data, uint8_t length);

	// Returns status code
	uint8_t wait_for_command_acknowledged(uint16_t opcode, uint64_t timeout_ms, uint8_t* response_data = nullptr, uint8_t response_size = 0);

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

	// BT5
	uint16_t le_read_maximum_advertising_data_length();

	void le_set_extended_advertising_enable();
	void le_set_extended_advertising_disable();
	void le_read_local_supported_features();

	void hci_read_local_supported_features();

	void le_set_extended_advertising_parameters(int interval_ms);
	void le_set_advertising_set_random_address();
	void le_remove_advertising_set();

	// BT Legacy
	// -- set params
	// -- set random address
	// -- enable adv
	// -- send data
	void legacy_set_advertising_parameters(uint16_t interval_ms);
	void legacy_set_random_address();
	void legacy_set_advertising_enable();
	void legacy_set_advertising_disable();

private:
	std::string _mac {};
	std::string _device_name {};
	int _device {};
};

} // end namespace bt