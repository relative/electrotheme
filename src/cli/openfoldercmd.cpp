#include <Windows.h>

#include "../config.hpp"
#include "cli.hpp"

void load_command_openfolder(CLI::App& app) {
	auto editconfig = app.add_subcommand(
			"openfolder", "Open electrotheme folder in File Explorer");
	editconfig->callback([&]() {
		auto stlFolderPath = gConfig->config_directory.string();
		auto chFolderPath = stlFolderPath.c_str();
		printf("Opening folder \"%s\" in File Explorer\n", chFolderPath);

		ShellExecuteA(nullptr, "open", chFolderPath, nullptr, nullptr,
									SW_SHOWDEFAULT);
	});
}
