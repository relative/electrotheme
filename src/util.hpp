#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>

namespace util {
  bool check_for_conflicting_ports();
  std::string to_hex(int num);
  std::string get_last_error(int error);
  std::string get_last_error();
}

#endif /* UTIL_HPP */
