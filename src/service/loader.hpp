#ifndef SERVICE_LOADER_HPP
#define SERVICE_LOADER_HPP

#include <condition_variable>
#include <stdint.h>
#include <mutex>
#include <queue>

typedef struct loader_application_t {
  uint32_t processId;
  std::string executableName;
  bool removeCSP;
} LoaderApplication, *PLoaderApplication;

class Loader {
  public:
    Loader(): q(), m(), c() {}
  
    void start();
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
    std::queue<LoaderApplication> q;
    mutable std::mutex m;
    std::condition_variable c;

    std::string get_jsbundle(LoaderApplication& app);
    std::string get_scripts(LoaderApplication& app);
};

#endif /* SERVICE_LOADER_HPP */
