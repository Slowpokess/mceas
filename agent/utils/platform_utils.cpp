#include "utils/platform_utils.h"
#include "internal/logging.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <array>
#include <memory>

#if defined(_WIN32)
    #include <windows.h>
    #include <shlobj.h>
#elif defined(__APPLE__) || defined(__linux__)
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
#endif

namespace mceas {
namespace utils {

std::string GetHomeDir() {
#if defined(_WIN32)
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return std::string(path);
    }
    
    char* homeDrive = getenv("HOMEDRIVE");
    char* homePath = getenv("HOMEPATH");
    if (homeDrive && homePath) {
        return std::string(homeDrive) + std::string(homePath);
    }
    
    return "";
#else
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home);
    }
    
    struct passwd* pw = getpwuid(getuid());
    if (pw) {
        return std::string(pw->pw_dir);
    }
    
    return "";
#endif
}

std::string GetExtensionPath(const std::string& browser, const std::string& extension_id) {
    std::string base;
    std::string home = GetHomeDir();
    
    if (home.empty()) {
        LogError("Failed to get home directory");
        return "";
    }

#if defined(_WIN32)
    if (browser == "Chrome") {
        base = home + "\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Extensions\\";
    } else if (browser == "Edge") {
        base = home + "\\AppData\\Local\\Microsoft\\Edge\\User Data\\Default\\Extensions\\";
    } else if (browser == "Firefox") {
        // Firefox на Windows хранит расширения иначе
        LogInfo("Firefox extension path on Windows is not fully implemented yet");
        return "";
    }
#elif defined(__APPLE__)
    if (browser == "Chrome") {
        base = home + "/Library/Application Support/Google/Chrome/Default/Extensions/";
    } else if (browser == "Edge") {
        base = home + "/Library/Application Support/Microsoft Edge/Default/Extensions/";
    } else if (browser == "Firefox") {
        // Firefox на macOS хранит расширения иначе
        LogInfo("Firefox extension path on macOS is not fully implemented yet");
        return "";
    }
#elif defined(__linux__)
    if (browser == "Chrome") {
        base = home + "/.config/google-chrome/Default/Extensions/";
    } else if (browser == "Edge") {
        base = home + "/.config/microsoft-edge/Default/Extensions/";
    } else if (browser == "Firefox") {
        // Firefox на Linux хранит расширения иначе
        LogInfo("Firefox extension path on Linux is not fully implemented yet");
        return "";
    }
#endif
    else {
        LogWarning("Unsupported browser: " + browser);
        return "";
    }

    return base + extension_id;
}

std::string GetBrowserStoragePath(const std::string& browser) {
    std::string home = GetHomeDir();
    
    if (home.empty()) {
        LogError("Failed to get home directory");
        return "";
    }

#if defined(_WIN32)
    if (browser == "Chrome") {
        return home + "\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Local Storage\\leveldb\\";
    } else if (browser == "Edge") {
        return home + "\\AppData\\Local\\Microsoft\\Edge\\User Data\\Default\\Local Storage\\leveldb\\";
    }
#elif defined(__APPLE__)
    if (browser == "Chrome") {
        return home + "/Library/Application Support/Google/Chrome/Default/Local Storage/leveldb/";
    } else if (browser == "Edge") {
        return home + "/Library/Application Support/Microsoft Edge/Default/Local Storage/leveldb/";
    }
#elif defined(__linux__)
    if (browser == "Chrome") {
        return home + "/.config/google-chrome/Default/Local Storage/leveldb/";
    } else if (browser == "Edge") {
        return home + "/.config/microsoft-edge/Default/Local Storage/leveldb/";
    }
#endif

    LogWarning("Unsupported browser storage path: " + browser);
    return "";
}

std::string GetAppPath(const std::string& app_name) {
#if defined(_WIN32)
    // На Windows проверяем Program Files и Program Files (x86)
    std::vector<std::string> program_dirs = {
        "C:\\Program Files\\",
        "C:\\Program Files (x86)\\"
    };
    
    for (const auto& dir : program_dirs) {
        std::string app_path = dir + app_name;
        if (FileExists(app_path)) {
            return app_path;
        }
        
        // Проверяем с расширением .exe
        app_path = dir + app_name + "\\";
        if (FileExists(app_path)) {
            return app_path;
        }
        
        app_path = dir + app_name + "\\" + app_name + ".exe";
        if (FileExists(app_path)) {
            return app_path;
        }
    }
#elif defined(__APPLE__)
    // Проверяем стандартные директории для приложений
    std::vector<std::string> app_dirs = {
        "/Applications/",
        "/Applications/Utilities/",
        GetHomeDir() + "/Applications/"
    };
    
    for (const auto& dir : app_dirs) {
        std::string app_path = dir + app_name + ".app";
        if (FileExists(app_path)) {
            return app_path;
        }
    }
#elif defined(__linux__)
    // На Linux проверяем несколько стандартных путей
    std::vector<std::string> app_dirs = {
        "/usr/bin/",
        "/usr/local/bin/",
        "/opt/"
    };
    
    for (const auto& dir : app_dirs) {
        std::string app_path = dir + app_name;
        if (FileExists(app_path)) {
            return app_path;
        }
        
        // Проверяем с заглавной буквы
        std::string capitalized = app_name;
        if (!capitalized.empty()) {
            capitalized[0] = std::toupper(capitalized[0]);
        }
        app_path = dir + capitalized;
        if (FileExists(app_path)) {
            return app_path;
        }
    }
#endif

    return "";
}

