#include "Bluetooth.hpp"
#include <global_include.hpp>

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "print_bt_features.h"

namespace bt
{

void Bluetooth::legacy_set_random_address()
{
	LOG("Setting random address: Legacy");

	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = OCF_LE_SET_RANDOM_ADDRESS;
	uint8_t buf[_mac.size()] = {};

	memcpy(buf, _mac.data(), _mac.size());

	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set legacy random address: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::legacy_set_advertising_enable()
{
	LOG("Setting legacy advertising enable");

	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = OCF_LE_SET_ADVERTISE_ENABLE;
	uint8_t enable = 1;

	if (send_command(ogf, ocf, &enable, sizeof(enable))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set legacy advertising enable: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::legacy_set_advertising_disable()
{
	LOG("Setting legacy advertising disable");

	uint8_t ogf = OGF_LE_CTL;
	uint16_t ocf = OCF_LE_SET_ADVERTISE_ENABLE;
	uint8_t enable = 0;

	if (send_command(ogf, ocf, &enable, sizeof(enable))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set legacy advertising disable: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::legacy_set_advertising_parameters(uint16_t interval_ms)
{
	LOG("Setting legacy advertising parameters");

	uint8_t ogf = OGF_LE_CTL;     // Opcode Group Field. LE Controller Commands
	uint16_t ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
	uint8_t buf[] = { 0x00, 0x08, // Advertising_Interval_Min: N * 0.625 ms. 0x000800 = 1280 ms
			  0x00, 0x08, // Advertising_Interval_Max: N * 0.625 ms. 0x000800 = 1280 ms
			  0x03,       // Advertising_Type: 3 = Non connectable undirected advertising (ADV_NONCONN_IND)
			  0x01,       // Own_Address_Type: 1 = Random Device Address
			  0x00,       // Peer_Address_Type: 0 = Public Device Address (default) or Public Identity Address
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Peer_Address
			  0x07,       // Advertising_Channel_Map: 7 = all three channels enabled
			  0x00      // Advertising_Filter_Policy: 0 = Process scan and connection requests from all devices (i.e., the White List is not in use) (default).
			};

	interval_ms = std::min(std::max((1000 * interval_ms) / 625, 0x0020), 0x4000);

	uint16_t interval_min = interval_ms - 50;
	uint16_t interval_max = interval_ms + 50;

	buf[0] = interval_min & 0xFF;
	buf[1] = (interval_min >> 8) & 0xFF;
	buf[2] = interval_max & 0xFF;
	buf[3] = (interval_max >> 8) & 0xFF;

	// Send off the data
	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set legacy advertising parameters: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

void Bluetooth::legacy_set_advertising_data(const ODID_Message_encoded* data, uint8_t count)
{
	LOG("Setting legacy advertising data");

	uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
	uint16_t ocf = OCF_LE_SET_ADVERTISING_DATA; // LE Set Advertising Data
	uint8_t buf[7 + ODID_MESSAGE_SIZE] = {
		0x1F, // Advertising_Data_Length: The number of significant octets in the Advertising_Data.
		0x1E, // Length of the service data element
		0x16, // 16 = GAP AD Type = "Service Data - 16-bit UUID"
		0xFA, 0xFF, // 0xFFFA = ASTM International, ASTM Remote ID
		0x0D, // 0x0D = AD Application Code within the ASTM address space = Open Drone ID
		0x00, // xx = 8-bit message counter starting at 0x00 and wrapping around at 0xFF
		0x00
	};
	buf[6] = count;

	memcpy(&buf[7], (uint8_t*)data, ODID_MESSAGE_SIZE);

	// Send off the data
	if (send_command(ogf, ocf, buf, sizeof(buf))) {
		uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
		uint8_t status = wait_for_command_acknowledged(opcode, 100);

		if (status) {
			LOG(RED_TEXT "Failed to set legacy advertising data: error 0x%x" NORMAL_TEXT, status);
		}
	}
}

} // end namepspace bt