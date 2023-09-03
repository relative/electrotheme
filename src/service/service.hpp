#ifndef SERVICE_SERVICE_HPP
#define SERVICE_SERVICE_HPP

#include <Windows.h>

#include <thread>

#include "eventsink.hpp"
#include "loader.hpp"
#include "server.hpp"

class Service {
 public:
  std::unique_ptr<Loader> loader;
  std::unique_ptr<Server> server;
  void start();

 private:
  IWbemLocator* pLoc = nullptr;
  IWbemServices* pSvc = nullptr;
  IUnsecuredApartment* pApp = nullptr;
  EventSink* pSink = nullptr;
  IUnknown* pStubUnk = nullptr;
  IWbemObjectSink* pStubSink = nullptr;

  void initialize_wmi();
  void destroy_wmi();
};

extern std::unique_ptr<Service> gService;

#endif /* SERVICE_SERVICE_HPP */
