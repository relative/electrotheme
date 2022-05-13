#include "server.hpp"
#include "../log.hpp"
#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>
#include "../config.hpp"

using json = nlohmann::json;

enum class MessageType {
  HELLO = 0,
  STYLES_UPDATE = 1
};

void Server::start() {
  using uws = uWS::WebSocket<false, true, PerSocketData>;
  app = new uWS::App();

  app->ws<PerSocketData>("/client", {
    .compression = uWS::SHARED_COMPRESSOR,
    .maxPayloadLength = 16 * 1024 * 1024,
    .idleTimeout = 16,
    .maxBackpressure = 1 * 1024 * 1024,
    .closeOnBackpressureLimit = false,
    .resetIdleTimeoutOnSend = false,
    .sendPingsAutomatically = true,

    .upgrade = nullptr,
    .open = [this](etws* ws) {
      LOGDEBUG("WS connection received\n");
      json p = {
        {"type", 3}
      };
      ws->send(p.dump());
      sockets.push_back(ws);
    },
    .message = [this](etws* ws, std::string_view message, uWS::OpCode opCode) {
      try {
        auto psd = reinterpret_cast<PerSocketData*>(ws->getUserData());
        psd->executableName = "";
        auto payload = json::parse(message);
        if (!payload.contains("type") || !payload["type"].is_number()) return;
        
        auto type = payload["type"].get<MessageType>();

        switch (type) {
          case MessageType::HELLO:
            if (psd->ready) return;
            if (!payload.contains("exe") || !payload["exe"].is_string()) return;
            psd->executableName = payload["exe"].get<std::string>();
            try {
              // throws if the app does not exist
              auto app = gConfig->get_application_by_executable(psd->executableName);
              psd->ready = true;
              LOGDEBUG("WS connected for %s (directory = %s, file = %s)\n", 
                app->name.c_str(), app->directory.c_str(), app->style.c_str());
              auto style = gConfig->get_style_for_application(app);
              update_style(app->name, style);
              LOGDEBUG("WS for %s had style sent\n", app->name.c_str());
            } catch (const std::exception& ex) {
              fprintf(stderr, "Error while processing HELLO message from client: %s\n", ex.what());
            }
            break;
          default:
            break;
        }
      } catch (const std::exception& ex) {
        WARNDEBUG("Failed to handle WebSocket message: %s\n", ex.what());
      }
    },
    .drain = nullptr,
    .ping = nullptr,
    .pong = nullptr,
    .close = [this](etws* ws, int code, std::string_view reason) {
      auto it = std::find_if(sockets.begin(), sockets.end(), [&ws](etws* vs) {
        return vs == ws;
      });
      if (it == sockets.end()) return;
      sockets.erase(it);
    }
  }).listen(port, [](auto* listen_s) {
    if (listen_s) {
      LOGDEBUG("Websocket listening\n");
    }
  });

  app->run();
}

void Server::release() {
  delete app;
  uWS::Loop::get()->free();
}

void Server::update_style(std::string& exeName, std::string& styleContent) {
  LOGDEBUG("Updating style for %s - styles %zi length\n", exeName.c_str(), styleContent.size());
  json p = {
    {"type", MessageType::STYLES_UPDATE},
    {"css", styleContent}
  };

  auto payload = p.dump();

  for (const auto& ws : sockets) {
    auto psd = ws->getUserData();
    if (!psd->ready) continue;
    
    if (psd->executableName.compare(exeName) != 0) continue;
    ws->send(payload, uWS::OpCode::BINARY, false, true);
  }
}