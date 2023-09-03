#include "server.hpp"

#include <uwebsockets/App.h>

#include <nlohmann/json.hpp>

#include "../config.hpp"
#include "../log.hpp"
#include "../util.hpp"

using json = nlohmann::json;

enum class MessageType { HELLO = 0, STYLES_UPDATE = 1 };

void Server::loop() {
#ifndef _DEBUG
  srand(static_cast<unsigned int>(time(0)));
  port = rand() % (65534 - 32768 + 1) + 32768;
  while (util::check_for_conflicting_ports(port)) {
    port = rand() % (65534 - 32768 + 1) + 32768;
  }
#endif
  using etws = uWS::WebSocket<false, true, PerSocketData>;
  app = std::make_unique<uWS::App>();

  app->ws<PerSocketData>(
         "/client",
         {
             .compression = uWS::SHARED_COMPRESSOR,
             .maxPayloadLength = 16 * 1024 * 1024,
             .idleTimeout = 16,
             .maxBackpressure = 1 * 1024 * 1024,
             .closeOnBackpressureLimit = false,
             .resetIdleTimeoutOnSend = false,
             .sendPingsAutomatically = true,

             .upgrade = nullptr,
             .open =
                 [this](etws* ws) {
                   DbgLog("WS connection received");
                   json p = {{"type", 3}};
                   ws->send(p.dump());
                 },
             .message =
                 [this](etws* ws, std::string_view message,
                        uWS::OpCode opCode) {
                   try {
                     auto payload = json::parse(message);
                     if (!payload.contains("type") ||
                         !payload["type"].is_number())
                       return;

                     auto type = payload["type"].get<MessageType>();
                     switch (type) {
                       case MessageType::HELLO: {
                         if (!payload.contains("exe") ||
                             !payload["exe"].is_string())
                           return;
                         auto executableName =
                             payload["exe"].get<std::string>();
                         try {
                           // throws if the app does not exist
                           auto app = gConfig->get_application_by_executable(
                               executableName);
                           DbgLog("WS connected for {}", app);
                           ws->subscribe(executableName);
                           auto style = app.get_style();
                           update_style(app.name, style);
                           DbgLog("WS for {} had style sent", app);
                         } catch (const std::exception& ex) {
                           __print(stderr,
                                   "Error while processing HELLO message from "
                                   "client: {}",
                                   ex.what());
                         }
                       } break;
                       default:
                         break;
                     }
                   } catch (const std::exception& ex) {
                     DbgLog("Failed to handle WebSocket message: {}",
                            ex.what());
                   }
                 },
             .drain = nullptr,
             .ping = nullptr,
             .pong = nullptr,
         })
      .listen(port, [](auto* listen_s) {
        if (listen_s) {
          DbgLog("Websocket listening");
        }
      });

  app->run();
}

Server::~Server() { uWS::Loop::get()->free(); }

void Server::update_style(std::string& exeName, std::string& styleContent) {
  DbgLog("Updating style for {} - styles {} length", exeName,
         styleContent.size());
  app->publish(
      exeName,
      json({{"type", MessageType::STYLES_UPDATE}, {"css", styleContent}})
          .dump(),
      uWS::OpCode::BINARY, false);
}
