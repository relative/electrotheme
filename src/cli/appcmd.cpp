#include "cli.hpp"
#include <Windows.h>
#include "../config.hpp"

void load_command_app(CLI::App& app) {
  auto cmd = app.add_subcommand("app", "Create/manage applications");

  auto create = cmd->add_subcommand("create", "Create application");
  auto remove = cmd->add_subcommand("remove", "Remove application");
  auto list = cmd->add_subcommand("list", "List applications");
}