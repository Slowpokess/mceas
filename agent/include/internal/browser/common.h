#ifndef MCEAS_BROWSER_COMMON_H
#define MCEAS_BROWSER_COMMON_H

#include <string>
#include <vector>
#include <map>

namespace mceas {

// Структура для данных о закладке
struct Bookmark {
    std::string title;
    std::string url;
    std::string added_date;
    std::string folder;
};

// Структура для данных о истории
struct HistoryEntry {
    std::string title;
    std::string url;
    std::string visit_date;
    int visit_count;
};

// Структура для данных о расширении
struct Extension {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    bool enabled;
};

// Общие функции для работы с браузерами
namespace browser {

// Получение пути к профилю браузера на macOS
std::string getProfilePathMacOS(const std::string& relative_path);

// Получение пути к исполняемому файлу браузера на macOS
std::string getExecutablePathMacOS(const std::string& app_name);

// Проверка, установлен ли браузер на macOS
bool isBrowserInstalledMacOS(const std::string& app_name);

// Извлечение версии браузера на macOS
std::string getBrowserVersionMacOS(const std::string& app_name, const std::string& plist_key);

} // namespace browser

} // namespace mceas

#endif // MCEAS_BROWSER_COMMON_H