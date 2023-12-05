#pragma once

#include <Mavlink.hpp>

#include <parameters.hpp>
#include <states/base/State.hpp>
#include <ThreadSafeQueue.hpp>

#include <memory>
#include <functional>
#include <unordered_map>
#include <thread>

// From opendroneid-core-c
extern "C" {
#include <bluetooth.h>
}

namespace txr
{

#if defined(DOCKER_BUILD)
static char VERSION_FILE_NAME[] = "/data/version.txt";
#else
static char VERSION_FILE_NAME[] = "/tmp/rid-transmitter/version.txt";
#endif

class Transmitter
{
public:
	Transmitter(const mavlink::ConfigurationSettings& settings);

	bool start();
	void stop();

	void run_state_machine();
	void handle_state_transition(states::AppState next_state);

	std::vector<mavlink::Parameter> mavlink_param_request_list_cb();
	bool mavlink_param_set_cb(mavlink::Parameter* param);

	uint64_t app_start_time() { return _app_start_time; };

	std::shared_ptr<mavlink::Mavlink> mavlink() { return _mavlink; };

	// Setters
	void set_next_state(states::AppState state);
	void request_next_state(states::AppState state); // MAVLink commands use requests

private:
	uint64_t _app_start_time {};
	volatile std::atomic<bool> _should_exit {};

	mavlink::ConfigurationSettings _settings {};

	// States
	states::AppState _current_state {};
	states::AppState _next_state {};

	ThreadSafeQueue<states::AppState> 		_state_request_queue {10};

	// State enum to object map
	std::unordered_map<states::AppState, std::shared_ptr<states::State<Transmitter>>> _states_map = {
		std::make_pair(states::AppState::STATE_1, std::make_shared<states::State1>()),
		std::make_pair(states::AppState::STATE_2, std::make_shared<states::State2>()),
		std::make_pair(states::AppState::STATE_3, std::make_shared<states::State3>()),
	};

	const std::string get_sw_version();
	void set_sw_version(const std::string& version);

	std::string state_string(const states::AppState state);

	std::shared_ptr<mavlink::Mavlink> _mavlink {};
};

} // end namespace txr