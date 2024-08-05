#include "Bluetooth.hpp"
#include "print_bt_features.h"

#include <global_include.hpp>

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

namespace bt
{

Bluetooth::Bluetooth(const Settings& settings)
	: _settings(settings)
{}

void Bluetooth::stop()
{
	disable_legacy_advertising();
	disable_le_extended_advertising();
}

bool Bluetooth::initialize()
{
	LOG("Initializing Bluetooth");
	_mac = generate_random_mac_address();

	_device = hci_open();

	if (_device < 0) {
		LOG(RED_TEXT "hci_open() failed!" NORMAL_TEXT);
		return false;
	}

	hci_reset();
	le_read_local_supported_features();
	hci_read_local_supported_features();

	return true;
}

void Bluetooth::enable_legacy_advertising()
{
	LOG("Enabling Legacy advertising");
	legacy_set_advertising_parameters(300);
	legacy_set_random_address();
	legacy_set_advertising_enable();
}

void Bluetooth::enable_le_extended_advertising()
{
	LOG("Enabling LE Extended advertising");
	le_set_extended_advertising_parameters(300);
	le_set_advertising_set_random_address();
	le_set_extended_advertising_enable();
}

void Bluetooth::disable_legacy_advertising()
{
	legacy_set_advertising_disable();
	hci_reset();
}

void Bluetooth::disable_le_extended_advertising()
{
	le_set_extended_advertising_disable();
	le_remove_advertising_set();
	hci_reset();
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

void Bluetooth::hci_reset()
{
	LOG("Resetting");
	uint8_t ogf = OGF_HOST_CTL;
	uint16_t ocf = 0x0003;

	if (send_command(ogf, ocf, nullptr, 0)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 500);

		if (status) {
			LOG(RED_TEXT "Failed to reset: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::read_le_host_support()
{
	LOG("Read le host support");
	uint8_t ogf = OGF_HOST_CTL;
	uint16_t ocf = 0x006C; // Read LE Host Support

	uint8_t resp[3] = {};

	if (send_command(ogf, ocf, nullptr, 0)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100, resp, sizeof(resp));

		if (status) {
			LOG(RED_TEXT "Failed to read le host support: error 0x%x" NORMAL_TEXT, status);
		}
	}

	LOG("LE_Supported_Host: %u", resp[1]);
	LOG("Simultaneous_LE_Host: %u", resp[2]);
}

void Bluetooth::write_le_host_support()
{
	LOG("Write le host support");
	uint8_t ogf = OGF_HOST_CTL;
	uint16_t ocf = 0x006D; // Write LE Host Support

	uint8_t buf[2] = {};

	buf[0] = 1; // LE_Supported_Host
	buf[1] = 1; // Simultaneous_LE_Host

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to read le host support: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

uint16_t Bluetooth::le_read_maximum_advertising_data_length()
{
	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = 0x003A;
	uint8_t resp[3] = {};

	if (send_command(ogf, ocf, nullptr, 0)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100, resp, sizeof(resp));

		if (status) {
			LOG(RED_TEXT "Failed to read maximum advertising length: error 0x%x" NORMAL_TEXT, status);
		}
	}

	return uint16_t(resp[2] << 8) + uint16_t(resp[1]);
}

void Bluetooth::le_set_extended_advertising_disable()
{
	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = 0x0039;      // LE Set Extended Advertising Enable
	uint8_t buf[] = {
		0x00, // Enable: 0 = Advertising is disabled
		0x00 // Number_of_Sets: 0 = Disable all advertising sets
	};

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set extended advertising disable: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::le_set_extended_advertising_enable()
{
	LOG("Setting extended advertising enable");

	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = 0x0039; // LE Set Extended Advertising Enable
	// enable(1) | num_sets(1) | AdvHandle(1*num_sets) | Duration(2*num_sets) | MaxAdvEvt(1*num_sets)
	uint8_t buf[6] = {};

	buf[0] = 1; // enable
	buf[1] = 1; // num sets
	buf[2] = 0; // handle

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set extended advertising enable: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::le_remove_advertising_set()
{
	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = 0x003C; // LE Remove Advertising Set
	uint8_t set = 0;

	if (send_command(ogf, ocf, &set, sizeof(set))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to remove extended advertising set: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::le_set_advertising_set_random_address()
{
	LOG("Setting LE random address");

	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = 0x0035; // LE Set Advertising Set Random Address
	uint8_t buf[_mac.size() + 1] = {};

	buf[0] = 0; // Advertising_Handle: Used to identify an advertising set
	memcpy(&buf[1], _mac.data(), _mac.size());

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set extended advertising random address: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::le_read_local_supported_features()
{
	// Page 2479 of reference 5.2
	LOG("Reading le local supported features");
	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = 0x0003; // LE Read Local Supported Features

	if (send_command(ogf, ocf, nullptr, 0)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t resp[9] = {};
		uint8_t status = wait_for_command_acknowledged(opcode, 100, resp, sizeof(resp));

		if (status == 0) {
			LOG("Supported LE Bluetooth features:");
			print_bt_le_features(&resp[1], 8);

		} else {
			LOG(RED_TEXT "Failed to set read le local supported features: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::hci_read_local_supported_features()
{
	// Page 2226 of reference 5.2
	LOG("Reading hci local supported features");
	uint8_t ogf = OGF_INFO_PARAM;
	uint16_t ocf = 0x0003; // Read Local Supported Features

	if (send_command(ogf, ocf, nullptr, 0)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t resp[9] = {};
		uint8_t status = wait_for_command_acknowledged(opcode, 100, resp, sizeof(resp));

		if (status == 0) {
			LOG("Supported HCI Bluetooth features:");
			print_bt_hci_features(&resp[1], 8);

		} else {
			LOG(RED_TEXT "Failed to set read hci local supported features: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::le_set_extended_advertising_parameters(int interval_ms)
{
	LOG("Setting extended advertising parameters");
	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = 0x0036; // LE Set Extended Advertising Parameters
	uint8_t buf[] = { 0x00, // Advertising_Handle: Used to identify an advertising set
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
			  0x00        // Scan_Request_Notification_Enable: 0 = Scan request notifications disabled
			};

	interval_ms = std::min(std::max((1000 * interval_ms) / 625, 0x000020), 0xFFFFFF);
	buf[3] = buf[6] = interval_ms & 0xFF;
	buf[4] = buf[7] = (interval_ms >> 8) & 0xFF;
	buf[5] = buf[8] = (interval_ms >> 16) & 0xFF;

	buf[1] = 0x00;  // Advertising_Event_Properties: 0x0000 = Non-connectable and non-scannable undirected
	buf[20] = 0x03; // Primary_Advertising_PHY: 3 = Primary advertisement PHY is LE Coded
	buf[22] = 0x03; // Secondary_Advertising_PHY: 3 = Secondary advertisement PHY is LE Coded

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t resp[2] = {};
		uint8_t status = wait_for_command_acknowledged(opcode, 100, resp, sizeof(resp));

		if (status) {
			LOG(RED_TEXT "Failed to set extended advertising parameters: error 0x%x" NORMAL_TEXT, status);

		} else {
			LOG("Selected TX Power: %u dbm", resp[1]);
			// Setting the extended advertising parameters for BT4 also causes the controller to set
			// extended advertising data so we must read that response here now as well
			wait_for_command_acknowledged(htobs(cmd_opcode_pack(OGF_LE_CTL, 0x0037)), 100);
		}
	}
}

void Bluetooth::hci_le_set_extended_advertising_data(const ODID_Message_encoded* data, uint8_t count)
{
	uint16_t adv_data_hdr_size = 6; // AD len(1), Type(1), UUID(2), AppCode(1), Counter(1)
	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = 0x0037;// LE Set Extended Advertising Data
	uint8_t buf[4 + adv_data_hdr_size + ODID_MESSAGE_SIZE] = {
		0x00,   	// Advertising_Handle: Used to identify an advertising set
		0x03,   	// Operation: 3 = Complete extended advertising data
		0x01,   	// Fragment_Preference: 1 = The Controller should not fragment or should minimize fragmentation of Host advertising data
		0x00,   	// Advertising_Data_Length: The number of octets in the Advertising Data parameter
		// BEGIN ODID
		0x00,   	// The length of the following data field
		0x16,   	// 16 = GAP AD Type = "Service Data - 16-bit UUID"
		0xFA, 0xFF, // 0xFFFA = ASTM International, ASTM Remote ID
		0x0D,   	// 0x0D = AD Application Code within the ASTM address space = Open Drone ID
		0x00   		// xx = 8-bit message counter starting at 0x00 and wrapping around at 0xFF
	};

	buf[9] = count;
	buf[3] = ODID_MESSAGE_SIZE + adv_data_hdr_size; // Advertising_Data_Length
	buf[4] = ODID_MESSAGE_SIZE + adv_data_hdr_size - 1; // AD Info -- The length of the following data

	memcpy(&buf[10], (uint8_t*)data, ODID_MESSAGE_SIZE);

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set extended advertising data: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

uint8_t Bluetooth::wait_for_command_acknowledged(uint16_t opcode, uint64_t timeout_ms, uint8_t* response_data, uint8_t response_size)
{
	uint8_t status = {};
	uint64_t start_time = millis();
	bool exit_loop = false;

	// Handle nullptr for response and response_size
	uint8_t* data = nullptr;
	uint8_t size = 0;

	if (response_data) {
		data = response_data;
		size = response_size;

	} else {
		data = &status;
		size = sizeof(status);
	}

	while (!exit_loop) {
		CommandResponse response = read_command_response(opcode, data, size);
		status = data[0];

		switch (response) {
		case CommandResponse::NoData:
		case CommandResponse::InvalidPacketType:
		case CommandResponse::InvalidOpCode:
			// keep looping
			break;

		case CommandResponse::Error:
		case CommandResponse::MissingData:
		case CommandResponse::Success:
		default:
			exit_loop = true;
			break;
		}

		if (millis() - start_time > timeout_ms) {
			LOG(RED_TEXT "Timed out waiting for response" NORMAL_TEXT);
			exit_loop = true;
		}
	}

	return status;
}

Bluetooth::CommandResponse Bluetooth::read_command_response(uint16_t opcode, uint8_t* response, int response_size)
{
	unsigned char buf[HCI_MAX_EVENT_SIZE] = {};

	ssize_t bytes_read = ::read(_device, buf, sizeof(buf));

	// Check length
	if (bytes_read < 0) {
		LOG(RED_TEXT "read error" NORMAL_TEXT);
		return CommandResponse::Error;

	} else if (bytes_read == 0) {
		LOG(RED_TEXT "no data available" NORMAL_TEXT);
		return CommandResponse::NoData;
	}

	// Check packet type
	if (buf[0] != HCI_EVENT_PKT) {
		LOG(RED_TEXT "wrong packet type: %u" NORMAL_TEXT, buf[0]);
		return CommandResponse::InvalidPacketType;
	}

	// Points to hci event header struct
	// | Packet Type (1 byte) | Event Code (1 byte) | Parameter Total Length (1 byte) | Event Parameters (Variable) |
	hci_event_hdr* hdr = (hci_event_hdr*)(buf + HCI_TYPE_LEN);
	ssize_t packet_length = hdr->plen + HCI_TYPE_LEN + HCI_EVENT_HDR_SIZE;

	if (bytes_read < packet_length) {
		LOG(RED_TEXT "missing bytes" NORMAL_TEXT);
		return CommandResponse::MissingData;
	}

	switch (hdr->evt) {
	case EVT_CMD_COMPLETE: {
		evt_cmd_complete* cc = (evt_cmd_complete*)(&buf[1 + HCI_EVENT_HDR_SIZE]);

		if (cc->opcode != opcode) {
			LOG(RED_TEXT "Wrong opcode: %u expected %u" NORMAL_TEXT, cc->opcode, opcode);
			LOG(RED_TEXT "ogf: 0x%x expected 0x%x" NORMAL_TEXT, cmd_opcode_ogf(cc->opcode), cmd_opcode_ogf(opcode));
			LOG(RED_TEXT "ocf: 0x%x expected 0x%x" NORMAL_TEXT, cmd_opcode_ocf(cc->opcode), cmd_opcode_ocf(opcode));
			return CommandResponse::InvalidOpCode;
		}

		// Copy param values into response data
		ssize_t params_offset = HCI_TYPE_LEN + HCI_EVENT_HDR_SIZE + EVT_CMD_COMPLETE_SIZE;
		memcpy(response, &buf[params_offset], response_size);

		switch (response[0]) {
		case 0x00:
			return CommandResponse::Success;

		case 0x07:
			LOG(RED_TEXT "Memory capacity exceed" NORMAL_TEXT);
			break;

		case 0xC:
			LOG(RED_TEXT "Command disallowed" NORMAL_TEXT);
			break;

		case 0x12:
			LOG(RED_TEXT "Invalid HCI command parameter" NORMAL_TEXT);
			break;

		default:
			LOG(RED_TEXT "Unhandled error status: 0x%x" NORMAL_TEXT, response[0]);
			break;
		}

		break;
	}

	case EVT_CMD_STATUS: {
		LOG("TODO: Received event command status");
		break;
	}

	default:
		LOG("Received unknown event: 0x%X", hdr->evt);
		break;
	}

	return CommandResponse::Error;
}

bool Bluetooth::send_command(uint8_t ogf, uint16_t ocf, uint8_t* data, uint8_t length)
{
	if (hci_send_cmd(_device, ogf, ocf, length, data) < 0) {
		LOG("send_command failed (did you use sudo?");
		return false;
	}

	return true;
}

} // end namespace bt