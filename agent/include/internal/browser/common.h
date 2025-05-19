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

// Структура для данных о записи истории
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
    std::string browser; // опционально — удобно при многобраузерной агрегации
};

// Пространство имён для работы с браузерами
namespace browser {

// Получение пути к профилю браузера
std::string getProfilePath(const std::string& browser_name);

// Получение пути к исполняемому файлу браузера
std::string getExecutablePath(const std::string& browser_name);

// Проверка, установлен ли браузер
bool isBrowserInstalled(const std::string& browser_name);

// Получение версии браузера с опциональным ключом версии
std::string getBrowserVersion(const std::string& browser_name, 
                            const std::string& version_key = "CFBundleShortVersionString");

// Получение пути к локальному хранилищу браузера
std::string getBrowserStoragePath(const std::string& browser_name);

// Проверка, установлено ли расширение
bool isExtensionInstalled(const std::string& browser_name, const std::string& extension_id);

// Получение версии установленного расширения
std::string getExtensionVersion(const std::string& browser_name, const std::string& extension_id);

// Получение закладок браузера
std::vector<Bookmark> readBookmarksFromFile(const std::string& browser_name);

// Получение истории браузера
std::vector<HistoryEntry> readHistoryFromFile(const std::string& browser_name, int limit = 100);

// Получение установленных расширений
std::vector<Extension> readExtensions(const std::string& browser_name);

} // namespace browser
    
} // namespace mceas

#endif // MCEAS_BROWSER_COMMON_H
