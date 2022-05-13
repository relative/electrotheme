#include "loader.hpp"
#include "../util.hpp"
#include "node.hpp"
#include <Windows.h>
#include <chrono>
#include <thread>
#include "../dependencies/easywsclient/easywsclient.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include "../js/bundle.hpp"

#define MAX_RETRIES 30

using json = nlohmann::json;

namespace {
  size_t curlwrite_callbackfunc_stdstring(void* contents, const size_t size, const size_t nmemb, std::string* s) {
    const auto new_length = size * nmemb;
    try {
      s->append(static_cast<char*>(contents), new_length);
    } catch (const std::bad_alloc& e) {
      return 0;
    }
    return new_length;
  }
  std::string http_get(const char* url, int port, int* res) {
    CURL* req = curl_easy_init();
    std::string s;

    curl_easy_setopt(req, CURLOPT_URL, url);
    curl_easy_setopt(req, CURLOPT_PORT, port);
    curl_easy_setopt(req, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/103.0.5042.0 Safari/537.36");
    curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, curlwrite_callbackfunc_stdstring);
    curl_easy_setopt(req, CURLOPT_WRITEDATA, &s);
    *res = curl_easy_perform(req);

    curl_easy_cleanup(req);
    
    return s;
  }
}

class Inspector {
  public:
    Inspector(std::string wsUrl) {
      this->wsUrl = wsUrl;
      this->ws = std::unique_ptr<easywsclient::WebSocket>(easywsclient::WebSocket::from_url(wsUrl));
    }
    void sendRaw(json payload) {
      payload["id"] = messageId++;
      ws->send(payload.dump());
    }
    void send(std::string method) {
      sendRaw({
        {"method", method}
      });
    }
    void send(std::string method, json params) {
      sendRaw({
        {"method", method},
        {"params", params}
      });
    }
    bool poll() {
      if (ws->getReadyState() == easywsclient::WebSocket::CLOSED) return false;
      
      easywsclient::WebSocket::pointer wsp = &*ws;
      ws->poll();
      ws->dispatch([this, wsp](const std::string& message) {
        try {
          auto p = json::parse(message);
          if (!p.contains("method")) return;
        } catch (const std::exception& ex) {
          fprintf(stderr, "Failed to handle message from v8 inspector: %s\n", ex.what());
        }
      });
      return true;
    }

    std::unique_ptr<easywsclient::WebSocket> ws;
    int messageId = 0;
  private:
    std::string wsUrl;
};

std::string Loader::get_jsbundle(LoaderApplication& app) {
  json options = {
    {"executableName", app.executableName},
    {"pid", app.processId},
    {"removeCSP", app.removeCSP}
  };
  auto optionsStr = options.dump();
  auto pos = 0;
  while (pos = optionsStr.find("`", pos) != std::string::npos) {
    optionsStr.replace(pos, 1, "\\`");
    pos += 2;
  }
  std::string preamble = "(globalThis || global).electrothemeOptions = JSON.parse(`" + optionsStr + "`);\n";
  std::string bundle(jsbundle, jsbundle + jsbundle_size);
  return preamble + bundle;
}

void Loader::start() {
  while (true) {
    auto app = dequeue();
    try {
      process_application(app);
    } catch (const std::exception& ex) {
      fprintf(stderr, "Loader failed to process %s (%i): %s\n", app.executableName.c_str(), app.processId, ex.what());
    }
  }
}

void Loader::process_application(LoaderApplication& app) {
  HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | 
    PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, 
    false, app.processId);

  if (process == nullptr)
    throw std::runtime_error("Failed to open process\n" + util::get_last_error());

  // Wait a second for node to create the debugger address file mapping, maybe we were too fast!
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
      fprintf(stderr, "Error while attempting to acquire %s's WebSocket URL: %s\n", app.executableName.c_str(), ex.what());
      return;
    }
  }

  if (curlCode != CURLE_OK)
    throw std::runtime_error("Could not acquire WebSocket URL for process; exceeded max retries");

  auto inspector = new Inspector(wsUrl);
  
  // Enable the Runtime domain to evaluate JS
  inspector->send("Runtime.enable");

  bool sentStyleInjector = false;
  int sentDebugEnd = 0;
  while (inspector->poll()) {
    if (!sentStyleInjector){
      sentStyleInjector = true;
      inspector->send("Runtime.evaluate", {
        {"expression", get_jsbundle(app)},
        {"includeCommandLineAPI", true} // so we can use CJS require to get electron.app
      });
    } else {
      inspector->send("Runtime.evaluate", {
        {"expression", "process._debugEnd();"},
        {"includeCommandLineAPI", true}
      });
      if (++sentDebugEnd > 5) {
        // likely we are stuck so we should manually close the websocket
        // hopefully it recovers and the port is closed, else we may have to terminate the process
        inspector->ws->close();
      }
    }
  }

  printf("Done injecting into process %s\n", app.executableName.c_str());
}