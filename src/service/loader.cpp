#include "loader.hpp"

#include <Windows.h>
#include <curl/curl.h>
#include <curl/websockets.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <queue>
#include <thread>

#include "../config.hpp"
#include "../js/bundle.hpp"
#include "../log.hpp"
#include "../util.hpp"
#include "node.hpp"
#include "service.hpp"

#define MAX_RETRIES 30
#define ASSERT_CURLCODE(Result_)        \
  res = Result_;                        \
  DbgLog("{}  =  {}\n", #Result_, res); \
  if (res != CURLE_OK)                  \
    throw std::runtime_error(std::format("curl error {}", res));

using json = nlohmann::json;

template <>
struct std::formatter<CURLcode> : formatter<string_view> {
  template <typename Context>
  auto format(const CURLcode& code, Context& ctx) {
    return formatter<string_view>::format(
        std::format("{} ({})", std::to_underlying(code),
                    curl_easy_strerror(code)),
        ctx);
  }
};

namespace {
  size_t curlwrite_callbackfunc_stdstring(void* contents, const size_t size,
                                          const size_t nmemb, std::string* s) {
    const auto new_length = size * nmemb;
    try {
      s->append(static_cast<char*>(contents), new_length);
    } catch (const std::bad_alloc& e) {
      return 0;
    }
    return new_length;
  }
  std::string http_get(const char* url, int port, int* ress) {
    CURL* req = curl_easy_init();
    std::string s;

    CURLcode res = CURLE_OK;

    ASSERT_CURLCODE(curl_easy_setopt(req, CURLOPT_URL, url));
    ASSERT_CURLCODE(curl_easy_setopt(req, CURLOPT_PORT, port));
    ASSERT_CURLCODE(curl_easy_setopt(
        req, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
        "like Gecko) Chrome/103.0.5042.0 Safari/537.36"));
    ASSERT_CURLCODE(curl_easy_setopt(req, CURLOPT_WRITEFUNCTION,
                                     curlwrite_callbackfunc_stdstring));
    ASSERT_CURLCODE(curl_easy_setopt(req, CURLOPT_WRITEDATA, &s));
    *ress = curl_easy_perform(req);

    curl_easy_cleanup(req);

    return s;
  }
}  // namespace

class Inspector {
 public:
  Inspector(std::string wsUrl) : req(curl_easy_init()) {
    this->wsUrl = wsUrl;
    CURLcode res = CURLE_OK;
    ASSERT_CURLCODE(curl_easy_setopt(req, CURLOPT_URL, wsUrl.c_str()));
    ASSERT_CURLCODE(
        curl_easy_setopt(req, CURLOPT_CONNECT_ONLY, 2L /* WebSocket */));
    ASSERT_CURLCODE(curl_easy_perform(req));
  }
  ~Inspector() { close(); }
  int sendRaw(json payload) {
    CURLcode res = CURLE_OK;
    auto id = messageId++;
    payload["id"] = lastSentId = id;
    auto pl = payload.dump();
    ASSERT_CURLCODE(
        curl_ws_send(req, pl.c_str(), pl.size(), &sent, 0, CURLWS_TEXT));
    return id;
  }
  int send(std::string method) { return sendRaw({{"method", method}}); }
  int send(std::string method, json params) {
    return sendRaw({{"method", method}, {"params", params}});
  }
  bool poll() {
    if (req == nullptr) return false;

    size_t rlen;
    struct curl_ws_frame* oMeta;
    CURLcode res = curl_ws_recv(req, nullptr, 0, &rlen, &oMeta);
    /*if (res != CURLE_OK && res != CURLE_AGAIN)
      DbgLog("curl_ws_recv: {}", res);*/
    switch (res) {
      case CURLE_OK: {
        if (oMeta != nullptr) {
          if (oMeta->bytesleft) {
            char* buf = new char[oMeta->bytesleft];
            q.resize(oMeta->len);
            struct curl_ws_frame* meta;

            res = curl_ws_recv(req, buf, oMeta->bytesleft, &rlen, &meta);

            if (res != CURLE_OK) {
              __print(stderr, "ws error {} !!!!!!", res);
              delete[] buf;
              return false;
            } else {
              q.insert(q.begin() + meta->offset, buf, buf + rlen);
              if (meta->bytesleft == 0) parseMessage(std::move(q));
              delete[] buf;
            }
          }
        }
      } break;
      case CURLE_GOT_NOTHING:
      case CURLE_RECV_ERROR:
        // closing
        return false;
      default:
        break;
    }

    return true;
  }
  void close() {
    if (req == nullptr) return;
    CURLcode res = CURLE_OK;
    ASSERT_CURLCODE(curl_ws_send(req, "", 0, &sent, 0, CURLWS_CLOSE));
    curl_easy_cleanup(req);
    req = nullptr;
  }
  int messageId = 0;
  int lastSentId = -1;
  int lastReplyId = -2;
  json lastReply;

 private:
  std::string wsUrl;
  CURL* req;
  std::vector<char> q;
  size_t sent;

  void parseMessage(const std::vector<char>&& vec) {
    std::string msg(vec.data(), vec.size());
    try {
      auto p = json::parse(msg);
      DbgLog("Message from v8 inspector: {}", p.dump());
      if (p.contains("id")) {
        lastReplyId = p["id"].get<int>();
        lastReply = p;
      }
      if (!p.contains("method")) return;
    } catch (const std::exception& ex) {
      __print(stderr, "Failed to handle message from v8 inspector: {}",
              ex.what());
    }
  }

  // static cb_write(?)
};

std::string Loader::get_jsbundle(LoaderApplication& app) {
  json options = {{"executableName", app.executableName},
                  {"pid", app.processId},
                  {"removeCSP", app.removeCSP},
                  {"port", gService->server->port}};
  auto optionsStr = options.dump();
  std::string preamble =
      "(globalThis || global).electrothemeOptions = " + optionsStr + ";\n";
  std::string bundle(jsbundle, jsbundle + jsbundle_size);
  return preamble + bundle;
}

std::string Loader::get_scripts(LoaderApplication& app) {
  auto app_ = gConfig->get_application_by_executable(app.executableName);
  return app_.get_script();
}

void Loader::loop() {
  while (true) {
    auto app = dequeue();
    try {
      process_application(app);
    } catch (const std::exception& ex) {
      __print(stderr, "Loader failed to process {} ({}): {}",
              app.executableName, app.processId, ex.what());
    }
  }
}

void Loader::process_application(LoaderApplication& app) {
  HANDLE process =
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                  false, app.processId);

  if (process == nullptr)
    throw std::runtime_error("Failed to open process\n" +
                             util::get_last_error());

  // TODO: Wait a second for node to create the debugger address file mapping,
  // maybe we were too fast!
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  node_debug_process(app.processId, process);
  CloseHandle(process);

  // attempt to request 127.0.0.1:9229/json/list until it succeeds
  int retries = 0;
  int curlCode = -1;
  std::string wsUrl;
  for (int i = 0; i < MAX_RETRIES; ++i) {
    try {
      auto res = http_get("http://127.0.0.1/json/list", 9229, &curlCode);
      if (curlCode != CURLE_OK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        continue;
      }
      auto list_json = json::parse(res);
      wsUrl = list_json[0]["webSocketDebuggerUrl"].get<std::string>();
    } catch (const std::exception& ex) {
      __print(stderr,
              "Error while attempting to acquire {}'s WebSocket URL: {}",
              app.executableName, ex.what());
      return;
    }
  }

  if (curlCode != CURLE_OK)
    throw std::runtime_error(
        "Could not acquire WebSocket URL for process; exceeded max retries");

  auto inspector = std::make_unique<Inspector>(wsUrl);

  // Enable the Runtime domain to evaluate JS
  inspector->send("Runtime.enable");

  auto appScript = get_scripts(app);
  bool sentScripts = appScript == "";

  int scriptsId = -1;
  int stylesId = -1;
  bool lastMessageReplied = false;
  while (inspector->poll()) {
    // DbgLog("{} / {} / {}", inspector->messageId, inspector->lastSentId,
    //        inspector->lastReplyId);
    lastMessageReplied = inspector->lastReplyId == inspector->lastSentId;
    if (lastMessageReplied) {
      if (!sentScripts) {
        sentScripts = true;
        DbgLog("sending scripts");
        inspector->send(
            "Runtime.evaluate",
            {
                {"expression", appScript},
                {"includeCommandLineAPI",
                 true}  // so we can use CJS require to get electron.app
            });
      } else {
        DbgLog("sending styles");
        stylesId = inspector->send(
            "Runtime.evaluate",
            {
                {"expression", get_jsbundle(app) + ";process._debugEnd();"},
                {"includeCommandLineAPI",
                 true}  // so we can use CJS require to get electron.app
            });
        inspector->close();
      }
    }
  }

  __print(stdout, "Done injecting into process {}", app);
}
