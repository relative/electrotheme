#include "config.hpp"
#include <ShlObj.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include "log.hpp"

Config* gConfig = nullptr;

void Config::set_config_directory(std::string& configDirectory) {
  if (configDirectory.empty()) {
    wchar_t* wcPath;
    auto status = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &wcPath);
    config_directory = wcPath;
    config_directory /= CONFIG_DIRECTORY;
  } else {
    config_directory = configDirectory;
  }
  styles_directory = config_directory / STYLES_DIRECTORY;
  config_file = config_directory / CONFIG_FILE;

  if (!std::filesystem::exists(config_directory)) {
    LOGDEBUGW(L"Creating config directory %s\n", config_directory.c_str());
    std::filesystem::create_directory(config_directory);
  }

  if (!std::filesystem::exists(styles_directory)) {
    LOGDEBUGW(L"Creating styles directory %s\n", styles_directory.c_str());
    std::filesystem::create_directory(styles_directory);
  }

  if (!std::filesystem::exists(config_file)) {
    LOGDEBUGW(L"Creating default config: %s\n", config_file.c_str());
    config = nlohmann::json::object();
    save_file();
  }
  LOGDEBUGW(L"Config directory: %s\n", config_directory.c_str());
}

void Config::load_file() {
  LOGDEBUGW(L"Loading config from file %s\n", config_file.c_str());
  try {
    std::ifstream ifs(config_file);
    std::stringstream buf;
    buf << ifs.rdbuf();
    config = json::parse(buf);
    ifs.close();
  } catch (const std::exception& ex) {
    fprintf(stderr, "Failed to read config from disk\n%s\n\nExiting.\n", ex.what());
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
    if (!config.contains("applications") || !jsonApplications.is_array()) return;

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

      // css path
      if (e.contains("style") && e["style"].is_string()) {
        app.style = e["style"].get<std::string>();
      } else {
        app.style = "index.css";
      }

      if (e.contains("removeCSP") && e["removeCSP"].is_boolean()) {
        app.removeCSP = e["removeCSP"].get<bool>();
        if (app.removeCSP) {
          fprintf(stderr, "!! %s will have its Content-Security-Policy removed to allow for unsafe CSS @imports !!\n", app.name.c_str());
        }
      } else {
        app.removeCSP = false;
      }

      LOGDEBUG("Adding %s (%s) to Config applications\n", app.name.c_str(), app.directory.c_str());

      watchedExecutables.insert(app.name);
      applications.push_back(app);
    }
  } catch(const std::exception& ex) {
    fprintf(stderr, "Failed to load applications from config: %s\n", ex.what());
  }
}

Application* Config::get_application_by_directory(const std::string& directory) {
  auto it = std::find_if(applications.begin(), applications.end(),
    [directory](const Application& a) { return a.directory.compare(directory) == 0; });
  if (it == applications.end())
    throw std::runtime_error("Application with directory \"" + directory + "\" does not exist");
  return &*it;
}
Application* Config::get_application_by_executable(const std::string& executable) {
  auto it = std::find_if(applications.begin(), applications.end(),
    [executable](const Application& a) { return a.name.compare(executable) == 0; });
  if (it == applications.end())
    throw std::runtime_error("Application with executable name \"" + executable + "\" does not exist");
  return &*it;
}

std::string Config::get_style_for_application(Application* app) {
  std::filesystem::path p = styles_directory / app->directory / app->style;
  std::ifstream ifs(p);
  std::stringstream buf;
  
  buf << ifs.rdbuf();
  auto str = buf.str();

  return str;
}