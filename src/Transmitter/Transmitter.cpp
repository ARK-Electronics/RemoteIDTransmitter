#include <Transmitter.hpp>
#include <unistd.h>
#include <CircularBuffer.hpp>

namespace txr
{

Transmitter::Transmitter(const txr::Settings& settings)
	: _mavlink_settings(settings.mavlink_settings)
	, _bluetooth_settings(settings.bluetooth_settings)
{
	// If version has changed reset parameters to default
	std::string version(APP_GIT_VERSION);
	std::string saved_version(get_sw_version());

	if (saved_version != version) {
		params::set_defaults();
		set_sw_version(version);
		LOG(GREEN_TEXT "New software version! Resetting parameters" NORMAL_TEXT);
	}

	LOG(GREEN_TEXT "Version: " CYAN_TEXT "%s" NORMAL_TEXT, version.c_str());

	// Load parameters from file system into RAM
	params::load();
}

bool Transmitter::start()
{
	//// Setup Bluetooth
	_bluetooth = std::make_shared<bt::Bluetooth>(_bluetooth_settings);

	if (!_bluetooth->initialize()) {
		return false;
	}

	//// Setup MAVLink
	_mavlink = std::make_shared<mavlink::Mavlink>(_mavlink_settings);

	// Provide PARAM_REQUEST_LIST and PARAM_SET callbacks for our application
	_mavlink->enable_parameters(
		std::bind(&Transmitter::mavlink_param_request_list_cb, this),
		std::bind(&Transmitter::mavlink_param_set_cb, this, std::placeholders::_1)
	);

	// messages -- all should publish at >= 1_Hz (and not older than 1_s)
	// OPEN_DRONE_ID_BASIC_ID -- UAS info
	// OPEN_DRONE_ID_LOCATION -- UAS location
	// OPEN_DRONE_ID_SYSTEM -- GCS location

	// Subscribe to mavlink OPEN_DRONE_ID_LOCATION
	_mavlink->subscribe_to_message(MAVLINK_MSG_ID_OPEN_DRONE_ID_LOCATION, [this](const mavlink_message_t& message) {
		mavlink_open_drone_id_location_t msg;
		mavlink_msg_open_drone_id_location_decode(&message, &msg);
		LOG("MAVLINK_MSG_ID_OPEN_DRONE_ID_LOCATION");
	});

	auto result = _mavlink->start();

	if (result != mavlink::ConnectionResult::Success) {
		std::cout << "Mavlink connection start failed: " << result << std::endl;
		return false;
	}

	return true;
}

void Transmitter::stop()
{
	if (_mavlink.get()) _mavlink->stop();

	if (_bluetooth.get()) _bluetooth->stop();

	_should_exit.store(true);
}

void Transmitter::run_state_machine()
{
	int msg_counter = 0;

	while (!_should_exit) {

		if (params::updated()) {
			params::load();
		}

		LOG("Sending BT5 message pack");
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		struct ODID_UAS_Data uasData = {};

		// Testing
		uasData.BasicID[0].UAType = ODID_UATYPE_CAPTIVE_BALLOON;

		send_single_messages(&uasData, &msg_counter);

		// create_message_pack(&uasData, &encoded);
		// // Only send BT5 (for now)
		// _bluetooth->hci_le_set_extended_advertising_data(bt::BluetoothMode::BT5, &encoded, ++msg_counter);
	}
}

void Transmitter::send_single_messages(struct ODID_UAS_Data* uasData, int* count)
{
	union ODID_Message_encoded encoded;
	memset(&encoded, 0, sizeof(union ODID_Message_encoded));

	encodeBasicIDMessage((ODID_BasicID_encoded*) &encoded, &uasData->BasicID[0]);

	_bluetooth->hci_le_set_extended_advertising_data(bt::BluetoothMode::BT5, &encoded, ++(*count));
}

void Transmitter::create_message_pack(struct ODID_UAS_Data* uasData, struct ODID_MessagePack_encoded* pack_enc)
{
	union ODID_Message_encoded encoded = { 0 };
	ODID_MessagePack_data pack_data = { 0 };
	pack_data.SingleMessageSize = ODID_MESSAGE_SIZE;
	pack_data.MsgPackSize = 9;

	if (encodeBasicIDMessage((ODID_BasicID_encoded*) &encoded, &uasData->BasicID[0]) != ODID_SUCCESS)
		printf("Error: Failed to encode Basic ID\n");

	memcpy(&pack_data.Messages[0], &encoded, ODID_MESSAGE_SIZE);

	if (encodeBasicIDMessage((ODID_BasicID_encoded*) &encoded, &uasData->BasicID[1]) != ODID_SUCCESS)
		printf("Error: Failed to encode Basic ID\n");

	memcpy(&pack_data.Messages[1], &encoded, ODID_MESSAGE_SIZE);

	if (encodeLocationMessage((ODID_Location_encoded*) &encoded, &uasData->Location) != ODID_SUCCESS)
		printf("Error: Failed to encode Location\n");

	memcpy(&pack_data.Messages[2], &encoded, ODID_MESSAGE_SIZE);

	if (encodeAuthMessage((ODID_Auth_encoded*) &encoded, &uasData->Auth[0]) != ODID_SUCCESS)
		printf("Error: Failed to encode Auth 0\n");

	memcpy(&pack_data.Messages[3], &encoded, ODID_MESSAGE_SIZE);

	if (encodeAuthMessage((ODID_Auth_encoded*) &encoded, &uasData->Auth[1]) != ODID_SUCCESS)
		printf("Error: Failed to encode Auth 1\n");

	memcpy(&pack_data.Messages[4], &encoded, ODID_MESSAGE_SIZE);

	if (encodeAuthMessage((ODID_Auth_encoded*) &encoded, &uasData->Auth[2]) != ODID_SUCCESS)
		printf("Error: Failed to encode Auth 2\n");

	memcpy(&pack_data.Messages[5], &encoded, ODID_MESSAGE_SIZE);

	if (encodeSelfIDMessage((ODID_SelfID_encoded*) &encoded, &uasData->SelfID) != ODID_SUCCESS)
		printf("Error: Failed to encode Self ID\n");

	memcpy(&pack_data.Messages[6], &encoded, ODID_MESSAGE_SIZE);

	if (encodeSystemMessage((ODID_System_encoded*) &encoded, &uasData->System) != ODID_SUCCESS)
		printf("Error: Failed to encode System\n");

	memcpy(&pack_data.Messages[7], &encoded, ODID_MESSAGE_SIZE);

	if (encodeOperatorIDMessage((ODID_OperatorID_encoded*) &encoded, &uasData->OperatorID) != ODID_SUCCESS)
		printf("Error: Failed to encode Operator ID\n");

	memcpy(&pack_data.Messages[8], &encoded, ODID_MESSAGE_SIZE);

	if (encodeMessagePack(pack_enc, &pack_data) != ODID_SUCCESS)
		printf("Error: Failed to encode message pack_data\n");
}


std::vector<mavlink::Parameter> Transmitter::mavlink_param_request_list_cb()
{
	std::vector<mavlink::Parameter> mavlink_parameters;
	auto params = params::parameters(); // Copy parameters from working set
	uint16_t count = 0;

	for (auto& [name, value] : params) {
		mavlink::Parameter p = {
			.name = name,
			.float_value = value,
			.index = count++,
			.total_count = (uint16_t)params.size(),
			.type = MAV_PARAM_TYPE_REAL32 // We only use floats
		};

		mavlink_parameters.emplace_back(p);
	}

	return mavlink_parameters;
}

bool Transmitter::mavlink_param_set_cb(mavlink::Parameter* param)
{
	bool succes = params::set_parameter(param->name, param->float_value);

	if (succes) {
		auto params = params::parameters();
		param->index = distance(params.begin(), params.find(param->name));
	}

	return succes;
}

const std::string Transmitter::get_sw_version()
{
	std::string version;

	std::ifstream infile(VERSION_FILE_NAME);
	std::stringstream is;

	if (!is.good()) {
		return version;
	}

	is << infile.rdbuf();
	infile.close();

	std::string line;

	if (std::getline(is, line)) {
		version = line;
	}

	return version;
}

void Transmitter::set_sw_version(const std::string& version)
{
	std::ofstream outfile;
	outfile.open(VERSION_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
	outfile << version << std::endl;
	outfile.close();
}

} // end namespace txr