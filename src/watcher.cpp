#include "watcher.hpp"

#include <fstream>
#include <iostream>

#include "log.hpp"
#include "service/service.hpp"

std::unique_ptr<Watcher> gWatcher;

Watcher::Watcher() {
  watcher = new efsw::FileWatcher();
  watcherId = watcher->addWatch(gConfig->config_directory.string(), this, true);
}

void Watcher::start() { watcher->watch(); }

// efsw::FileWatchListener
void Watcher::handleFileAction(efsw::WatchID watchId, const std::string& dir,
                               const std::string& filename, efsw::Action action,
                               std::string oldFilename) {
  std::filesystem::path p(dir);

  // dir is inside of STYLES_DIRECTORY
  bool bStyles = false;
  std::string styleDir;
  for (const auto& it : p) {
    if (!bStyles && it.compare(STYLES_DIRECTORY) == 0) bStyles = true;
    if (bStyles) {
      styleDir = it.string();
    }
  }

  // ensure that CONFIG_FILE isn't triggering a config reload if it's
  // being modified inside of a style
  if (filename.compare(CONFIG_FILE) == 0 && !bStyles) {
    if (action != efsw::Actions::Modified) return;
    DbgLog("Config file {} modified, reloading configuration", filename);
    gConfig->load_file();
    return;
  }

  try {
    if (bStyles) {
      auto app = gConfig->get_application_by_directory(styleDir);

      // Ensure we are running in service
      // The watcher should not be created unless launched as a service
      if (gService == nullptr) return;

      auto style = app.get_style();
      gService->server->update_style(app.name, style);
    }
  } catch (std::exception& ex) {
    DbgLog("Error while processing file action: {}", ex.what());
  }
}
