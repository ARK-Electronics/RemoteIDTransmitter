#pragma once

#include <Bluetooth.hpp>
#include <Mavlink.hpp>

#include <parameters.hpp>
#include <ThreadSafeQueue.hpp>

#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <thread>

namespace txr
{

struct Settings {
	mavlink::ConfigurationSettings mavlink_settings {};
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

	std::vector<mavlink::Parameter> mavlink_param_request_list_cb();
	bool mavlink_param_set_cb(mavlink::Parameter* param);

	std::shared_ptr<mavlink::Mavlink> mavlink() { return _mavlink; };
	std::shared_ptr<bt::Bluetooth> bluetooth() { return _bluetooth; };


private:
	volatile std::atomic<bool> _should_exit {};

	// App settings
	Settings _settings {};

	// Bluetooth interface
	std::shared_ptr<bt::Bluetooth> _bluetooth {};

	// Mavlink interface
	std::shared_ptr<mavlink::Mavlink> _mavlink {};

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

	void set_sw_version(const std::string& version);
	const std::string get_sw_version();
};

} // end namespace txr