#include <Transmitter.hpp>
#include <unistd.h>
#include <CircularBuffer.hpp>

namespace txr
{

#if defined(DOCKER_BUILD)
static char VERSION_FILE_NAME[] = "/data/version.txt";
#else
static char VERSION_FILE_NAME[] = "/tmp/rid-transmitter/version.txt";
#endif

Transmitter::Transmitter(const txr::Settings& settings)
	: _bluetooth_settings(settings.bluetooth_settings)
	, _mavlink_settings(settings.mavlink_settings)
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

	// Subscribe to mavlink OPEN_DRONE_ID_LOCATION
	_mavlink->subscribe_to_message(MAVLINK_MSG_ID_OPEN_DRONE_ID_BASIC_ID, [this](const mavlink_message_t& message) {
		// LOG("MAVLINK_MSG_ID_OPEN_DRONE_ID_BASIC_ID: %u / %u", message.sysid, message.compid);
		std::lock_guard<std::mutex> lock(_basic_id_mutex);
		mavlink_msg_open_drone_id_basic_id_decode(&message, &_basic_id_msg);
	});

	_mavlink->subscribe_to_message(MAVLINK_MSG_ID_OPEN_DRONE_ID_LOCATION, [this](const mavlink_message_t& message) {
		// LOG("MAVLINK_MSG_ID_OPEN_DRONE_ID_LOCATION: %u / %u", message.sysid, message.compid);
		std::lock_guard<std::mutex> lock(_location_mutex);
		mavlink_msg_open_drone_id_location_decode(&message, &_location_msg);
	});

	_mavlink->subscribe_to_message(MAVLINK_MSG_ID_OPEN_DRONE_ID_SYSTEM, [this](const mavlink_message_t& message) {
		// LOG("MAVLINK_MSG_ID_OPEN_DRONE_ID_SYSTEM: %u / %u", message.sysid, message.compid);
		std::lock_guard<std::mutex> lock(_system_mutex);
		mavlink_msg_open_drone_id_system_decode(&message, &_system_msg);
	});

	auto result = _mavlink->start();

