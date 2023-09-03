#ifndef SERVICE_SERVER_HPP
#define SERVICE_SERVER_HPP

#include <uwebsockets/App.h>

#include <memory>
#include <string>
#include <vector>

struct PerSocketData {};

class Server {
 public:
  Server() { thread = std::thread(&Server::loop, this); }
  ~Server();
  void release();

  void update_style(std::string& exeName, std::string& styleContent);
  int port = 64132;

  std::thread thread;

 private:
  std::unique_ptr<uWS::App> app;
  void loop();
};

#endif /* SERVICE_SERVER_HPP */
