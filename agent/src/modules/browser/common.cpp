#include "browser/common.h"
#include "internal/logging.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <array>
#include <memory>

namespace mceas {
namespace browser {

std::string getProfilePathMacOS(const std::string& relative_path) {
    // Получаем домашнюю директорию пользователя
    const char* home_dir = std::getenv("HOME");
    if (!home_dir) {
        LogError("Failed to get HOME environment variable");
        return "";
    }
    
    // Формируем путь к профилю
    std::string profile_path = std::string(home_dir) + "/" + relative_path;
    
    // Проверяем существование директории
    if (!std::filesystem::exists(profile_path)) {
        LogWarning("Profile directory does not exist: " + profile_path);
        return "";
    }
    
    return profile_path;
}

std::string getExecutablePathMacOS(const std::string& app_name) {
    // Проверяем стандартные директории для приложений
    std::vector<std::string> app_dirs = {
        "/Applications",
        "/Applications/Utilities",
        std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/Applications"
    };
    
    for (const auto& dir : app_dirs) {
        std::string app_path = dir + "/" + app_name + ".app";
        if (std::filesystem::exists(app_path)) {
            return app_path;
        }
    }
    
    LogWarning("Application not found: " + app_name);
    return "";
}

bool isBrowserInstalledMacOS(const std::string& app_name) {
    return !getExecutablePathMacOS(app_name).empty();
}

std::string getBrowserVersionMacOS(const std::string& app_name, const std::string& plist_key) {
    std::string app_path = getExecutablePathMacOS(app_name);
    if (app_path.empty()) {
        return "";
    }
    
    // Путь к Info.plist в пакете приложения
    std::string plist_path = app_path + "/Contents/Info.plist";
    
    if (!std::filesystem::exists(plist_path)) {
        LogWarning("Info.plist not found: " + plist_path);
        return "";
    }
    
    // Используем defaults для чтения значения из plist
    std::array<char, 128> buffer;
    std::string result;
    std::string command = "/usr/bin/defaults read \"" + plist_path + "\" " + plist_key;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        LogError("Failed to execute command: " + command);
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    // Удаляем лишние пробелы и переносы строк
    result.erase(0, result.find_first_not_of(" \n\r\t"));
    result.erase(result.find_last_not_of(" \n\r\t") + 1);
    
    return result;
}

} // namespace browser
} // namespace mceas