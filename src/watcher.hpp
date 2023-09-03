#ifndef WATCHER_HPP
#define WATCHER_HPP

#include <efsw/efsw.hpp>

#include "config.hpp"

class Watcher : public efsw::FileWatchListener {
 public:
	Watcher(Config* config);

	void start();

	// efsw::FileWatchListener
	void handleFileAction(efsw::WatchID watchId, const std::string& dir,
												const std::string& filename, efsw::Action action,
												std::string oldFilename) override;

 private:
	efsw::FileWatcher* watcher;
	efsw::WatchID watcherId;
	Config* config;
};

extern Watcher* gWatcher;

#endif /* WATCHER_HPP */
