#include <Transmitter.hpp>
#include <unistd.h>
#include <mavsdk/log_callback.h>

namespace txr
{

Transmitter::Transmitter(const txr::Settings& settings)
	: _settings(settings)
{
	// Disable mavsdk noise
	mavsdk::log::subscribe([](...) {
		// https://mavsdk.mavlink.io/main/en/cpp/guide/logging.html
		return true;
	});
}

bool Transmitter::start()
{
	//// Setup Bluetooth
	_bluetooth = std::make_shared<bt::Bluetooth>(_settings.bluetooth_device);

	if (!_bluetooth->initialize()) {
		return false;
	}

	LOG("Waiting for MAVSDK connection: %s", _settings.mavsdk_connection_url.c_str());

	while (!wait_for_mavsdk_connection(3)) {
		if (_should_exit) {
			return false;
		}
	}

	_mavlink->subscribe_message(MAVLINK_MSG_ID_HEARTBEAT, [this](const mavlink_message_t& message) {
		if (message.sysid == 1 && message.compid == 1) {
			// LOG("MAVLINK_MSG_ID_HEARTBEAT: %u / %u", message.sysid, message.compid);
			std::lock_guard<std::mutex> lock(_heartbeat_mutex);
			mavlink_msg_heartbeat_decode(&message, &_heartbeat_msg);
		}
	});

	_mavlink->subscribe_message(MAVLINK_MSG_ID_OPEN_DRONE_ID_LOCATION, [this](const mavlink_message_t& message) {
		// LOG("MAVLINK_MSG_ID_OPEN_DRONE_ID_LOCATION: %u / %u", message.sysid, message.compid);
		std::lock_guard<std::mutex> lock(_location_mutex);
		mavlink_msg_open_drone_id_location_decode(&message, &_location_msg);
	});

	_mavlink->subscribe_message(MAVLINK_MSG_ID_OPEN_DRONE_ID_SYSTEM, [this](const mavlink_message_t& message) {
		// LOG("MAVLINK_MSG_ID_OPEN_DRONE_ID_SYSTEM: %u / %u", message.sysid, message.compid);
		std::lock_guard<std::mutex> lock(_system_mutex);
		mavlink_msg_open_drone_id_system_decode(&message, &_system_msg);
	});

	return true;
}

void Transmitter::stop()
{
	if (_bluetooth.get()) _bluetooth->stop();

	_should_exit.store(true);
}

bool Transmitter::wait_for_mavsdk_connection(double timeout_s)
{
	auto config = mavsdk::Mavsdk::Configuration(mavsdk::ComponentType::RemoteId);
	_mavsdk = std::make_shared<mavsdk::Mavsdk>(config);

	auto result = _mavsdk->add_any_connection(_settings.mavsdk_connection_url);

	if (result != mavsdk::ConnectionResult::Success) {
		return false;
	}

	auto system = _mavsdk->first_autopilot(timeout_s);

	if (!system) {
		return false;
	}

	LOG("Connected to autopilot");
	_mavlink = std::make_shared<mavsdk::MavlinkPassthrough>(system.value());

	return true;
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

		// Fill in the data from mavlink messages
		struct ODID_UAS_Data data = {};
		// Basic ID
		{
			std::lock_guard<std::mutex> lock(_heartbeat_mutex);
			data.BasicID[0].IDType = (ODID_idtype_t)MAV_ODID_ID_TYPE_SERIAL_NUMBER;
			data.BasicID[0].UAType = (ODID_uatype)_heartbeat_msg.type;
			strcpy(data.BasicID[0].UASID, _settings.uas_serial_number.c_str());
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
		LOG(RED_TEXT "failed to encode Basic ID" NORMAL_TEXT);
	}

	if (encodeLocationMessage((ODID_Location_encoded*) &location_encoded, &data->Location)) {
		LOG(RED_TEXT "failed to encode Location" NORMAL_TEXT);
	}

	if (encodeSystemMessage((ODID_System_encoded*) &system_encoded, &data->System)) {
		LOG(RED_TEXT "failed to encode System" NORMAL_TEXT);
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

} // end namespace txr
