#ifndef SERVICE_LOADER_HPP
#define SERVICE_LOADER_HPP

#include <stdint.h>

#include <condition_variable>
#include <format>
#include <mutex>
#include <queue>

typedef struct loader_application_t {
  uint32_t processId;
  std::string executableName;
  bool removeCSP;
} LoaderApplication, *PLoaderApplication;

template <>
struct std::formatter<loader_application_t> : std::formatter<std::string_view> {
  template <class FormatContext>
  auto format(loader_application_t a, FormatContext& ctx) const {
    return formatter<string_view>::format(
        std::format("{} ({})", a.executableName, a.processId), ctx);
  }
};

class Loader {
 public:
  Loader() : q(), m(), c() { thread = std::thread(&Loader::loop, this); }

  void process_application(LoaderApplication& app);

  inline void enqueue(LoaderApplication app) {
    std::lock_guard<std::mutex> lock(m);
    q.push(app);
    c.notify_one();
  }

  inline LoaderApplication dequeue() {
    std::unique_lock<std::mutex> lock(m);
    while (q.empty()) {
      c.wait(lock);
    }
    auto app = q.front();
    q.pop();
    return app;
  }

 private:
  std::thread thread;
  std::queue<LoaderApplication> q;
  mutable std::mutex m;
  std::condition_variable c;

  std::string get_jsbundle(LoaderApplication& app);
  std::string get_scripts(LoaderApplication& app);

  void loop();
};

#endif /* SERVICE_LOADER_HPP */
