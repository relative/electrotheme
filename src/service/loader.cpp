#include "loader.hpp"

#include <Windows.h>
#include <curl/curl.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

#include "../config.hpp"
#include "../dependencies/easywsclient/easywsclient.hpp"
#include "../js/bundle.hpp"
#include "../log.hpp"
#include "../util.hpp"
#include "node.hpp"
#include "service.hpp"

#define MAX_RETRIES 30

using json = nlohmann::json;

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
	std::string http_get(const char* url, int port, int* res) {
		CURL* req = curl_easy_init();
		std::string s;

		curl_easy_setopt(req, CURLOPT_URL, url);
		curl_easy_setopt(req, CURLOPT_PORT, port);
		curl_easy_setopt(
				req, CURLOPT_USERAGENT,
				"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
				"like Gecko) Chrome/103.0.5042.0 Safari/537.36");
		curl_easy_setopt(req, CURLOPT_WRITEFUNCTION,
										 curlwrite_callbackfunc_stdstring);
		curl_easy_setopt(req, CURLOPT_WRITEDATA, &s);
		*res = curl_easy_perform(req);

		curl_easy_cleanup(req);

		return s;
	}
}	 // namespace

class Inspector {
 public:
	Inspector(std::string wsUrl) {
		this->wsUrl = wsUrl;
		this->ws = std::unique_ptr<easywsclient::WebSocket>(
				easywsclient::WebSocket::from_url(wsUrl));
	}
	int sendRaw(json payload) {
		auto id = messageId++;
		payload["id"] = id;
		ws->send(payload.dump());
		return id;
	}
	int send(std::string method) { return sendRaw({{"method", method}}); }
	int send(std::string method, json params) {
		return sendRaw({{"method", method}, {"params", params}});
	}
	bool poll(int timeout = 0) {
		if (ws->getReadyState() == easywsclient::WebSocket::CLOSED) return false;

		easywsclient::WebSocket::pointer wsp = &*ws;
		ws->dispatch([this, wsp](const std::string& message) {
			try {
				auto p = json::parse(message);
				LOGDEBUG("Message from v8 inspector: %s\n", p.dump().c_str());
				if (p.contains("id")) {
					lastReplyId = p["id"].get<int>();
					lastReply = p;
				}
				if (!p.contains("method")) return;
			} catch (const std::exception& ex) {
				fprintf(stderr, "Failed to handle message from v8 inspector: %s\n",
								ex.what());
			}
		});
		ws->poll(timeout);

		return true;
	}

	std::unique_ptr<easywsclient::WebSocket> ws;
	int messageId = 0;
	int lastReplyId = -1;
	json lastReply;

 private:
	std::string wsUrl;
};

std::string Loader::get_jsbundle(LoaderApplication& app) {
	json options = {{"executableName", app.executableName},
									{"pid", app.processId},
									{"removeCSP", app.removeCSP},
									{"port", gService->server->port}};
	auto optionsStr = options.dump();
	auto pos = 0;
	while (pos = optionsStr.find("`", pos) != std::string::npos) {
		optionsStr.replace(pos, 1, "\\`");
		pos += 2;
	}
	std::string preamble =
			"(globalThis || global).electrothemeOptions = JSON.parse(`" + optionsStr +
			"`);\n";
	std::string bundle(jsbundle, jsbundle + jsbundle_size);
	return preamble + bundle;
}

std::string Loader::get_scripts(LoaderApplication& app) {
	auto app_ = gConfig->get_application_by_executable(app.executableName);
	return gConfig->get_script_for_application(app_);
}

void Loader::start() {
	while (true) {
		auto app = dequeue();
		try {
			process_application(app);
		} catch (const std::exception& ex) {
			fprintf(stderr, "Loader failed to process %s (%i): %s\n",
							app.executableName.c_str(), app.processId, ex.what());
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

	// Wait a second for node to create the debugger address file mapping, maybe
	// we were too fast!
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
			fprintf(stderr,
							"Error while attempting to acquire %s's WebSocket URL: %s\n",
							app.executableName.c_str(), ex.what());
			return;
		}
	}

	if (curlCode != CURLE_OK)
		throw std::runtime_error(
				"Could not acquire WebSocket URL for process; exceeded max retries");

	auto inspector = new Inspector(wsUrl);

	// Enable the Runtime domain to evaluate JS
	inspector->send("Runtime.enable");

	auto appScript = get_scripts(app);
	bool sentScripts = appScript == "";
	bool sentScripts_ = false;
	bool sentStyleInjector = false;
	bool sentStyleInjector_ = false;
	int sentDebugEnd = 0;

	int scriptsId = -1;
	int stylesId = -1;

	while (inspector->poll(500)) {
		LOGDEBUG("\npoll(), scriptsId = %i\nstylesId = %i\nlastReplyId = %i\n",
						 scriptsId, stylesId, inspector->lastReplyId);
		if (scriptsId > 0 && !sentScripts) {
			if (inspector->lastReplyId >= scriptsId)
				sentScripts = true;
			else
				continue;
		} else if (stylesId > 0 && !sentStyleInjector) {
			if (inspector->lastReplyId >= stylesId)
				sentStyleInjector = true;
			else
				continue;
		}
		if (!sentScripts) {
			// sentScripts = true;
			LOGDEBUG("sending scripts\n");
			scriptsId = inspector->send(
					"Runtime.evaluate",
					{
							{"expression", appScript},
							{"includeCommandLineAPI",
							 true}	// so we can use CJS require to get electron.app
					});
		} else if (!sentStyleInjector) {
			// sentStyleInjector = true;
			LOGDEBUG("sending styles\n");
			stylesId = inspector->send(
					"Runtime.evaluate",
					{
							{"expression", get_jsbundle(app)},
							{"includeCommandLineAPI",
							 true}	// so we can use CJS require to get electron.app
					});
		} else {
			LOGDEBUG("sending debugEnd\n");
			inspector->send("Runtime.evaluate",
											{{"expression", "process._debugEnd();"},
											 {"includeCommandLineAPI", true}});
			if (++sentDebugEnd > 5) {
				// likely we are stuck so we should manually close the websocket
				// hopefully it recovers and the port is closed, else we may have to
				// terminate the process
				inspector->ws->close();
			}
		}
	}

	printf("Done injecting into process %s\n", app.executableName.c_str());
}
