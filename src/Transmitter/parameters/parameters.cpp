#include <parameters.hpp>

#include <cmath>
#include <math.h>
#include <atomic>
#include <mutex>

namespace params
{

static ParameterList DEFAULT_PARAMETERS = {
	{ "TEST_PARAM", DEFAULT_TEST_PARAM },
};

static ParameterList _parameters {};
static std::atomic<bool> _updated {};

std::mutex _mutex {};

#if defined(DOCKER_BUILD)
static char PARAMETER_FILE_NAME[] = "/data/parameters.txt";
#else
static char PARAMETER_FILE_NAME[] = "/tmp/rid-transmitter/parameters.txt";
#endif

static void update_file(const ParameterList& params);

ParameterList parameters()
{
	std::scoped_lock<std::mutex> lock(_mutex);
	return _parameters;
};

void set_defaults()
{
	std::scoped_lock<std::mutex> lock(_mutex);
	update_file(DEFAULT_PARAMETERS);
}

// NOTE: unguarded, used only in this file
void update_file(const ParameterList& params)
{
	std::ofstream outfile;
	outfile.open(PARAMETER_FILE_NAME, std::ofstream::out | std::ofstream::trunc);

	for (auto& [name, value] : params) {
		outfile << name << "=" << std::to_string(value) << std::endl;
	}

	outfile.close();

	_updated = true;
}

bool updated()
{
	return _updated;
}

void load()
{
	std::scoped_lock<std::mutex> lock(_mutex);

	std::ifstream infile(PARAMETER_FILE_NAME);

	std::stringstream is;
	is << infile.rdbuf();
	infile.close();

	std::string line;

	while (std::getline(is, line)) {

		std::istringstream is_line(line);
		std::string key;

		if (std::getline(is_line, key, '=')) {
			std::string value;

			if (std::getline(is_line, value)) {
				auto kvp = std::pair(key, std::stof(value));
				_parameters.insert(kvp);
			}
		}
	}

	_updated = false;
}

float get_parameter(const std::string& name)
{
	std::scoped_lock<std::mutex> lock(_mutex);

	if (_parameters.contains(name)) {
		return _parameters.at(name);
	}

	return 0;
}

bool set_parameter(const std::string& name, float value)
{
	std::scoped_lock<std::mutex> lock(_mutex);

	if (_parameters.contains(name)) {
		_parameters[name] = value;
		update_file(_parameters);
		return true;
	}

	return false;
}




} // end namespace params