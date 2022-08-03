#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <vector>
#include <set>

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
} Application, *PApplication;

class Config {
  public:
    std::filesystem::path config_directory;
    std::filesystem::path styles_directory;
    std::filesystem::path scripts_directory;
    std::filesystem::path config_file;
    json config {};

    std::set<std::string> watchedExecutables;
    std::vector<Application> applications;

    void load_file();
    void save_file();
    void set_config_directory(std::string& configDirectory);
    Application* get_application_by_directory(const std::string& directory);
    Application* get_application_by_executable(const std::string& executable);
    std::string get_style_for_application(Application* app);
    std::string get_script_for_application(Application* app);
  private:
    void load_applications();
};

extern Config* gConfig;

#endif /* CONFIG_HPP */
