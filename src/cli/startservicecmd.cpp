#include "../config.hpp"
#include "../service/service.hpp"
#include "../watcher.hpp"
#include "cli.hpp"

void load_command_startservice(CLI::App& app) {
  auto startservice = app.add_subcommand("startservice", "Start service");

  startservice->callback([&]() {
    gService = std::make_unique<Service>();

    // Only start the file watcher if we are running as service
    gWatcher = std::make_unique<Watcher>();
    gWatcher->start();

    // locks main thread
    gService->start();
  });
}
