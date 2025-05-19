#ifndef MCEAS_PLATFORM_UTILS_H
#define MCEAS_PLATFORM_UTILS_H

#include <string>
#include <vector>
#include <map>

namespace mceas {
namespace utils {

// Определение платформы
#if defined(_WIN32)
    #define PLATFORM_WINDOWS
#elif defined(__APPLE__)
    #define PLATFORM_MACOS
#elif defined(__linux__)
    #define PLATFORM_LINUX
#else
    #error "Unsupported platform"
#endif

// Получение пути к домашнему каталогу
std::string GetHomeDir();

// Получение стандартных системных директорий
std::string GetAppDataDir();          // Данные приложений
std::string GetLocalAppDataDir();     // Локальные данные приложений
std::string GetProgramFilesDir();     // Директория программных файлов
std::string GetTempDir();             // Временная директория

// Получение кроссплатформенного пути к расширению браузера
std::string GetExtensionPath(const std::string& browser, const std::string& extension_id);

// Получение пути к локальному хранилищу браузера
std::string GetBrowserStoragePath(const std::string& browser);

// Получение кроссплатформенного пути к приложению
std::string GetAppPath(const std::string& app_name);

// Получение кроссплатформенного пути к профилю браузера
std::string GetBrowserProfileDir(const std::string& browser_name);

// Получение пути к исполняемому файлу браузера
std::string GetBrowserExecutablePath(const std::string& browser_name);

// Получение версии браузера
std::string GetBrowserVersion(const std::string& browser_name);

// Получение кроссплатформенного пути к данным приложения
std::string GetAppDataPath(const std::string& app_name);

// Проверка существования директории или файла
bool FileExists(const std::string& path);

// Проверка, установлено ли приложение
bool IsAppInstalled(const std::string& app_name);

// Проверка, установлен ли браузер
bool IsBrowserInstalled(const std::string& browser_name);

// Проверка, установлено ли расширение браузера
bool IsBrowserExtensionInstalled(const std::string& browser, const std::string& extension_id);

// Получение версии приложения
std::string GetAppVersion(const std::string& app_name);

// Получение версии расширения браузера
std::string GetExtensionVersion(const std::string& browser, const std::string& extension_id);

// Выполнение команды и получение вывода
std::string ExecuteCommand(const std::string& command);

} // namespace utils
} // namespace mceas

#endif // MCEAS_PLATFORM_UTILS_H