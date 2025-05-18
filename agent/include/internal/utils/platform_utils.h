#ifndef MCEAS_PLATFORM_UTILS_H
#define MCEAS_PLATFORM_UTILS_H

#include <string>
#include <vector>
#include <map>

namespace mceas {
namespace utils {

// Получение пути к домашнему каталогу
std::string GetHomeDir();

// Получение кроссплатформенного пути к расширению браузера
std::string GetExtensionPath(const std::string& browser, const std::string& extension_id);

// Получение пути к локальному хранилищу браузера
std::string GetBrowserStoragePath(const std::string& browser);

// Получение кроссплатформенного пути к приложению
std::string GetAppPath(const std::string& app_name);

// Получение кроссплатформенного пути к данным приложения
std::string GetAppDataPath(const std::string& app_name);

// Проверка существования директории или файла
bool FileExists(const std::string& path);

// Проверка, установлено ли приложение
bool IsAppInstalled(const std::string& app_name);

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