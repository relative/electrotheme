#include "watcher.hpp"

#include <fstream>
#include <iostream>

#include "log.hpp"
#include "service/service.hpp"

Watcher* gWatcher = nullptr;

Watcher::Watcher(Config* config) {
	this->config = config;

	watcher = new efsw::FileWatcher();
	watcherId = watcher->addWatch(config->config_directory.string(), this, true);
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
		LOGDEBUG("Config file %s modified, reloading configuration\n",
						 filename.c_str());
		config->load_file();
		return;
	}

	if (bStyles) {
		auto app = config->get_application_by_directory(styleDir);

		// Ensure we are running in service
		// The watcher should not be created unless launched as a service
		if (gService == nullptr) return;

		auto style = config->get_style_for_application(app);
		gService->server->update_style(app->name, style);
	}
}
