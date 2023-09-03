#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>

namespace util {
	bool check_for_conflicting_ports(int port_ = 0);
	std::string to_hex(int num);
	std::string get_last_error(int error);
	std::string get_last_error();
}	 // namespace util

#endif /* UTIL_HPP */
