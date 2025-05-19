#include "browser/common.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <array>
#include <memory>
#include "external/json/json.hpp" // Используем nlohmann/json

// Проверка на поддержку SQLite
#ifdef SQLITE_SUPPORT
#include <sqlite3.h>
#endif

namespace mceas {
namespace browser {

std::string getProfilePath(const std::string& browser_name) {
    return utils::GetBrowserProfileDir(browser_name);
}

std::string getExecutablePath(const std::string& browser_name) {
    return utils::GetBrowserExecutablePath(browser_name);
}

bool isBrowserInstalled(const std::string& browser_name) {
    return utils::IsBrowserInstalled(browser_name);
}

std::string getBrowserVersion(const std::string& browser_name) {
    return utils::GetBrowserVersion(browser_name);
}

std::string getBrowserStoragePath(const std::string& browser_name) {
    return utils::GetBrowserStoragePath(browser_name);
}

bool isExtensionInstalled(const std::string& browser_name, const std::string& extension_id) {
    return utils::IsBrowserExtensionInstalled(browser_name, extension_id);
}

std::string getExtensionVersion(const std::string& browser_name, const std::string& extension_id) {
    return utils::GetExtensionVersion(browser_name, extension_id);
}

std::vector<Bookmark> readBookmarksFromFile(const std::string& browser_name) {
    std::vector<Bookmark> bookmarks;
    
    std::string profile_path = getProfilePath(browser_name);
    if (profile_path.empty()) {
        LogWarning("Profile path is empty for browser: " + browser_name);
        return bookmarks;
    }
    
    std::string bookmarks_path;
    
    if (browser_name == "Chrome" || browser_name == "Edge") {
        // Chrome и Edge используют одинаковый формат закладок
#if defined(_WIN32)
        bookmarks_path = profile_path + "\\Bookmarks";
#else
        bookmarks_path = profile_path + "/Bookmarks";
#endif

        if (!utils::FileExists(bookmarks_path)) {
            LogWarning("Bookmarks file not found: " + bookmarks_path);
            return bookmarks;
        }

        try {
            // Чтение JSON файла закладок
            std::ifstream file(bookmarks_path);
            if (file.is_open()) {
                nlohmann::json j;
                file >> j;
                file.close();

                // Рекурсивная функция для обхода дерева закладок
                std::function<void(const nlohmann::json&, const std::string&)> parseBookmarkNode;
                
                parseBookmarkNode = [&bookmarks, &parseBookmarkNode](const nlohmann::json& node, const std::string& folder) {
                    if (node.contains("type") && node["type"] == "url") {
                        // Это закладка
                        Bookmark bookmark;
                        bookmark.title = node.value("name", "");
                        bookmark.url = node.value("url", "");
                        bookmark.added_date = std::to_string(node.value("date_added", 0));
                        bookmark.folder = folder;
                        bookmarks.push_back(bookmark);
                    } 
                    else if (node.contains("children")) {
                        // Это папка с вложенными закладками
                        std::string new_folder = folder;
                        if (node.contains("name")) {
                            if (!new_folder.empty()) {
                                new_folder += "/";
                            }
                            new_folder += node["name"].get<std::string>();
                        }
                        
                        // Обходим все дочерние элементы
                        for (const auto& child : node["children"]) {
                            parseBookmarkNode(child, new_folder);
                        }
                    }
                };

                // Начинаем с корневых папок
                if (j.contains("roots")) {
                    for (auto it = j["roots"].begin(); it != j["roots"].end(); ++it) {
                        parseBookmarkNode(it.value(), "");
                    }
                }
            }
        }
        catch (const std::exception& e) {
            LogError("Error parsing bookmarks file: " + std::string(e.what()));
        }
    } 
    else if (browser_name == "Firefox") {
        // Firefox использует SQLite для хранения закладок в places.sqlite
#if defined(_WIN32)
        bookmarks_path = profile_path + "\\places.sqlite";
#else
        bookmarks_path = profile_path + "/places.sqlite";
#endif

        if (!utils::FileExists(bookmarks_path)) {
            LogWarning("Firefox places.sqlite not found: " + bookmarks_path);
            return bookmarks;
        }

#ifdef SQLITE_SUPPORT
        try {
            sqlite3* db;
            int rc = sqlite3_open(bookmarks_path.c_str(), &db);
            if (rc != SQLITE_OK) {
                LogError("Cannot open SQLite database: " + std::string(sqlite3_errmsg(db)));
                sqlite3_close(db);
                return bookmarks;
            }

            const char* sql = "SELECT b.title, p.url, b.dateAdded, "
                             "(SELECT title FROM moz_bookmarks WHERE id = b.parent) as folder "
                             "FROM moz_bookmarks b "
                             "JOIN moz_places p ON b.fk = p.id "
                             "WHERE b.type = 1 "
                             "ORDER BY b.dateAdded DESC "
                             "LIMIT 100";
            
            sqlite3_stmt* stmt;
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                LogError("Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(db)));
                sqlite3_close(db);
                return bookmarks;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Bookmark bookmark;
                bookmark.title = (const char*)sqlite3_column_text(stmt, 0);
                bookmark.url = (const char*)sqlite3_column_text(stmt, 1);
                bookmark.added_date = std::to_string(sqlite3_column_int64(stmt, 2));
                
                // Папка может быть NULL
                if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                    bookmark.folder = (const char*)sqlite3_column_text(stmt, 3);
                }
                
                bookmarks.push_back(bookmark);
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        catch (const std::exception& e) {
            LogError("Error reading Firefox bookmarks: " + std::string(e.what()));
        }
#else
        LogWarning("SQLite support is not available, cannot read Firefox bookmarks");
#endif
    } 
    else {
        LogWarning("Unsupported browser for bookmarks: " + browser_name);
        return bookmarks;
    }
    
    LogInfo("Successfully read " + std::to_string(bookmarks.size()) + " bookmarks from " + browser_name);
    return bookmarks;
}

std::vector<HistoryEntry> readHistoryFromFile(const std::string& browser_name, int limit) {
    std::vector<HistoryEntry> history;
    
    std::string profile_path = getProfilePath(browser_name);
    if (profile_path.empty()) {
        LogWarning("Profile path is empty for browser: " + browser_name);
        return history;
    }
    
    std::string history_path;
    
    if (browser_name == "Chrome" || browser_name == "Edge") {
        // Chrome и Edge используют одинаковый формат истории
#if defined(_WIN32)
        history_path = profile_path + "\\History";
#else
        history_path = profile_path + "/History";
#endif

        if (!utils::FileExists(history_path)) {
            LogWarning("History file not found: " + history_path);
            return history;
        }

#ifdef SQLITE_SUPPORT
        try {
            // История хранится в SQLite
            sqlite3* db;
            int rc = sqlite3_open(history_path.c_str(), &db);
            if (rc != SQLITE_OK) {
                LogError("Cannot open SQLite database: " + std::string(sqlite3_errmsg(db)));
                sqlite3_close(db);
                return history;
            }

            const char* sql = "SELECT title, url, last_visit_time, visit_count "
                             "FROM urls "
                             "ORDER BY last_visit_time DESC "
                             "LIMIT ?";
            
            sqlite3_stmt* stmt;
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                LogError("Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(db)));
                sqlite3_close(db);
                return history;
            }

            sqlite3_bind_int(stmt, 1, limit);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                HistoryEntry entry;
                
                // Заголовок может быть NULL
                if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                    entry.title = (const char*)sqlite3_column_text(stmt, 0);
                }
                
                entry.url = (const char*)sqlite3_column_text(stmt, 1);
                entry.visit_date = std::to_string(sqlite3_column_int64(stmt, 2));
                entry.visit_count = sqlite3_column_int(stmt, 3);
                
                history.push_back(entry);
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        catch (const std::exception& e) {
            LogError("Error reading history: " + std::string(e.what()));
        }
#else
        LogWarning("SQLite support is not available, cannot read Chrome/Edge history");
#endif
    } 
    else if (browser_name == "Firefox") {
        // Firefox использует SQLite для хранения истории в places.sqlite
#if defined(_WIN32)
        history_path = profile_path + "\\places.sqlite";
#else
        history_path = profile_path + "/places.sqlite";
#endif

        if (!utils::FileExists(history_path)) {
            LogWarning("Firefox places.sqlite not found: " + history_path);
            return history;
        }

#ifdef SQLITE_SUPPORT
        try {
            sqlite3* db;
            int rc = sqlite3_open(history_path.c_str(), &db);
            if (rc != SQLITE_OK) {
                LogError("Cannot open SQLite database: " + std::string(sqlite3_errmsg(db)));
                sqlite3_close(db);
                return history;
            }

            const char* sql = "SELECT p.title, p.url, v.visit_date, p.visit_count "
                             "FROM moz_places p "
                             "JOIN moz_historyvisits v ON p.id = v.place_id "
                             "ORDER BY v.visit_date DESC "
                             "LIMIT ?";
            
            sqlite3_stmt* stmt;
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                LogError("Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(db)));
                sqlite3_close(db);
                return history;
            }

            sqlite3_bind_int(stmt, 1, limit);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                HistoryEntry entry;
                
                // Заголовок может быть NULL
                if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                    entry.title = (const char*)sqlite3_column_text(stmt, 0);
                }
                
                entry.url = (const char*)sqlite3_column_text(stmt, 1);
                entry.visit_date = std::to_string(sqlite3_column_int64(stmt, 2));
                entry.visit_count = sqlite3_column_int(stmt, 3);
                
                history.push_back(entry);
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        catch (const std::exception& e) {
            LogError("Error reading Firefox history: " + std::string(e.what()));
        }
#else
        LogWarning("SQLite support is not available, cannot read Firefox history");
#endif
    } 
    else {
        LogWarning("Unsupported browser for history: " + browser_name);
        return history;
    }
    
    LogInfo("Successfully read " + std::to_string(history.size()) + " history entries from " + browser_name);
    return history;
}

std::vector<Extension> readExtensions(const std::string& browser_name) {
    std::vector<Extension> extensions;
    
    std::string profile_path = getProfilePath(browser_name);
    if (profile_path.empty()) {
        LogWarning("Profile path is empty for browser: " + browser_name);
        return extensions;
    }
    
    std::string extensions_path;
    
    if (browser_name == "Chrome" || browser_name == "Edge") {
        // Chrome и Edge хранят расширения в папке Extensions
#if defined(_WIN32)
        extensions_path = profile_path + "\\Extensions";
#else
        extensions_path = profile_path + "/Extensions";
#endif
    } 
    else if (browser_name == "Firefox") {
        // Firefox хранит расширения в extensions.json
#if defined(_WIN32)
        extensions_path = profile_path + "\\extensions.json";
#else
        extensions_path = profile_path + "/extensions.json";
#endif
    } 
    else {
        LogWarning("Unsupported browser for extensions: " + browser_name);
        return extensions;
    }
    
    if (!utils::FileExists(extensions_path)) {
        LogWarning("Extensions path not found: " + extensions_path);
        return extensions;
    }
    
    LogInfo("Extensions path found at: " + extensions_path);
    
    if (browser_name == "Chrome" || browser_name == "Edge") {
        // Для Chrome и Edge нужно обойти папки с расширениями
        try {
            if (std::filesystem::is_directory(extensions_path)) {
                for (const auto& entry : std::filesystem::directory_iterator(extensions_path)) {
                    if (entry.is_directory()) {
                        std::string extension_id = entry.path().filename().string();
                        
                        // Поиск версии расширения
                        std::string latest_version = "";
                        for (const auto& ver_entry : std::filesystem::directory_iterator(entry.path())) {
                            if (ver_entry.is_directory()) {
                                latest_version = ver_entry.path().filename().string();
                            }
                        }
                        
                        if (!latest_version.empty()) {
                            // Чтение manifest.json
                            std::string manifest_path;
#if defined(_WIN32)
                            manifest_path = entry.path().string() + "\\" + latest_version + "\\manifest.json";
#else
                            manifest_path = entry.path().string() + "/" + latest_version + "/manifest.json";
#endif
                            
                            if (std::filesystem::exists(manifest_path)) {
                                try {
                                    // Чтение и парсинг JSON
                                    std::ifstream ifs(manifest_path);
                                    if (!ifs.is_open()) {
                                        LogWarning("Cannot open manifest file: " + manifest_path);
                                        continue;
                                    }
                                    
                                    nlohmann::json j;
                                    ifs >> j;
                                    ifs.close();
                                    
                                    Extension ext;
                                    ext.id = extension_id;
                                    ext.version = j.value("version", latest_version);
                                    ext.name = j.value("name", "Unknown");
                                    ext.description = j.value("description", "");
                                    ext.enabled = true;
                                    ext.browser = browser_name; // Добавляем информацию о браузере
                                    
                                    extensions.push_back(ext);
                                }
                                catch (const std::exception& e) {
                                    LogError("Error parsing manifest.json: " + std::string(e.what()));
                                }
                            }
                        }
                    }
                }
            }
        }
        catch (const std::exception& e) {
            LogError("Error reading extensions: " + std::string(e.what()));
        }
    }
    else if (browser_name == "Firefox") {
        // Для Firefox нужно прочитать extensions.json
        try {
            // Чтение и парсинг JSON
            std::ifstream ifs(extensions_path);
            if (!ifs.is_open()) {
                LogWarning("Cannot open extensions.json: " + extensions_path);
                return extensions;
            }
            
            nlohmann::json j;
            ifs >> j;
            ifs.close();
            
            // Чтение расширений из JSON
            if (j.contains("addons")) {
                for (const auto& ext : j["addons"]) {
                    if (ext.contains("id") && ext.contains("defaultLocale")) {
                        Extension e;
                        e.id = ext["id"];
                        e.name = ext["defaultLocale"]["name"];
                        e.version = ext.value("version", "1.0.0");
                        e.enabled = ext.value("active", true);
                        e.browser = browser_name; // Добавляем информацию о браузере
                        
                        if (ext["defaultLocale"].contains("description")) {
                            e.description = ext["defaultLocale"]["description"];
                        }
                        
                        extensions.push_back(e);
                    }
                }
            }
        }
        catch (const std::exception& e) {
            LogError("Error parsing extensions.json: " + std::string(e.what()));
        }
    }
    
    LogInfo("Successfully read " + std::to_string(extensions.size()) + " extensions from " + browser_name);
    return extensions;
}

} // namespace browser
} // namespace mceas

using json = nlohmann::json;

void to_json(json& j, const mceas::Bookmark& b) {
    j = json{
        {"title", b.title},
        {"url", b.url},
        {"added_date", b.added_date},
        {"folder", b.folder}
    };
}

void from_json(const json& j, mceas::Bookmark& b) {
    j.at("title").get_to(b.title);
    j.at("url").get_to(b.url);
    j.at("added_date").get_to(b.added_date);
    j.at("folder").get_to(b.folder);
}

void to_json(json& j, const mceas::HistoryEntry& h) {
    j = json{
        {"title", h.title},
        {"url", h.url},
        {"visit_date", h.visit_date},
        {"visit_count", h.visit_count}
    };
}

void from_json(const json& j, mceas::HistoryEntry& h) {
    j.at("title").get_to(h.title);
    j.at("url").get_to(h.url);
    j.at("visit_date").get_to(h.visit_date);
    j.at("visit_count").get_to(h.visit_count);
}

void to_json(json& j, const mceas::Extension& e) {
    j = json{
        {"id", e.id},
        {"name", e.name},
        {"version", e.version},
        {"description", e.description},
        {"enabled", e.enabled},
        {"browser", e.browser}
    };
}

void from_json(const json& j, mceas::Extension& e) {
    j.at("id").get_to(e.id);
    j.at("name").get_to(e.name);
    j.at("version").get_to(e.version);
    j.at("description").get_to(e.description);
    j.at("enabled").get_to(e.enabled);
    j.at("browser").get_to(e.browser);
}
