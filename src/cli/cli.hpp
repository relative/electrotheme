#ifndef CLI_CLI_HPP
#define CLI_CLI_HPP

#include <CLI/CLI.hpp>

struct MainOptions {
  std::string configDirectory;
};

void load_command_app(CLI::App& app);
void load_command_editconfig(CLI::App& app);
void load_command_openfolder(CLI::App& app);
void load_command_startservice(CLI::App& app);

#endif /* CLI_CLI_HPP */
