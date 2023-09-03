#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <filesystem>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#define CONFIG_DIRECTORY "electrotheme"
#define STYLES_DIRECTORY "styles"
#define SCRIPTS_DIRECTORY "scripts"
#define CONFIG_FILE "config.json"

using json = nlohmann::json;

typedef struct application_t {
  std::string name;
  std::string directory;
  std::string style;
  std::string script;
  bool removeCSP;

  std::string get_style();
  std::string get_script();
} Application, *PApplication;

template <>
struct std::formatter<application_t> : std::formatter<std::string_view> {
  template <class FormatContext>
  auto format(application_t a, FormatContext& ctx) const {
    return formatter<string_view>::format(
        std::format("{} ({})", a.name, a.directory), ctx);
  }
};

class Config {
 public:
  std::filesystem::path config_directory;
  std::filesystem::path styles_directory;
  std::filesystem::path scripts_directory;
  std::filesystem::path config_file;
  json config{};

  std::set<std::string> watchedExecutables;
  std::vector<Application> applications;

  void load_file(bool silent = false);
  void save_file();
  void set_config_directory(std::string& configDirectory);
  Application& get_application_by_directory(const std::string& directory);
  Application& get_application_by_executable(const std::string& executable);

 private:
  void load_applications();
};

extern std::unique_ptr<Config> gConfig;

#endif /* CONFIG_HPP */