std::string GetAppDataPath(const std::string& app_name) {
    std::string home = GetHomeDir();
    
    if (home.empty()) {
        LogError("Failed to get home directory");
        return "";
    }

#if defined(_WIN32)
    if (app_name == "Exodus") {
        return home + "\\AppData\\Roaming\\Exodus";
    } else if (app_name == "Trust Wallet") {
        return home + "\\AppData\\Roaming\\Trust Wallet";
    } else if (app_name == "TON Wallet") {
        return home + "\\AppData\\Roaming\\TON Wallet";
    }
#elif defined(__APPLE__)
    if (app_name == "Exodus") {
        return home + "/Library/Application Support/Exodus";
    } else if (app_name == "Trust Wallet") {
        return home + "/Library/Application Support/Trust Wallet";
    } else if (app_name == "TON Wallet") {
        return home + "/Library/Application Support/TON Wallet";
    }
#elif defined(__linux__)
    if (app_name == "Exodus") {
        return home + "/.config/exodus";
    } else if (app_name == "Trust Wallet") {
        return home + "/.config/trust-wallet";
    } else if (app_name == "TON Wallet") {
        return home + "/.config/ton-wallet";
    }
#endif

    LogWarning("Unknown app data path for: " + app_name);
    return "";
}

bool FileExists(const std::string& path) {
    return !path.empty() && std::filesystem::exists(path);
}

bool IsAppInstalled(const std::string& app_name) {
    // Сначала проверяем путь к приложению
    std::string app_path = GetAppPath(app_name);
    if (!app_path.empty()) {
        return true;
    }
    
    // Затем проверяем путь к данным приложения
    std::string data_path = GetAppDataPath(app_name);
    return !data_path.empty() && FileExists(data_path);
}

bool IsBrowserExtensionInstalled(const std::string& browser, const std::string& extension_id) {
    std::string extension_path = GetExtensionPath(browser, extension_id);
    return !extension_path.empty() && FileExists(extension_path);
}

std::string GetAppVersion(const std::string& app_name) {
#if defined(__APPLE__)
    std::string app_path = GetAppPath(app_name);
    if (app_path.empty()) {
        return "";
    }
    
    // Путь к Info.plist в пакете приложения
    std::string plist_path = app_path + "/Contents/Info.plist";
    
    if (!FileExists(plist_path)) {
        LogWarning("Info.plist not found: " + plist_path);
        return "";
    }
    
    // Используем defaults для чтения значения из plist
    std::string command = "/usr/bin/defaults read \"" + plist_path + "\" CFBundleShortVersionString";
    return ExecuteCommand(command);
#elif defined(_WIN32)
    // На Windows можно получать версию через Windows API или реестр
    // Для примера возвращаем заглушку
    return "1.0.0";
#elif defined(__linux__)
    // На Linux можно получать версию через командную строку
    // Для примера возвращаем заглушку
    return "1.0.0";
#else
    return "";
#endif
}

std::string GetExtensionVersion(const std::string& browser, const std::string& extension_id) {
    std::string extension_path = GetExtensionPath(browser, extension_id);
    if (extension_path.empty() || !FileExists(extension_path)) {
        return "";
    }
    
    // Ищем последнюю версию расширения (самую новую директорию)
    std::string latest_version = "";
    if (std::filesystem::is_directory(extension_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(extension_path)) {
            if (entry.is_directory()) {
                latest_version = entry.path().filename().string();
            }
        }
    }
    
    if (latest_version.empty()) {
        return "";
    }
    
    // Читаем manifest.json для получения версии
    std::string manifest_path = extension_path + "/" + latest_version + "/manifest.json";
    if (!FileExists(manifest_path)) {
        return "";
    }
    
    try {
        std::ifstream file(manifest_path);
        if (!file.is_open()) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        // Простой парсинг для поиска строки версии
        std::string content = buffer.str();
        size_t version_pos = content.find("\"version\"");
        if (version_pos == std::string::npos) {
            return "";
        }
        
        size_t colon_pos = content.find(":", version_pos);
        if (colon_pos == std::string::npos) {
            return "";
        }
        
        size_t quote1_pos = content.find("\"", colon_pos + 1);
        if (quote1_pos == std::string::npos) {
            return "";
        }
        
        size_t quote2_pos = content.find("\"", quote1_pos + 1);
        if (quote2_pos == std::string::npos) {
            return "";
        }
        
        return content.substr(quote1_pos + 1, quote2_pos - quote1_pos - 1);
    }
    catch (const std::exception& e) {
        LogError("Error reading extension manifest: " + std::string(e.what()));
        return "";
    }
}

std::string ExecuteCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;
    
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
    size_t last_non_whitespace = result.find_last_not_of(" \n\r\t");
    if (last_non_whitespace != std::string::npos) {
        result.erase(last_non_whitespace + 1);
    }
    
    return result;
}

} // namespace utils
} // namespace mceas