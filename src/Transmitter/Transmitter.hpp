#pragma once

#include <Bluetooth.hpp>

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <thread>

#include <global_include.hpp>


namespace txr
{

struct Settings {
	// mavlink::ConfigurationSettings mavlink_settings {};
	std::string mavsdk_connection_url;
	std::string bluetooth_device {};
	std::string uas_serial_number {};
};

class Transmitter
{
public:
	Transmitter(const txr::Settings& settings);

	bool start();
	void stop();

	void run_state_machine();


	// std::shared_ptr<mavlink::Mavlink> mavlink() { return _mavlink; };
	std::shared_ptr<bt::Bluetooth> bluetooth() { return _bluetooth; };


private:
	volatile std::atomic<bool> _should_exit {};

	// App settings
	Settings _settings {};

	// Bluetooth interface
	std::shared_ptr<bt::Bluetooth> _bluetooth {};

	// Mavlink interface
	// std::shared_ptr<mavlink::Mavlink> _mavlink {};
	std::shared_ptr<mavsdk::Mavsdk> _mavsdk;
	std::shared_ptr<mavsdk::MavlinkPassthrough> _mavlink;

	// Mavlink message data
	std::mutex _heartbeat_mutex;
	std::mutex _location_mutex;
	std::mutex _system_mutex;
	mavlink_heartbeat_t _heartbeat_msg {};
	mavlink_open_drone_id_location_t _location_msg {};
	mavlink_open_drone_id_system_t _system_msg {};

	// Each message has a unique counter
	int _basic_msg_counter {};
	int _location_msg_counter {};
	int _system_msg_counter {};

	// Toggles between legacy and extended advertisements
	bool _toggle_legacy {};

	// Sends the Basic ID, Location/Vector, and System messages
	void send_single_messages(struct ODID_UAS_Data* data);

	bool wait_for_mavsdk_connection(double timeout_s);
};

} // end namespace txr
