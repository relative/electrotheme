#include "node.hpp"

#include <Windows.h>
#include <WtsApi32.h>

#include <cstdio>
#include <stdexcept>

#include "../util.hpp"

namespace {
  int GetDebugSignalHandlerMappingName(uint32_t pid, char* buf,
                                       size_t buf_len) {
    return sprintf_s(buf, buf_len, "node-debug-handler-%u", pid);
  }
}  // namespace

bool node_debuggable_process(uint32_t pid) {
  HANDLE mapping = nullptr;
  char mapping_name[32];

  if (GetDebugSignalHandlerMappingName(pid, mapping_name,
                                       sizeof(mapping_name)) < 0)
    return false;
  mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == nullptr) return false;

  CloseHandle(mapping);
  return true;
}

void node_debug_process(uint32_t pid, void* handle) {
  HANDLE process = handle;
  HANDLE thread = nullptr;
  HANDLE mapping = nullptr;
  char mapping_name[32];
  LPTHREAD_START_ROUTINE* handler = nullptr;

  if (GetDebugSignalHandlerMappingName(pid, mapping_name,
                                       sizeof(mapping_name)) < 0)
    throw std::runtime_error(
        "node_debug_process(): GetDebugSignalHandlerMappingName returned < 0");

  mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == nullptr)
    throw std::runtime_error(
        "node_debug_process(): could not open file mapping\n" +
        util::get_last_error());

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof *handler));
  CloseHandle(mapping);
  if (handler == nullptr || *handler == nullptr)
    throw std::runtime_error(
        "node_debug_process(): could not get debug handler address");

  thread =
      CreateRemoteThread(process, nullptr, 0, *handler, nullptr, 0, nullptr);
  if (thread == nullptr)
    throw std::runtime_error(
        "node_debug_process(): could not create debug handler thread\n" +
        util::get_last_error());
}
