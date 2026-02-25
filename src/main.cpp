#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <filesystem>

#include <toml.hpp>

#include <uas_serial.hpp>
#include <Transmitter.hpp>

static void signal_handler(int signum);

std::shared_ptr<txr::Transmitter> _transmitter {nullptr};

int main()
{
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	setbuf(stdout, NULL); // Disable stdout buffering

	// Two-tier config lookup: user override > deb-installed default
	const std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
	const auto user_config = std::filesystem::path(home) / ".config/ark/rid-transmitter/config.toml";
	const auto default_config = std::filesystem::path("/opt/ark/share/rid-transmitter/config.toml");
	const auto config_path = std::filesystem::exists(user_config) ? user_config : default_config;

	toml::table config;

	try {
		config = toml::parse_file(config_path.string());

	} catch (const toml::parse_error& err) {
		std::cerr << "Parsing failed:\n" << err << "\n";
		return -1;

	} catch (const std::exception& err) {
		std::cerr << "Error: " << err.what() << "\n";
		return -1;
	}

	std::string uas_serial_number;
	std::string manufacturer_code = config["manufacturer_code"].value_or("MFR1");
	std::string serial_number = config["serial_number"].value_or("123456789ABC");

	try {
		uas_serial_number = generateUASSerialNumber(manufacturer_code, serial_number);
		LOG("UAS Serial Number: %s", uas_serial_number.c_str());

	} catch (const std::invalid_argument& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return -1;
	}

	txr::Settings settings = {
		.mavsdk_connection_url = config["connection_url"].value_or("udp://0.0.0.0:14553"),
		.bluetooth_device = config["bluetooth_device"].value_or("hci0"),
		.uas_serial_number = uas_serial_number,
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
