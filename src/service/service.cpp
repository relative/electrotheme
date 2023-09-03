#include "service.hpp"

#include <cstdio>
#include <thread>

#include "../log.hpp"
#include "../util.hpp"

std::unique_ptr<Service> gService;

void Service::initialize_wmi() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr))
    throw std::runtime_error("Failed to initialize COM: " + std::to_string(hr));
  hr = CoInitializeSecurity(
      nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
  if (FAILED(hr)) {
    CoUninitialize();
    throw std::runtime_error("Failed to initialize COM security level: " +
                             std::to_string(hr));
  }

  hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                        IID_IWbemLocator, reinterpret_cast<void**>(&pLoc));
  if (FAILED(hr)) {
    CoUninitialize();
    throw std::runtime_error("Failed to create WbemLocator instance: " +
                             std::to_string(hr));
  }

  hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, 0,
                           0, &pSvc);
  if (FAILED(hr)) {
    destroy_wmi();
    throw std::runtime_error("Failed to connect to WMI: " + std::to_string(hr));
  }

  hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                         RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                         nullptr, EOAC_NONE);
  if (FAILED(hr)) {
    destroy_wmi();
    throw std::runtime_error("Failed to set proxy blanket: " +
                             std::to_string(hr));
  }

  hr = CoCreateInstance(CLSID_UnsecuredApartment, nullptr, CLSCTX_LOCAL_SERVER,
                        IID_IUnsecuredApartment,
                        reinterpret_cast<void**>(&pApp));
  if (FAILED(hr)) {
    destroy_wmi();
    throw std::runtime_error("Failed to create UnsecuredApartment instance: " +
                             std::to_string(hr));
  }

  pApp->CreateObjectStub(pSink, &pStubUnk);

  pStubUnk->QueryInterface(IID_IWbemObjectSink,
                           reinterpret_cast<void**>(&pStubSink));

  // child processes of Electron apps have --type command line argument
  // we are only targeting the main processes as those are the only processes
  // that have the debug handler
  hr = pSvc->ExecNotificationQueryAsync(
      _bstr_t("WQL"),
      _bstr_t("SELECT * FROM "
              "__InstanceCreationEvent WITHIN "
              "1 WHERE "  // PollingInterval
                          // = 1.fsec
              "TargetInstance ISA "
              "'Win32_Process' AND NOT "
              "TargetInstance.CommandLine "
              "LIKE '%--type=%'"),
      WBEM_FLAG_SEND_STATUS, nullptr, pStubSink);
  if (FAILED(hr)) {
    destroy_wmi();
    throw std::runtime_error("Failed to setup WMI notification: " +
                             std::to_string(hr));
  }
}

void Service::destroy_wmi() {
  if (pSvc != nullptr) pSvc->Release();
  if (pLoc != nullptr) pLoc->Release();
  if (pApp != nullptr) pApp->Release();
  if (pStubUnk != nullptr) pStubUnk->Release();
  if (pSink != nullptr) pSink->Release();
  if (pStubSink != nullptr) pStubSink->Release();
  CoUninitialize();
}

void Service::start() {
  try {
    DbgLog("Checking for conflicting ports");
    auto bConflicting = util::check_for_conflicting_ports();
    if (bConflicting) {
      __print(stdout,
              "Found conflicting ports, please resolve before continuing.");
      exit(1);
    }
  } catch (const std::exception& ex) {
    __print(stderr,
            "Checking for conflicting ports failed: {}\nThis is not fatal.",
            ex.what());
  }

  __print(stdout, "Starting WebSocket server");
  server = std::make_unique<Server>();

  __print(stdout, "Starting loader thread");
  loader = std::make_unique<Loader>();

  __print(stdout, "Initializing process watcher");
  try {
    pSink = new EventSink();
    initialize_wmi();
  } catch (const std::exception& ex) {
    __print(stderr, "Initializing process watcher failed: {}", ex.what());
    exit(1);
  }

  server->thread.join();

  destroy_wmi();
}
