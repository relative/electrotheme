#include "config.hpp"

#include <ShlObj.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "log.hpp"

std::unique_ptr<Config> gConfig;

template <>
struct std::formatter<std::filesystem::path>
    : std::formatter<std::string_view> {
  template <class FormatContext>
  auto format(std::filesystem::path a, FormatContext& ctx) const {
    std::wstring h = a.c_str();
    return formatter<string_view>::format(std::string(h.begin(), h.end()), ctx);
  }
};

void Config::set_config_directory(std::string& configDirectory) {
  if (configDirectory.empty()) {
    wchar_t* wcPath;
    auto status = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE,
                                       nullptr, &wcPath);
    config_directory = wcPath;
    config_directory /= CONFIG_DIRECTORY;
  } else {
    config_directory = configDirectory;
  }
  styles_directory = config_directory / STYLES_DIRECTORY;
  scripts_directory = config_directory / SCRIPTS_DIRECTORY;
  config_file = config_directory / CONFIG_FILE;

  if (!std::filesystem::exists(config_directory)) {
    DbgLog("Creating config directory {}", config_directory);
    std::filesystem::create_directory(config_directory);
  }

  if (!std::filesystem::exists(styles_directory)) {
    DbgLog("Creating styles directory {}", styles_directory);
    std::filesystem::create_directory(styles_directory);
  }

  if (!std::filesystem::exists(scripts_directory)) {
    DbgLog("Creating scripts directory {}", scripts_directory);
    std::filesystem::create_directory(scripts_directory);
  }

  if (!std::filesystem::exists(config_file)) {
    DbgLog("Creating default config: {}", config_file);
    config = nlohmann::json::object();
    save_file();
  }
  DbgLog("Config directory: {}", config_directory);
}

void Config::load_file() {
  DbgLog("Loading config from file {}", config_file);
  try {
    std::ifstream ifs(config_file);
    std::stringstream buf;
    buf << ifs.rdbuf();
    config = json::parse(buf);
    ifs.close();
  } catch (const std::exception& ex) {
    __print(stderr, "Failed to read config from disk\n{}\n\nExiting.",
            ex.what());
    exit(1);
  }
  if (config == nullptr) return;

  load_applications();
}

void Config::save_file() {
  std::ofstream ofs;
  ofs.open(config_file, std::ios::out);
  ofs << config.dump();
  ofs.close();
}

void Config::load_applications() {
  try {
    auto jsonApplications = config["applications"];
    if (!config.contains("applications") || !jsonApplications.is_array())
      return;

    watchedExecutables.clear();
    applications.clear();

    for (const auto& e : jsonApplications) {
      auto jsonName = e["name"];
      auto jsonDirectory = e["directory"];
      if (!e.contains("name") || !e.contains("directory")) continue;
      if (!jsonName.is_string() || !jsonDirectory.is_string()) continue;

      Application app;
      app.name = e["name"].get<std::string>();
      app.directory = e["directory"].get<std::string>();

      if (app.directory != "") {
        auto appStyleDir = styles_directory / app.directory;
        auto appScriptDir = scripts_directory / app.directory;
        if (!std::filesystem::exists(appStyleDir)) {
          DbgLog("Creating application style directory {}", appStyleDir);
          std::filesystem::create_directory(appStyleDir);
        }
        if (!std::filesystem::exists(appScriptDir)) {
          DbgLog("Creating application script directory {}", appScriptDir);
          std::filesystem::create_directory(appScriptDir);
        }
      }

      // css path
      if (e.contains("style") && e["style"].is_string()) {
        app.style = e["style"].get<std::string>();
      } else {
        app.style = "index.css";
      }

      if (e.contains("script") && e["script"].is_string()) {
        app.script = e["script"].get<std::string>();
      } else {
        app.script = "index.js";
      }

      if (e.contains("removeCSP") && e["removeCSP"].is_boolean()) {
        app.removeCSP = e["removeCSP"].get<bool>();
        if (app.removeCSP) {
          __print(stderr,
                  "!! {} will have its Content-Security-Policy removed to "
                  "allow for unsafe CSS @imports !!",
                  app.name);
        }
      } else {
        app.removeCSP = false;
      }

      DbgLog("Adding {} to Config applications", app);

      watchedExecutables.insert(app.name);
      applications.push_back(app);
    }
  } catch (const std::exception& ex) {
    __print(stderr, "Failed to load applications from config: {}", ex.what());
  }
}

Application& Config::get_application_by_directory(
    const std::string& directory) {
  auto it = std::find_if(applications.begin(), applications.end(),
                         [directory](const Application& a) {
                           return a.directory.compare(directory) == 0;
                         });
  if (it == applications.end())
    throw std::runtime_error("Application with directory \"" + directory +
                             "\" does not exist");
  return *it;
}
Application& Config::get_application_by_executable(
    const std::string& executable) {
  auto it = std::find_if(applications.begin(), applications.end(),
                         [executable](const Application& a) {
                           return a.name.compare(executable) == 0;
                         });
  if (it == applications.end())
    throw std::runtime_error("Application with executable name \"" +
                             executable + "\" does not exist");
  return *it;
}

std::string application_t::get_style() {
  std::filesystem::path p = gConfig->styles_directory / directory / style;
  std::ifstream ifs(p);
  std::stringstream buf;

  buf << ifs.rdbuf();
  auto str = buf.str();

  return str;
}

std::string application_t::get_script() {
  std::filesystem::path p = gConfig->scripts_directory / directory / script;
  std::ifstream ifs(p);
  std::stringstream buf;

  buf << ifs.rdbuf();
  auto str = buf.str();

  return str;
}
