#ifndef SERVICE_SERVER_HPP
#define SERVICE_SERVER_HPP

#include <string>
#include <vector>

namespace uWS {
  template <typename bool>
  class TemplatedApp;

  template <bool SSL, bool isServer, typename USERDATA>
  struct WebSocket;
}

struct PerSocketData {
  bool ready = false;
  std::string executableName;
};

using etws = uWS::WebSocket<false, true, PerSocketData>;

class Server {
  public:
    void start();
    void release();

    void update_style(std::string& exeName, std::string& styleContent);
  private:
    uWS::TemplatedApp<false>* app;
    std::vector<etws*> sockets;
    int port = 64132;
};

#endif /* SERVICE_SERVER_HPP */
