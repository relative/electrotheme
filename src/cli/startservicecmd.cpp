#include "cli.hpp"
#include "../config.hpp"
#include "../watcher.hpp"
#include "../service/service.hpp"

void load_command_startservice(CLI::App& app) {
  auto startservice = app.add_subcommand("startservice", "Start service");

  startservice->callback([&]() {
    gService = new Service();

    // Only start the file watcher if we are running as service
    gWatcher = new Watcher(gConfig);
    gWatcher->start();

    // locks main thread
    gService->start();
  });
}