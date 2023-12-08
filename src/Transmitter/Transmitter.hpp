#pragma once

#include <Bluetooth.hpp>
#include <Mavlink.hpp>

#include <parameters.hpp>
#include <states/base/State.hpp>
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
	void handle_state_transition(states::AppState next_state);

	std::vector<mavlink::Parameter> mavlink_param_request_list_cb();
	bool mavlink_param_set_cb(mavlink::Parameter* param);

	uint64_t app_start_time() { return _app_start_time; };

	std::shared_ptr<mavlink::Mavlink> mavlink() { return _mavlink; };
	std::shared_ptr<bt::Bluetooth> bluetooth() { return _bluetooth; };

	void set_next_state(states::AppState state);

private:
	uint64_t _app_start_time {};
	volatile std::atomic<bool> _should_exit {};

	// Bluetooth interface
	bt::Settings _bluetooth_settings {};
	std::shared_ptr<bt::Bluetooth> _bluetooth {};

	// Mavlink interface
	mavlink::ConfigurationSettings _mavlink_settings {};
	std::shared_ptr<mavlink::Mavlink> _mavlink {};

	// States
	states::AppState _current_state {};
	states::AppState _next_state {};

	// State enum to object map
	std::unordered_map<states::AppState, std::shared_ptr<states::State<Transmitter>>> _states_map = {
		std::make_pair(states::AppState::STATE_1, std::make_shared<states::State1>()),
		std::make_pair(states::AppState::STATE_2, std::make_shared<states::State2>()),
		std::make_pair(states::AppState::STATE_3, std::make_shared<states::State3>()),
	};

	void set_sw_version(const std::string& version);
	const std::string get_sw_version();

	std::string state_string(const states::AppState state);
};

} // end namespace txr