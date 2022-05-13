#ifndef LOG_HPP
#define LOG_HPP

#ifdef _DEBUG
  #define LOGDEBUG(...) printf(__VA_ARGS__)
  #define LOGDEBUGW(...) wprintf(__VA_ARGS__)
  #define WARNDEBUG(...) fprintf(stderr, __VA_ARGS__)
  #define WARNDEBUGW(...) fwprintf(stderr, __VA_ARGS__)
#else
  #define LOGDEBUG
  #define LOGDEBUGW
  #define WARNDEBUG
  #define WARNDEBUGW
#endif

#endif /* LOG_HPP */
