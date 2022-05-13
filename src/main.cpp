#include <CLI/CLI.hpp>
#include <cstdio>
#include <vector>

#include "cli/cli.hpp"
#include "config.hpp"
#include "watcher.hpp"
#include "service/service.hpp"

int main(int argc, char** argv) {
  CLI::App app { "Simple Electron stylesheet loader", "electrotheme" };
  auto opt = new MainOptions();
  app
    .add_option("--config,-c", [](std::vector<std::string> dir) {
      gConfig->set_config_directory(dir.at(0));
      gConfig->load_file();
      return true;
    })
    ->default_val("")
    ->take_last()
    ->run_callback_for_default(true)
    ->force_callback(true);

  gConfig = new Config();

  load_command_app(app);
  load_command_editconfig(app);
  load_command_openfolder(app);
  load_command_startservice(app);

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }
}