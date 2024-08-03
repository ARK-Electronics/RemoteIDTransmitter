#pragma once

#include <Bluetooth.hpp>
#include <Mavlink.hpp>

#include <parameters.hpp>
#include <ThreadSafeQueue.hpp>

#include <memory>
#include <functional>
#include <unordered_map>
#include <thread>

namespace txr
{

#if defined(DOCKER_BUILD)
static char VERSION_FILE_NAME[] = "/data/version.txt";
#else
static char VERSION_FILE_NAME[] = "/tmp/rid-transmitter/version.txt";
#endif

struct Settings {
	mavlink::ConfigurationSettings mavlink_settings {};
	bt::Settings bluetooth_settings {};
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

	// Bluetooth interface
	bt::Settings _bluetooth_settings {};
	std::shared_ptr<bt::Bluetooth> _bluetooth {};

	// Mavlink interface
	mavlink::ConfigurationSettings _mavlink_settings {};
	std::shared_ptr<mavlink::Mavlink> _mavlink {};

	void send_single_messages(struct ODID_UAS_Data* uasData, int* count, bool legacy);

	void create_message_pack(struct ODID_UAS_Data* uasData, struct ODID_MessagePack_encoded* pack_enc);

	void set_sw_version(const std::string& version);
	const std::string get_sw_version();
};

} // end namespace txr