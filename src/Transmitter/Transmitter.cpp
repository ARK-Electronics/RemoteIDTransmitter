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
	_bluetooth->initialize();

	//// Setup MAVLink
	_mavlink = std::make_shared<mavlink::Mavlink>(_mavlink_settings);

	// Provide PARAM_REQUEST_LIST and PARAM_SET callbacks for our application
	_mavlink->enable_parameters(
		std::bind(&Transmitter::mavlink_param_request_list_cb, this),
		std::bind(&Transmitter::mavlink_param_set_cb, this, std::placeholders::_1)
	);

	// Subscribe to mavlink OPEN_DRONE_ID_BASIC_ID
	_mavlink->subscribe_to_message(MAVLINK_MSG_ID_OPEN_DRONE_ID_BASIC_ID, [this](const mavlink_message_t& message) {
		mavlink_open_drone_id_basic_id_t msg;
		mavlink_msg_open_drone_id_basic_id_decode(&message, &msg);
		LOG("OPEN_DRONE_ID_BASIC_ID");
	});


	auto result = _mavlink->start();

	if (result != mavlink::ConnectionResult::Success) {
		std::cout << "Mavlink connection start failed: " << result << std::endl;
		return false;
	}

	_app_start_time = millis();
	_current_state = states::AppState::STATE_1;
	_states_map[_current_state]->on_enter();

	return true;
}

void Transmitter::stop()
{
	if (_mavlink.get()) _mavlink->stop();

	_should_exit.store(true);
}

void Transmitter::run_state_machine()
{
	while (!_should_exit) {

		// Update parameters
		if (params::updated()) {
			params::load();
		}

		// Application internal state change
		bool application_new_state = _current_state != _next_state;

		// Only process external state requests if there is no internal change pending
		std::optional<states::AppState> requested_state = {std::nullopt};

		if (!application_new_state) {
			requested_state = _state_request_queue.pop_front();
		}

		// Always execute internal transitions before executing requested transitions (prevents race condition)
		auto next_state = application_new_state ? _next_state : requested_state ? requested_state.value() : _current_state;

		// NOTE: the 'set_next_state' and 'request_next_state' functions already check if the transition is possible
		if (next_state != _current_state) {
			handle_state_transition(next_state);
		}

		// Run current state
		_states_map[_current_state]->run();
	}
}

void Transmitter::handle_state_transition(states::AppState next_state)
{
	_states_map[_current_state]->on_exit();

	// Set current and next states equal
	_current_state = next_state;
	_next_state = _current_state;

	_states_map[_current_state]->on_enter();
}

void Transmitter::set_next_state(states::AppState state)
{
	if (!_states_map[_current_state]->can_transition_to(state)) {
		LOG(RED_TEXT "Cannot transition from: %s --> %s" NORMAL_TEXT, state_string(_current_state).c_str(), state_string(state).c_str());
		return;
	}

	LOG("Setting next transition: %s --> %s", state_string(_current_state).c_str(), state_string(state).c_str());
	_next_state = state;
}

void Transmitter::request_next_state(states::AppState state)
{
	if (!_states_map[_current_state]->can_transition_to(state)) {
		LOG(RED_TEXT "Cannot transition from: %s --> %s" NORMAL_TEXT, state_string(_current_state).c_str(), state_string(state).c_str());
		return;
	}

	LOG("Requesting next transition: %s --> %s", state_string(_current_state).c_str(), state_string(state).c_str());

	if (!_state_request_queue.push_back(state)) {
		LOG(RED_TEXT "State request queue full! Someone is spamming..." NORMAL_TEXT);
	}
}

std::string Transmitter::state_string(const states::AppState state)
{
	switch (state) {
	case states::AppState::STATE_1: return "STATE_1";

	case states::AppState::STATE_2: return "STATE_2";

	case states::AppState::STATE_3: return "STATE_3";

	default: return "UNDEFINED";
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