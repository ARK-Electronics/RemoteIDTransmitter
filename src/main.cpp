#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include <toml.hpp>

#include <Mavlink.hpp>
#include <Transmitter.hpp>

static void signal_handler(int signum);

std::shared_ptr<txr::Transmitter> _transmitter {nullptr};

int main()
{
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	setbuf(stdout, NULL); // Disable stdout buffering

	toml::table config;

	try {
		config = toml::parse_file(std::string(getenv("HOME")) + "/.local/share/rid-transmitter/config.toml");

	} catch (const toml::parse_error& err) {
		std::cerr << "Parsing failed:\n" << err << "\n";
		return -1;

	} catch (const std::exception& err) {
		std::cerr << "Error: " << err.what() << "\n";
		return -1;
	}

	mavlink::ConfigurationSettings mav_settings = {
		.connection_url = config["connection_url"].value_or("udp://0.0.0.0:14553"),
		.sysid = 1,
		.compid = MAV_COMP_ID_ODID_TXRX_1,
		.target_sysid = 0, // handle messages from all systems
		.target_compid = 0, // handle messages from all components
		.mav_type = MAV_TYPE_ODID,
		.mav_autopilot = MAV_AUTOPILOT_INVALID,
		.emit_heartbeat = true,
	};

	txr::Settings settings = {
		.mavlink_settings = mav_settings,
		.bluetooth_device = std::string("hci0"),
	};

	_transmitter = std::make_shared<txr::Transmitter>(settings);

	if (!_transmitter->start()) {
		LOG("Failed to start, exiting!");
		exit(1);
	}

	// Only exits on Ctrl+C
	_transmitter->run_state_machine();

	LOG("Exiting!");
	return 0;
}

static void signal_handler(int signum)
{
	LOG("signal_handler! %d", signum);

	if (_transmitter.get()) _transmitter->stop();
}