	if (result != mavlink::ConnectionResult::Success) {
		LOG("Mavlink connection start failed");
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
	uint64_t loop_rate_ms = 200;

	while (!_should_exit) {

		uint64_t start_time = millis();

		_toggle_legacy = !_toggle_legacy;

		if (_toggle_legacy) {
			_bluetooth->enable_legacy_advertising();

		} else {
			_bluetooth->enable_le_extended_advertising();
		}

		if (params::updated()) {
			params::load();
		}

		// Fill in the data from mavlink messages
		struct ODID_UAS_Data data = {};
		// Basic ID
		{
			std::lock_guard<std::mutex> lock(_basic_id_mutex);
			data.BasicID[0].UAType = (ODID_uatype)_basic_id_msg.id_type;
			data.BasicID[0].IDType = (ODID_idtype_t)_basic_id_msg.ua_type;
			memcpy(data.BasicID[0].UASID, _basic_id_msg.uas_id, 20);
		}
		// Location / Vector
		{
			std::lock_guard<std::mutex> lock(_location_mutex);
			data.Location.Status = (ODID_status_t)_location_msg.status;
			data.Location.Direction = float(_location_msg.direction) / 100.f;
			data.Location.SpeedHorizontal = float(_location_msg.speed_horizontal) / 100.f;
			data.Location.SpeedVertical = float(_location_msg.speed_vertical) / 100.f;
			data.Location.Latitude = double(_location_msg.latitude) / 1.e7;
			data.Location.Longitude = double(_location_msg.longitude) / 1.e7;
			data.Location.AltitudeBaro = _location_msg.altitude_barometric;
			data.Location.AltitudeGeo = _location_msg.altitude_geodetic;
			data.Location.HeightType = (ODID_Height_reference)_location_msg.height_reference;
			data.Location.Height = _location_msg.height;
			data.Location.HorizAccuracy = (ODID_Horizontal_accuracy_t)_location_msg.horizontal_accuracy;
			data.Location.VertAccuracy = (ODID_Vertical_accuracy_t)_location_msg.vertical_accuracy;
			data.Location.BaroAccuracy = (ODID_Vertical_accuracy_t)_location_msg.barometer_accuracy;
			data.Location.SpeedAccuracy = (ODID_Speed_accuracy_t)_location_msg.speed_accuracy;
			data.Location.TSAccuracy = (ODID_Timestamp_accuracy_t)_location_msg.timestamp_accuracy;
			data.Location.TimeStamp = _location_msg.timestamp;
		}
		// System
		{
			std::lock_guard<std::mutex> lock(_system_mutex);
			data.System.OperatorLocationType = (ODID_operator_location_type_t)_system_msg.operator_location_type;
			data.System.ClassificationType = (ODID_classification_type_t)_system_msg.classification_type;
			data.System.OperatorLatitude = _system_msg.operator_latitude / 1.e7;
			data.System.OperatorLongitude = _system_msg.operator_longitude / 1.e7;
			data.System.AreaCount = _system_msg.area_count;
			data.System.AreaRadius = _system_msg.area_radius;
			data.System.AreaCeiling = _system_msg.area_ceiling;
			data.System.AreaFloor = _system_msg.area_floor;
			data.System.CategoryEU = (ODID_category_EU_t)_system_msg.category_eu;
			data.System.ClassEU = (ODID_class_EU_t)_system_msg.class_eu;
			data.System.OperatorAltitudeGeo = _system_msg.operator_altitude_geo;
			data.System.Timestamp = _system_msg.timestamp;
		}

		// Send out the data
		send_single_messages(&data);

		// Disable when we're done so that we only broadcast a single advertisement
		if (_toggle_legacy) {
			_bluetooth->disable_legacy_advertising();

		} else {
			_bluetooth->disable_le_extended_advertising();
		}

		// Reschedule loop at fixed rate
		uint64_t elapsed = millis() - start_time;
		uint64_t sleep_time = elapsed > loop_rate_ms ? 0 : loop_rate_ms - elapsed;
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
	}
}

void Transmitter::send_single_messages(struct ODID_UAS_Data* data)
{
	union ODID_Message_encoded basic_encoded = {};
	union ODID_Message_encoded location_encoded = {};
	union ODID_Message_encoded system_encoded = {};

	if (encodeBasicIDMessage((ODID_BasicID_encoded*) &basic_encoded, &data->BasicID[0])) {
		LOG("failed to encode Basic ID");
	}

	if (encodeLocationMessage((ODID_Location_encoded*) &location_encoded, &data->Location)) {
		LOG("failed to encode Location");
	}

	if (encodeSystemMessage((ODID_System_encoded*) &system_encoded, &data->System)) {
		LOG("failed to encode System");
	}

	// We wait 30ms in between message advertisements since the min advertising interval is 20ms
	// This ensures that the data is published, any slower and data will get missed.
	if (_toggle_legacy) {
		// Set BT Legacy advertising data
		_bluetooth->legacy_set_advertising_data(&basic_encoded, ++_basic_msg_counter);
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
		_bluetooth->legacy_set_advertising_data(&location_encoded, ++_location_msg_counter);
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
		_bluetooth->legacy_set_advertising_data(&system_encoded, ++_system_msg_counter);
		std::this_thread::sleep_for(std::chrono::milliseconds(30));

	} else {
		// Send LE Extended advertising data
		_bluetooth->hci_le_set_extended_advertising_data(&basic_encoded, ++_basic_msg_counter);
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
		_bluetooth->hci_le_set_extended_advertising_data(&location_encoded, ++_location_msg_counter);
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
		_bluetooth->hci_le_set_extended_advertising_data(&system_encoded, ++_system_msg_counter);
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}
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