#include "Bluetooth.hpp"
#include <global_include.hpp>

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

#include <unistd.h>

extern "C" {
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "print_bt_features.h"
}

namespace bt
{

Bluetooth::Bluetooth(const Settings& settings)
	: _settings(settings)
{}

void Bluetooth::stop()
{
	// TODO:
	hci_stop_transmit();
}

bool Bluetooth::initialize()
{
	LOG("Initializing Bluetooth");
	_mac = generate_random_mac_address();

	_dd = hci_open();

	if (_dd < 0) {
		LOG(RED_TEXT "hci_open() failed!" NORMAL_TEXT);
		return false;
	}

	this->hci_reset();

	this->hci_le_read_local_supported_features();

	_max_adv_data_len = this->hci_le_read_maximum_advertising_data_length();

	// BT4
	if (_settings.use_bt4) {
		this->hci_le_set_extended_advertising_parameters(BluetoothMode::BT4, 300);
		this->hci_le_set_advertising_set_random_address(BluetoothMode::BT4);
	}

	// BT5
	if (_settings.use_bt5) {
		this->hci_le_set_extended_advertising_parameters(BluetoothMode::BT5, 300);
		this->hci_le_set_advertising_set_random_address(BluetoothMode::BT5);
	}

	this->hci_le_set_extended_advertising_enable();
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

void Bluetooth::hci_stop_transmit()
{
	hci_le_set_advertising_disable();
	hci_le_set_extended_advertising_disable();
	hci_le_remove_advertising_set(BluetoothMode::BT4);
	hci_le_remove_advertising_set(BluetoothMode::BT5);
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

void Bluetooth::hci_reset()
{
	LOG("Resetting");
	uint8_t ogf = OGF_HOST_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x0003;

	if (send_command(ogf, ocf, nullptr, 0)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = {};
		CommandResponse response = read_command_response(opcode, &status, sizeof(status));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to reset: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

uint16_t Bluetooth::hci_le_read_maximum_advertising_data_length()
{
	uint8_t ogf = OGF_LE_CTL;   // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x003A;
	uint8_t data[3] = {};

	if (send_command(ogf, ocf, nullptr, 0)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		CommandResponse response = read_command_response(opcode, data, sizeof(data));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to set extended advertising data: error 0x%x" NORMAL_TEXT, data[0]);
		}
	}

	return uint16_t(data[2] << 8) + uint16_t(data[1]);
}

void Bluetooth::hci_le_set_advertising_disable()
{
	uint8_t ogf = OGF_LE_CTL;   // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x000A;
	uint8_t data = 0x00;        // Enable: 0 = Advertising is disabled (default)

	if (send_command(ogf, ocf, &data, 1)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = {};
		CommandResponse response = read_command_response(opcode, &status, sizeof(status));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to set extended advertising data: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::hci_le_set_extended_advertising_disable()
{
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x0039;      // Opcode Command Field: LE Set Extended Advertising Enable
	uint8_t buf[] = {
		0x00,   // Enable: 0 = Advertising is disabled
		0x00 	// Number_of_Sets: 0 = Disable all advertising sets
	};

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = {};
		CommandResponse response = read_command_response(opcode, &status, sizeof(status));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to set extended advertising disable: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

// Max two advertising sets is supported by this function currently
void Bluetooth::hci_le_set_extended_advertising_enable()
{
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x0039;      // Opcode Command Field: LE Set Extended Advertising Enable

	uint8_t buf[9] = {};

	// enable(1) | num_sets(1) | AdvHandle(1*num_sets) | Duration(2*num_sets) | MaxAdvEvt(1*num_sets)

	uint8_t length = 0;

	buf[0] = 1; // enable

	if (_settings.use_bt4 && _settings.use_bt5) {
		LOG("enabling advertising for BT4 and BT5");
		buf[1] = 2; // num sets
		buf[2] = BT4_SET;
		buf[3] = BT5_SET;
		length = sizeof(buf);

	} else if (_settings.use_bt4) {
		LOG("enabling advertising for BT4");
		buf[1] = 1; // num sets
		buf[2] = BT4_SET;
		length = 6;

	} else if (_settings.use_bt5) {
		LOG("enabling advertising for BT5");
		buf[1] = 1; // num sets
		buf[2] = BT5_SET;
		length = 6;
	}

	if (send_command(ogf, ocf, buf, length)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = {};
		CommandResponse response = read_command_response(opcode, &status, sizeof(status));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to set extended advertising enable: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::hci_le_remove_advertising_set(BluetoothMode mode)
{
	uint8_t ogf = OGF_LE_CTL;   // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x003C;        // Opcode Command Field: LE Remove Advertising Set
	uint8_t set = mode == BluetoothMode::BT4 ? BT4_SET : BT5_SET;

	if (send_command(ogf, ocf, &set, sizeof(set))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = {};
		CommandResponse response = read_command_response(opcode, &status, sizeof(status));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to remove extended advertising set: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::hci_le_set_advertising_set_random_address(BluetoothMode mode)
{
	LOG("Setting random address: %s", mode == BluetoothMode::BT5 ? "BT5" : "BT4");

	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x0035;      // Opcode Command Field: LE Set Advertising Set Random Address
	uint8_t buf[_mac.size() + 1] = {};   // Advertising_Handle: Used to identify an advertising set

	buf[0] = mode == BluetoothMode::BT4 ? BT4_SET : BT5_SET;
	memcpy(&buf[1], _mac.data(), _mac.size());

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t data[10] = {};
		CommandResponse response = read_command_response(opcode, data, sizeof(data));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to set extended advertising random address: error 0x%x" NORMAL_TEXT, data[0]);
		}
	}
}

void Bluetooth::hci_le_read_local_supported_features()
{
	LOG("Reading local supported features");
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x0003;

	if (send_command(ogf, ocf, nullptr, 0)) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t data[9] = {};
		CommandResponse response = read_command_response(opcode, data, sizeof(data));

		if (response == CommandResponse::Success) {
			LOG("Supported Low Energy Bluetooth features:");
			print_bt_le_features(&data[1], 8);

		} else {
			LOG(RED_TEXT "Failed to set read local supported features: error 0x%x" NORMAL_TEXT, data[0]);
		}
	}
}

void Bluetooth::hci_le_set_extended_advertising_parameters(BluetoothMode mode, int interval_ms)
{
	LOG("Setting extended advertising parameters: %s", mode == BluetoothMode::BT5 ? "BT5" : "BT4");
	uint8_t ogf = OGF_LE_CTL;     // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x0036;          // Opcode Command Field: LE Set Extended Advertising Parameters
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
	buf[0] = mode == BluetoothMode::BT4 ? BT4_SET : BT5_SET;

	interval_ms = std::min(std::max((1000 * interval_ms) / 625, 0x000020), 0xFFFFFF);
	buf[3] = buf[6] = interval_ms & 0xFF;
	buf[4] = buf[7] = (interval_ms >> 8) & 0xFF;
	buf[5] = buf[8] = (interval_ms >> 16) & 0xFF;

	if (mode == BluetoothMode::BT5) {
		buf[1] = 0x00;  // Advertising_Event_Properties: 0x0000 = Non-connectable and non-scannable undirected
		buf[20] = 0x03; // Primary_Advertising_PHY: 3 = Primary advertisement PHY is LE Coded
		buf[22] = 0x03; // Secondary_Advertising_PHY: 3 = Secondary advertisement PHY is LE Coded
	}

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t data[10] = {};
		CommandResponse response = read_command_response(opcode, data, sizeof(data));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to set extended advertising parameters: error 0x%x" NORMAL_TEXT, data[0]);
		}

		// Setting the extended advertising parameters for BT legacy also causes the controller to set
		// extended advertising data so we must read that response here now as well
		if (mode == BluetoothMode::BT4) {
			read_command_response(htobs(cmd_opcode_pack(OGF_LE_CTL, 0x0037)), data, 1);
		}
	}
}

void Bluetooth::hci_le_set_extended_advertising_data(BluetoothMode mode, const ODID_Message_encoded* data, uint8_t count)
{
	uint16_t adv_data_hdr_size = 6; // AD len(1), Type(1), UUID(2), AppCode(1), Counter(1)
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x0037;      // Opcode Command Field: LE Set Extended Advertising Data
	uint8_t buf[4 + adv_data_hdr_size + ODID_MESSAGE_SIZE] = {
		0x00,   // Advertising_Handle: Used to identify an advertising set
		0x03,   // Operation: 3 = Complete extended advertising data
		0x01,   // Fragment_Preference: 1 = The Controller should not fragment or should minimize fragmentation of Host advertising data
		0x1F,   // Advertising_Data_Length: The number of octets in the Advertising Data parameter
		// BEGIN ODID
		// AD Info
		0x1E,   // The length of the following data field
		0x16,   // 16 = GAP AD Type = "Service Data - 16-bit UUID"
		0xFA, 0xFF, // 0xFFFA = ASTM International, ASTM Remote ID
		// App code
		0x0D,   // 0x0D = AD Application Code within the ASTM address space = Open Drone ID
		// Msg counter
		0x00   // xx = 8-bit message counter starting at 0x00 and wrapping around at 0xFF
	};

	buf[0] = mode == BluetoothMode::BT4 ? BT4_SET : BT5_SET;
	buf[9] = count;

	buf[3] = ODID_MESSAGE_SIZE + adv_data_hdr_size;
	buf[4] = ODID_MESSAGE_SIZE + adv_data_hdr_size - 1;

	memcpy(&buf[10], (uint8_t*)data, ODID_MESSAGE_SIZE);

	// Send off the data
	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = {};
		CommandResponse response = read_command_response(opcode, &status, sizeof(status));

		if (response != CommandResponse::Success) {
			LOG(RED_TEXT "Failed to set extended advertising data: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::hci_le_set_extended_advertising_data(BluetoothMode mode, const struct ODID_MessagePack_encoded* encoded_data,
		uint8_t msg_counter)
{
	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = 0x0037;      // Opcode Command Field: LE Set Extended Advertising Data
	uint8_t buf[10 + 3 + ODID_PACK_MAX_MESSAGES * ODID_MESSAGE_SIZE] = {
		0x00,   // Advertising_Handle: Used to identify an advertising set
		0x03,   // Operation: 3 = Complete extended advertising data
		0x00,   // Fragment_Preference: 1 = The Controller should not fragment or should minimize fragmentation of Host advertising data
		0x1F,   // Advertising_Data_Length: The number of octets in the Advertising Data parameter
		// BEGIN ODID
		// AD Info
		0x1E,   // The length of the following data field
		0x16,   // 16 = GAP AD Type = "Service Data - 16-bit UUID"
		0xFA, 0xFF, // 0xFFFA = ASTM International, ASTM Remote ID
		// App code
		0x0D,   // 0x0D = AD Application Code within the ASTM address space = Open Drone ID
		// Msg counter
		0x00   // xx = 8-bit message counter starting at 0x00 and wrapping around at 0xFF
	};

	buf[0] = mode == BluetoothMode::BT4 ? BT4_SET : BT5_SET;
	buf[9] = msg_counter;

	// The size of an ODID_MessagePack_encoded structure
	uint16_t encoded_data_size = 3 + encoded_data->MsgPackSize * ODID_MESSAGE_SIZE;
	// The size of that shit that comes before the ODID msg
	uint16_t adv_data_hdr_size = 6; // AD len(1), Type(1), UUID(2), AppCode(1), Counter(1)
	// Total bytes we need to send out
	uint16_t total_bytes_to_send = adv_data_hdr_size + encoded_data_size;
	LOG("total_bytes_to_send: %u", total_bytes_to_send);

	uint16_t bytes_sent = 0;

	while (bytes_sent < total_bytes_to_send) {
		uint8_t num_bytes = 0;
		uint16_t bytes_remaning = total_bytes_to_send - bytes_sent;

		if (bytes_remaning > _max_adv_data_len) {
			num_bytes = _max_adv_data_len;

		} else {
			num_bytes = bytes_remaning;
		}

		uint8_t* ptr = &buf[10];

		if (bytes_sent == 0) {
			memcpy(ptr, (uint8_t*)encoded_data + bytes_sent, num_bytes - adv_data_hdr_size);

		} else {
			ptr = &buf[4];
			memcpy(ptr, (uint8_t*)encoded_data + bytes_sent, num_bytes);
		}

		if (total_bytes_to_send > _max_adv_data_len) {

			if (bytes_sent == 0) {
				LOG("first fragment: %u", num_bytes);
				buf[1] = 1; // first fragmet

			} else if ((bytes_sent + num_bytes) == total_bytes_to_send) {
				LOG("last fragment: %u", num_bytes);
				buf[1] = 2; // last fragmet

			} else {
				LOG("intermediate fragment %u", num_bytes);
				buf[1] = 0; // intermediate fragmet
			}

		} else {
			LOG("complete message %u", num_bytes);
			buf[1] = 3; // complete
		}

		buf[3] = num_bytes;
		buf[4] = num_bytes - 1;

		// Send off the data
		if (send_command(ogf, ocf, buf, num_bytes)) {
			uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
			uint8_t status = {};
			CommandResponse response = read_command_response(opcode, &status, sizeof(status));

			if (response != CommandResponse::Success) {
				LOG(RED_TEXT "Failed to set extended advertising data: error 0x%x" NORMAL_TEXT, status);
			}
		}

		bytes_sent += num_bytes;
		LOG("bytes_sent %u", bytes_sent);
	}

	LOG("finished");
}

Bluetooth::CommandResponse Bluetooth::read_command_response(uint16_t opcode, uint8_t* response, int response_size)
{
	unsigned char buf[HCI_MAX_EVENT_SIZE] = {};

	ssize_t bytes_read = ::read(_dd, buf, sizeof(buf));

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

	default:
		LOG("Received unknown event: 0x%X", hdr->evt);
		break;
	}

	return CommandResponse::Error;
}

bool Bluetooth::send_command(uint8_t ogf, uint16_t ocf, uint8_t* data, uint8_t length)
{
	uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
	LOG("sending opcode: %u", opcode);

	if (hci_send_cmd(_dd, ogf, ocf, length, data) < 0) {
		LOG("send_command failed (did you use sudo?");
		return false;
	}

	return true;
}

} // end namespace bt