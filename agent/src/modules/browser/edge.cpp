#include "browser/edge.h"
#include "internal/logging.h"
#include "browser/common.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include "external/json/json.hpp" // Используем nlohmann/json

namespace mceas {

EdgeAnalyzer::EdgeAnalyzer() {
    // Инициализация
    is_installed_ = false;
    
    try {
        // Проверяем, установлен ли Edge (на macOS он называется Microsoft Edge)
        is_installed_ = browser::isBrowserInstalledMacOS("Microsoft Edge");
        
        if (is_installed_) {
            // Получаем путь к исполняемому файлу
            executable_path_ = browser::getExecutablePathMacOS("Microsoft Edge");
            
            // Получаем версию
            version_ = browser::getBrowserVersionMacOS("Microsoft Edge", "CFBundleShortVersionString");
            
            // Получаем путь к профилю (Edge использует структуру, похожую на Chrome)
            profile_path_ = browser::getProfilePathMacOS("Library/Application Support/Microsoft Edge/Default");
        }
    }
    catch (const std::exception& e) {
        LogError("Error initializing Edge analyzer: " + std::string(e.what()));
    }
}

bool EdgeAnalyzer::isInstalled() {
    return is_installed_;
}

BrowserType EdgeAnalyzer::getType() {
    return BrowserType::EDGE;
}

std::string EdgeAnalyzer::getName() {
    return "Edge";
}

std::string EdgeAnalyzer::getVersion() {
    return version_;
}

std::string EdgeAnalyzer::getExecutablePath() {
    return executable_path_;
}

std::string EdgeAnalyzer::getProfilePath() {
    return profile_path_;
}

std::map<std::string, std::string> EdgeAnalyzer::collectData() {
    std::map<std::string, std::string> data;
    
    if (!isInstalled()) {
        LogWarning("Edge is not installed, cannot collect data");
        return data;
    }
    
    try {
        // Добавляем базовую информацию
        data["version"] = version_;
        data["executable_path"] = executable_path_;
        data["profile_path"] = profile_path_;
        
        // Получаем закладки
        auto bookmarks = getBookmarks();
        data["bookmarks_count"] = std::to_string(bookmarks.size());
        
        // Получаем историю
        auto history = getHistory(100); // Ограничиваем 100 записями
        data["history_entries_count"] = std::to_string(history.size());
        
        // Получаем расширения
        auto extensions = getExtensions();
        data["extensions_count"] = std::to_string(extensions.size());
        
        // Создаем JSON с информацией о расширениях
        if (!extensions.empty()) {
            nlohmann::json extensions_json = nlohmann::json::array();
            
            for (const auto& ext : extensions) {
                nlohmann::json ext_json;
                ext_json["id"] = ext.id;
                ext_json["name"] = ext.name;
                ext_json["version"] = ext.version;
                ext_json["enabled"] = ext.enabled;
                
                extensions_json.push_back(ext_json);
            }
            
            data["extensions_json"] = extensions_json.dump();
        }
    }
    catch (const std::exception& e) {
        LogError("Error collecting Edge data: " + std::string(e.what()));
    }
    
    return data;
}

std::vector<Bookmark> EdgeAnalyzer::getBookmarks() {
    std::vector<Bookmark> bookmarks;
    
    try {
        // Edge использует формат закладок, аналогичный Chrome
        std::string bookmarks_path = profile_path_ + "/Bookmarks";
        
        if (!std::filesystem::exists(bookmarks_path)) {
            LogWarning("Edge bookmarks file not found: " + bookmarks_path);
            return bookmarks;
        }
        
        // Заглушка: здесь должен быть код для чтения закладок Edge
        // В реальном приложении здесь будет парсинг файла закладок
        
        LogInfo("Edge bookmarks analysis is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Edge bookmarks: " + std::string(e.what()));
    }
    
    return bookmarks;
}

std::vector<HistoryEntry> EdgeAnalyzer::getHistory(int limit) {
    std::vector<HistoryEntry> history;
    
    try {
        // Edge использует формат истории, аналогичный Chrome
        std::string history_path = profile_path_ + "/History";
        
        if (!std::filesystem::exists(history_path)) {
            LogWarning("Edge history file not found: " + history_path);
            return history;
        }
        
        // Заглушка: здесь должен быть код для чтения истории Edge
        // В реальном приложении здесь будет подключение к базе SQLite и выполнение запроса
        
        LogInfo("Edge history analysis is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Edge history: " + std::string(e.what()));
    }
    
    return history;
}

std::vector<Extension> EdgeAnalyzer::getExtensions() {
    std::vector<Extension> extensions;
    
    try {
        // Edge использует формат расширений, аналогичный Chrome
        std::string extensions_path = profile_path_ + "/Extensions";
        
        if (!std::filesystem::exists(extensions_path)) {
            LogWarning("Edge extensions directory not found: " + extensions_path);
            return extensions;
        }
        
        // Заглушка: здесь должен быть код для чтения расширений Edge
        // В реальном приложении здесь будет обход директорий расширений и парсинг manifest.json
        
        LogInfo("Edge extensions analysis is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Edge extensions: " + std::string(e.what()));
    }
    
    return extensions;
}

} // namespace mceas