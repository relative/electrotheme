#include <Windows.h>
#include <shellapi.h>

#include "../config.hpp"
#include "../log.hpp"
#include "cli.hpp"

void load_command_editconfig(CLI::App& app) {
  auto editconfig =
      app.add_subcommand("editconfig", "Open config file in default editor");
  editconfig->callback([&]() {
    auto stlConfigPath = gConfig->config_file.string();
    auto chConfigPath = stlConfigPath.c_str();
    __print(stdout, "Opening config file \"{}\"", chConfigPath);

    ShellExecuteA(nullptr, "open", chConfigPath, nullptr, nullptr,
                  SW_SHOWDEFAULT);
  });
}
