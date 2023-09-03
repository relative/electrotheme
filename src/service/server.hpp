#ifndef SERVICE_SERVER_HPP
#define SERVICE_SERVER_HPP

#include <uwebsockets/App.h>

#include <memory>
#include <string>
#include <vector>

struct PerSocketData {};

class Server {
 public:
  void start();
  void release();

  void update_style(std::string& exeName, std::string& styleContent);
  int port = 64132;

 private:
  std::unique_ptr<uWS::App> app;
};

#endif /* SERVICE_SERVER_HPP */
