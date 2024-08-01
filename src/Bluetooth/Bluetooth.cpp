#include "Bluetooth.hpp"
#include <global_include.hpp>

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

#include <unistd.h>

extern "C" {
#include <lib/bluetooth.h>
#include <lib/hci.h>
#include <lib/hci_lib.h>

#include "print_bt_features.h"
}

namespace bt
{

Bluetooth::Bluetooth(const Settings& settings)
	: _settings(settings)
{}

void Bluetooth::initialize()
{
	LOG("Initializing Bluetooth");
	_mac = generate_random_mac_address();

	_dd = hci_open();

	if (_dd < 0) {
		LOG(RED_TEXT "hci_open() failed!" NORMAL_TEXT);
		// TODO: handle error downstream
		return;
	}

	this->hci_reset();
	this->hci_stop_transmit();

	this->hci_le_read_local_supported_features();

	// BT4
	this->hci_le_set_extended_advertising_parameters(_settings.bt4_set, 300, false);
	this->hci_le_set_advertising_set_random_address(_settings.bt4_set);

	// BT5
	this->hci_le_set_extended_advertising_parameters(_settings.bt5_set, 300, true);
	this->hci_le_set_advertising_set_random_address(_settings.bt5_set);

	this->hci_le_set_extended_advertising_enable();
}

void Bluetooth::hci_reset()
{
	uint8_t ogf = OGF_HOST_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = OCF_RESET;
	send_command(ogf, ocf, nullptr, 0);
}

void Bluetooth::hci_stop_transmit()
{
	hci_le_set_advertising_disable();
	hci_le_set_extended_advertising_disable();
	hci_le_remove_advertising_set(_settings.bt4_set);
	hci_le_remove_advertising_set(_settings.bt5_set);
}

void Bluetooth::stop()
{
	// TODO:
}

int Bluetooth::hci_open()
{
	struct hci_filter filter; // Host Controller Interface filter

	int device_id = hci_devid("hci0");

	if (device_id < 0) {
		device_id = hci_get_route(NULL);
	}

	int device_descriptor = hci_open_dev(device_id);

	if (device_descriptor < 0) {
		LOG(RED_TEXT "Device open failed" NORMAL_TEXT);
		// exit(EXIT_FAILURE);
		return -1;
	}

	hci_filter_clear(&filter);
	hci_filter_set_ptype(HCI_EVENT_PKT, &filter);
	hci_filter_all_events(&filter);

	if (setsockopt(device_descriptor, SOL_HCI, HCI_FILTER, &filter, sizeof(filter)) < 0) {
		LOG(RED_TEXT "CI filter setup failed" NORMAL_TEXT);
		return -1;
	}

	return device_descriptor;
}

void Bluetooth::hci_le_set_extended_advertising_data_pack(uint8_t set, const struct ODID_MessagePack_encoded* pack_enc, uint8_t msg_counter)
{
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x37;      // Opcode Command Field: LE Set Extended Advertising Data
	uint8_t buf[10 + 3 + ODID_PACK_MAX_MESSAGES * ODID_MESSAGE_SIZE] = {
		0x00,   // Advertising_Handle: Used to identify an advertising set
		0x03,   // Operation: 3 = Complete extended advertising data
		0x01,   // Fragment_Preference: 1 = The Controller should not fragment or should minimize fragmentation of Host advertising data
		0x1F,   // Advertising_Data_Length: The number of octets in the Advertising Data parameter
		0x1E,   // The length of the following data field
		0x16,   // 16 = GAP AD Type = "Service Data - 16-bit UUID"
		0xFA, 0xFF, // 0xFFFA = ASTM International, ASTM Remote ID
		0x0D,   // 0x0D = AD Application Code within the ASTM address space = Open Drone ID
		0x00,   // xx = 8-bit message counter starting at 0x00 and wrapping around at 0xFF
		0x00
	}; // 3 + ODID_PACK_MAX_MESSAGES*ODID_MESSAGE_SIZE byte Drone ID message pack data
	buf[0] = set;
	buf[9] = msg_counter;

	int amount = pack_enc->MsgPackSize;
	buf[3] = 1 + 5 + 3 + amount * ODID_MESSAGE_SIZE;
	buf[4] = 5 + 3 + amount * ODID_MESSAGE_SIZE;

	for (int i = 0; i < 3 + amount * ODID_MESSAGE_SIZE; i++)
		buf[10 + i] = ((char*) pack_enc)[i];

	send_command(ogf, ocf, buf, sizeof(buf));
}

void Bluetooth::hci_le_set_advertising_disable()
{
	uint8_t ogf = OGF_LE_CTL;   // Opcode Group Field. LE Controller Commands
	uint16_t ocf = OCF_LE_SET_ADVERTISE_ENABLE;
	uint8_t data = 0x00;        // Enable: 0 = Advertising is disabled (default)
	send_command(ogf, ocf, &data, 1);
}

void Bluetooth::hci_le_set_extended_advertising_disable()
{
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x39;      // Opcode Command Field: LE Set Extended Advertising Enable
	uint8_t buf[] = {
		0x00,   // Enable: 0 = Advertising is disabled
		0x00 	// Number_of_Sets: 0 = Disable all advertising sets
	};
	send_command(ogf, ocf, buf, sizeof(buf));
}

// Max two advertising sets is supported by this function currently
void Bluetooth::hci_le_set_extended_advertising_enable()
{
	LOG("enabling advertising");

	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x39;      // Opcode Command Field: LE Set Extended Advertising Enable
	uint8_t buf[] = { 0x01,   // Enable: 1 = Advertising is enabled
			  0x01,   // Number_of_Sets: Number of advertising sets to enable or disable
			  0x00, 0x00,   // Advertising_Handle[i]:
			  0x00, 0x00,   // Duration[i]: 0 = No advertising duration. Advertising to continue until the Host disables it
			  0x00, 0x00
			}; // Max_Extended_Advertising_Events[i]: 0 = No maximum number of advertising events

	if (_settings.use_bt4 && _settings.use_bt5) {
		buf[1] = 2;
		buf[2] = _settings.bt4_set;
		buf[3] = _settings.bt5_set;

	} else if (_settings.use_bt4) {
		buf[2] = _settings.bt4_set;

	} else if (_settings.use_bt5) {
		buf[2] = _settings.bt5_set;
	}

	send_command(ogf, ocf, buf, sizeof(buf));
}

void Bluetooth::hci_le_remove_advertising_set(uint8_t set)
{
	uint8_t ogf = OGF_LE_CTL;   // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x3C;        // Opcode Command Field: LE Remove Advertising Set
	send_command(ogf, ocf, &set, 1);
}

void Bluetooth::hci_le_read_local_supported_features()
{
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = OCF_LE_READ_LOCAL_SUPPORTED_FEATURES;
	send_command(ogf, ocf, nullptr, 0);
}

void Bluetooth::hci_le_set_extended_advertising_parameters(uint8_t set, int interval_ms, bool long_range)
{
	uint8_t ogf = OGF_LE_CTL;     // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x36;          // Opcode Command Field: LE Set Extended Advertising Parameters
	uint8_t buf[] = { 0x00,       // Advertising_Handle: Used to identify an advertising set
			  0x10, 0x00, // Advertising_Event_Properties: 0x0010 = Use legacy advertising PDUs + Non-connectable and non-scannable undirected
			  0x00, 0x08, 0x00, // Primary_Advertising_Interval_Min: N * 0.625 ms. 0x000800 = 1280 ms
			  0x00, 0x08, 0x00, // Primary_Advertising_Interval_Max: N * 0.625 ms. 0x000800 = 1280 ms
			  0x07,       // Primary_Advertising_Channel_Map: 7 = all three channels enabled
			  0x01,       // Own_Address_Type: 1 = Random Device Address
			  0x00,       // Peer_Address_Type: 0 = Public Device Address or Public Identity Address
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Peer_Address
			  0x00,       // Advertising_Filter_Policy: 0 = Process scan and connection requests from all devices (i.e., the White List is not in use)
			  0x7F,       // Advertising_Tx_Power: 0x7F = Host has no preference
			  0x01,       // Primary_Advertising_PHY: 1 = Primary advertisement PHY is LE 1M
			  0x00,       // Secondary_Advertising_Max_Skip: 0 = AUX_ADV_IND shall be sent prior to the next advertising event
			  0x01,       // Secondary_Advertising_PHY: 1 = Secondary advertisement PHY is LE 1M
			  0x00,       // Advertising_SID: 0 = Value of the Advertising SID subfield in the ADI field of the PDU
			  0x00
			};     // Scan_Request_Notification_Enable: 0 = Scan request notifications disabled
	buf[0] = set;

	interval_ms = std::min(std::max((1000 * interval_ms) / 625, 0x000020), 0xFFFFFF);
	buf[3] = buf[6] = interval_ms & 0xFF;
	buf[4] = buf[7] = (interval_ms >> 8) & 0xFF;
	buf[5] = buf[8] = (interval_ms >> 16) & 0xFF;

	if (long_range) {
		buf[1] = 0x00;  // Advertising_Event_Properties: 0x0000 = Non-connectable and non-scannable undirected
		buf[20] = 0x03; // Primary_Advertising_PHY: 3 = Primary advertisement PHY is LE Coded
		buf[22] = 0x03; // Secondary_Advertising_PHY: 3 = Secondary advertisement PHY is LE Coded
	}

	send_command(ogf, ocf, buf, sizeof(buf));
}

void Bluetooth::hci_le_set_advertising_set_random_address(uint8_t set)
{
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x35;      // Opcode Command Field: LE Set Advertising Set Random Address
	uint8_t buf[_mac.size() + 1] = {0x00};   // Advertising_Handle: Used to identify an advertising set

	buf[0] = set;
	memcpy(&buf[1], _mac.data(), _mac.size());

	send_command(ogf, ocf, buf, sizeof(buf));
}

bool Bluetooth::send_command(uint8_t ogf, uint16_t ocf, uint8_t* data, int length)
{

	LOG("send_command\n ogf: %u ocf: %u length: %i", ogf, ocf, length);

	int result = hci_send_cmd(_dd, ogf, ocf, length, data);

	if (result < 0) {
		// TODO: handle error
		LOG("send_command failed: result: %d", result);
		return false;
	}

	unsigned char buf[HCI_MAX_EVENT_SIZE] = { 0 }, *ptr;
	uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
	hci_event_hdr* hdr = (hci_event_hdr*)(buf + 1);
	evt_cmd_complete* cc;
	ssize_t len;

	while ((len = ::read(_dd, buf, sizeof(buf))) < 0) {
		if (errno == EAGAIN || errno == EINTR) {
			continue;
		}

		LOG("While loop for reading event failed");
		return false;
	}

	hdr = (hci_event_hdr*)(buf + 1);
	ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
	len -= (1 + HCI_EVENT_HDR_SIZE);

	switch (hdr->evt) {
	case EVT_CMD_COMPLETE: {
		cc = (evt_cmd_complete*) ptr;

		if (cc->opcode != opcode) {
			LOG("Received event with invalid opcode 0x%X. Expected 0x%X", cc->opcode, opcode);
		}

		ptr += EVT_CMD_COMPLETE_SIZE;
		len -= EVT_CMD_COMPLETE_SIZE;

		uint8_t rparam[10] = { 0 };
		memcpy(rparam, ptr, len);

		if (rparam[0] && ocf != 0x3C) {
			LOG("Command 0x%X returned error 0x%X", ocf, rparam[0]);
		}

		if (ocf == OCF_LE_READ_LOCAL_SUPPORTED_FEATURES) {
			LOG("Supported Low Energy Bluetooth features:");
			print_bt_le_features(&rparam[1]);
		}

		if (ocf == 0x36) {
			LOG("The transmit power is set to %d dBm", (unsigned char) rparam[1]);
		}

		fflush(stdout);
		break;
	}

	default:
		LOG("Received unknown event: 0x%X", hdr->evt);
		break;
	}

	return true;
}

std::string Bluetooth::generate_random_mac_address()
{
	auto mac = std::string(6, 'x');

	// Seed for random number generation
	std::srand(std::time(0));

	for (auto& e : mac) {
		e = std::rand() % 255;
	}

	// set to Bluetooth Random Static Address, see https://www.novelbits.io/bluetooth-address-privacy-ble/
	// or section 1.3 of the Bluetooth specification (5.3)
	mac[0] |= 0xC0;

	return mac;
}

} // end namespace bt