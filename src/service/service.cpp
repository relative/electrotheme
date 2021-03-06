#include "service.hpp"
#include "../util.hpp"
#include "../log.hpp"
#include <cstdio>
#include <thread>

Service* gService = nullptr;

void Service::initialize_wmi() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr))
    throw std::runtime_error("Failed to initialize COM: " + std::to_string(hr));
  hr = CoInitializeSecurity(
      nullptr,
      -1,
      nullptr,
      nullptr,
      RPC_C_AUTHN_LEVEL_DEFAULT,
      RPC_C_IMP_LEVEL_IMPERSONATE,
      nullptr,
      EOAC_NONE,
      nullptr);
  if (FAILED(hr)) {
    CoUninitialize();
    throw std::runtime_error("Failed to initialize COM security level: " + std::to_string(hr));
  }

  hr = CoCreateInstance(
      CLSID_WbemLocator,
      nullptr,
      CLSCTX_INPROC_SERVER,
      IID_IWbemLocator, reinterpret_cast<void**>(&pLoc));
  if (FAILED(hr)) {
    CoUninitialize();
    throw std::runtime_error("Failed to create WbemLocator instance: " + std::to_string(hr));
  }

  hr = pLoc->ConnectServer(
      _bstr_t(L"ROOT\\CIMV2"),
      nullptr,
      nullptr,
      0,
      0,
      0,
      0,
      &pSvc);
  if (FAILED(hr)) {
    destroy_wmi();
    throw std::runtime_error("Failed to connect to WMI: " + std::to_string(hr));
  }

  hr = CoSetProxyBlanket(
      pSvc,
      RPC_C_AUTHN_WINNT,
      RPC_C_AUTHZ_NONE,
      nullptr,
      RPC_C_AUTHN_LEVEL_CALL,
      RPC_C_IMP_LEVEL_IMPERSONATE,
      nullptr,
      EOAC_NONE);
  if (FAILED(hr)) {
    destroy_wmi();
    throw std::runtime_error("Failed to set proxy blanket: " + std::to_string(hr));
  }

  hr = CoCreateInstance(
      CLSID_UnsecuredApartment,
      nullptr,
      CLSCTX_LOCAL_SERVER,
      IID_IUnsecuredApartment, reinterpret_cast<void**>(&pApp));
  if (FAILED(hr)) {
    destroy_wmi();
    throw std::runtime_error("Failed to create UnsecuredApartment instance: " + std::to_string(hr));
  }

  pApp->CreateObjectStub(pSink, &pStubUnk);

  pStubUnk->QueryInterface(IID_IWbemObjectSink, reinterpret_cast<void**>(&pStubSink));

  // child processes of Electron apps have --type command line argument
  // we are only targeting the main processes as those are the only processes that have the debug handler
  hr = pSvc->ExecNotificationQueryAsync(
      _bstr_t("WQL"),
      _bstr_t("SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE " // PollingInterval = 1.fsec
              "TargetInstance ISA 'Win32_Process' AND NOT TargetInstance.CommandLine LIKE '%--type=%'"),
      WBEM_FLAG_SEND_STATUS,
      nullptr,
      pStubSink);
  if (FAILED(hr)) {
    destroy_wmi();
    throw std::runtime_error("Failed to setup WMI notification: " + std::to_string(hr));
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
    LOGDEBUG("Checking for conflicting ports\n");
    auto bConflicting = util::check_for_conflicting_ports();
    if (bConflicting) {
      printf("Found conflicting ports, please resolve before continuing.\n");
      exit(1);
    }
  } catch (const std::exception& ex) {
    fprintf(stderr, "Checking for conflicting ports failed: %s\nThis is not fatal.\n", ex.what());
  }
  
  printf("Starting WebSocket server\n");
  server = new Server();
  websocketThread = std::thread(&Server::start, server);

  printf("Starting loader thread\n");
  loader = new Loader();
  loaderThread = std::thread(&Loader::start, loader);

  printf("Initializing process watcher\n");
  try {
    pSink = new EventSink();
    initialize_wmi();
  } catch (const std::exception& ex) {
    fprintf(stderr, "Initializing process watcher failed: %s\n", ex.what());
    exit(1);
  }

  websocketThread.join();

  destroy_wmi();
}