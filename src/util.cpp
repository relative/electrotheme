#include "util.hpp"
#include "log.hpp"
#include <Windows.h>
#include <iphlpapi.h>
#include <stdexcept>

bool util::check_for_conflicting_ports(int port_ = 0) {
  // For some odd reason GetOwnerModuleFromTcpEntry requires Advapi32.dll to be loaded
  // and it doesn't load it itself. Else it will return ERROR_MOD_NOT_FOUND (126L = 0x7e)
  LoadLibraryA("Advapi32.dll");
  
  bool bConflicting = false;
  DWORD hr = 0;

  MIB_TCPTABLE_OWNER_MODULE* tcpTable;
  DWORD size = 0;
  hr = GetExtendedTcpTable(nullptr, &size, false, AF_INET, TCP_TABLE_OWNER_MODULE_LISTENER, 0);
  if (hr != ERROR_INSUFFICIENT_BUFFER)
    throw std::runtime_error("check_for_conflicting_ports(): GetExtendedTcpTable() (size) failed with " + util::to_hex(hr));

  tcpTable = new MIB_TCPTABLE_OWNER_MODULE[size];
  hr = GetExtendedTcpTable(tcpTable, &size, false, AF_INET, TCP_TABLE_OWNER_MODULE_LISTENER, 0);
  if (hr != NO_ERROR)
    throw std::runtime_error("check_for_conflicting_ports(): GetExtendedTcpTable() failed with " + util::to_hex(hr));
  
  for (auto i = 0; i < tcpTable->dwNumEntries; ++i) {
    auto entry = tcpTable->table[i];
    auto localPort = ntohs(entry.dwLocalPort);

    std::string ownerModule = "?";

    TCPIP_OWNER_MODULE_BASIC_INFO* info;
    DWORD szInfo = 0;
    hr = GetOwnerModuleFromTcpEntry(&entry, TCPIP_OWNER_MODULE_INFO_BASIC, nullptr, &szInfo);
      
    if (hr != ERROR_INSUFFICIENT_BUFFER) {
      WARNDEBUG("Failed to get owner info (size) for port %i: error 0x%08x\n", localPort, hr);
    } else {
      info = new TCPIP_OWNER_MODULE_BASIC_INFO[szInfo];
      hr = GetOwnerModuleFromTcpEntry(&entry, TCPIP_OWNER_MODULE_INFO_BASIC, info, &szInfo);
      if (hr == NO_ERROR && info->pModuleName != nullptr) {
        char buf[512];
        wcstombs(buf, info->pModuleName, 512);
        ownerModule = std::string(buf);
      } else {
        WARNDEBUG("Failed to get owner info for port %i: error 0x%08x\n", localPort, hr);
      }
    }

    // LOGDEBUG("port %i - owned by process %s (pid: %i)\n", localPort, ownerModule.c_str(), entry.dwOwningPid);
    if (port_ == 0) { // check for conflicting ports
      switch (localPort) {
        case 9229:
          fprintf(stderr,
            "Found conflicting port %i - owned by process %s (pid: %i)\n",
            localPort, ownerModule.c_str(), entry.dwOwningPid);
          bConflicting = true;
          break;
        default:
          break;
      }
    } else { // check for conflicting SERVER ports (port_ param)
      if (localPort == port_) {
        fprintf(stderr,
          "Found conflicting port %i - owned by process %s (pid: %i)\n",
          localPort, ownerModule.c_str(), entry.dwOwningPid);
        return true; // bConflicting = true;
      }
    }
  }

  delete[] tcpTable;

  return bConflicting;
}

std::string util::to_hex(int num) {
  char buf[11];
  sprintf_s(buf, sizeof(buf), "0x%08x", num);
  std::string str = buf;
  return str;
}

std::string util::get_last_error(int error) {
  char* buf;
  auto size = FormatMessageA(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr,
    error,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    reinterpret_cast<char*>(&buf),
    0,
    nullptr);
  std::string msg(buf, size);
  LocalFree(buf);
  return "Error " + to_hex(error) + ":\n  " + msg;
}
std::string util::get_last_error() {
  return get_last_error(GetLastError());
}