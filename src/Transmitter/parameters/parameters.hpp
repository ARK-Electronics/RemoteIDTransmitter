#pragma once

// #include <helpers.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
#include <map>

namespace params
{

static constexpr float DEFAULT_TEST_PARAM = 6.9f;

using ParameterList = std::map<std::string, float>;

// Returns a copy of the entire parameter list.
ParameterList parameters();

bool updated();

// Loads parameters from the file system into RAM
void load();

// Sets all parameters to their default values
void set_defaults();

// Returns parameter value. Returns 0 if parameter does not exist.
float get_parameter(const std::string& name);

// Sets parameter value. Returns false if parameter does not exist.
bool set_parameter(const std::string& name, float value);

} // end namespace params