#include <iostream>
#include <string>
#include <stdexcept>
#include <cctype>

// Assuming _settings is a struct or class instance containing the manufacturer code and serial number
struct Settings {
	std::string manufacturer_code;
	std::string serial_number;
};

std::string generateUASSerialNumber(const std::string& manufacturer_code, const std::string& serial_number)
{
	// Validate the manufacturer code
	if (manufacturer_code.size() != 4) {
		throw std::invalid_argument("Manufacturer code must be exactly 4 characters long.");
	}

	// Ensure the manufacturer code contains only digits and uppercase letters (except 'O' and 'I')
	for (char c : manufacturer_code) {
		if (!(std::isdigit(c) || (std::isupper(c) && c != 'O' && c != 'I'))) {
			throw std::invalid_argument("Manufacturer code must contain only digits or uppercase letters (excluding 'O' and 'I').");
		}
	}

	// Get the length of the serial number and determine the length code
	size_t serial_number_length = serial_number.size();

	if (serial_number_length == 0 || serial_number_length > 15) {
		throw std::invalid_argument("Serial number length must be between 1 and 15 characters.");
	}

	char length_code;

	if (serial_number_length <= 9) {
		length_code = '0' + static_cast<char>(serial_number_length);

	} else {
		length_code = 'A' + static_cast<char>(serial_number_length - 10);
	}

	// Validate the serial number
	for (char c : serial_number) {
		if (!(std::isdigit(c) || (std::isupper(c) && c != 'O' && c != 'I'))) {
			throw std::invalid_argument("Serial number must contain only digits or uppercase letters (excluding 'O' and 'I').");
		}
	}

	// Concatenate to form the complete SN
	std::string uas_serial_number = manufacturer_code + length_code + serial_number;

	return uas_serial_number;
}
