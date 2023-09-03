#ifndef LOG_HPP
#define LOG_HPP

#include <format>
#include <string_view>

template <typename... Args>
inline void __print(FILE* const stream, std::string_view fmt, Args&&... args) {
  auto str = std::vformat(fmt, std::make_format_args(args...));
  fprintf(stream, "%s\n", str.c_str());
}

#ifdef _DEBUG
// #ifdef _MSC_VER
// #define __PRETTY_FUNCTION__ __FUNCTION__
// #endif

constexpr inline std::string __dbg_logctx(std::string_view file, int line,
                                          std::string_view func) {
  return std::format("[{}:{} {}]", file, line, func);
}
template <typename... Args>
inline void __dbg_print(std::string_view file, int line, std::string_view func,
                        std::string_view fmt, Args&&... args) {
  auto logctx = __dbg_logctx(file, line, func);
  auto str = std::vformat(fmt, std::make_format_args(args...));
  __print(stdout, "{} {}", logctx, str);
}

  #define DbgLog(fmt, ...) \
    __dbg_print(__FILE__, __LINE__, __FUNCTION__, fmt, __VA_ARGS__)
#else
  #define DbgLog(fmt, ...)
#endif

#endif /* LOG_HPP */
