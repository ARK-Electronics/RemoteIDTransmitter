#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include <argparse/argparse.hpp>

#include <Mavlink.hpp>
#include <Transmitter.hpp>

static void signal_handler(int signum);

std::shared_ptr<txr::Transmitter> _transmitter {nullptr};

int main(int argc, const char** argv)
{
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	setbuf(stdout, NULL); // Disable stdout buffering

	argparse::ArgumentParser parser("rid-transmitter");

	parser.add_argument("--mavlink-url").default_value("udp://0.0.0.0:14569").help("Mavlink connection url").metavar("MAV");
	parser.add_argument("--sysid").default_value(1).help("Mavlink System ID").metavar("SYSID");
	parser.add_argument("--compid").default_value((int)MAV_COMP_ID_ODID_TXRX_1).help("Mavlink Component ID").metavar("COMPID");

	parser.add_description("Specify a mavlink connection url");
	parser.add_epilog("Example usage:\nRemoteIDTransmitter udp://0.0.0.0:14569");

	try {
		parser.parse_args(argc, argv);

	} catch (const std::runtime_error& err) {
		std::cerr << err.what() << std::endl;
		std::cerr << parser;
		std::exit(1);
	}

	mavlink::ConfigurationSettings mav_settings = {
		.connection_url = parser.get<std::string>("--mavlink-url"),
		.sysid = static_cast<uint8_t>(parser.get<int>("--sysid")),
		.compid = static_cast<uint8_t>(parser.get<int>("--compid")),
		.target_sysid = 0, // handle messages from all systems
		.target_compid = 0, // handle messages from all components
		.mav_type = MAV_TYPE_ODID,
		.mav_autopilot = MAV_AUTOPILOT_INVALID,
		.emit_heartbeat = true,
	};

	bt::Settings bt_settings = {
		.hci_device_name = "hci0",
		.use_bt5 = true,
		.use_btl = true,
	};

	txr::Settings settings = {
		.mavlink_settings = mav_settings,
		.bluetooth_settings = bt_settings,
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
	LOG("signal_handler!");

	if (_transmitter.get()) _transmitter->stop();
}
