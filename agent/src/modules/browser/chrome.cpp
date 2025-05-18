#include "browser/chrome.h"
#include "internal/logging.h"
#include "browser/common.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include "external/json/json.hpp" // Используем nlohmann/json

namespace mceas {

ChromeAnalyzer::ChromeAnalyzer() {
    // Инициализация
    is_installed_ = false;
    
    try {
        // Проверяем, установлен ли Chrome
        is_installed_ = browser::isBrowserInstalledMacOS("Google Chrome");
        
        if (is_installed_) {
            // Получаем путь к исполняемому файлу
            executable_path_ = browser::getExecutablePathMacOS("Google Chrome");
            
            // Получаем версию
            version_ = browser::getBrowserVersionMacOS("Google Chrome", "CFBundleShortVersionString");
            
            // Получаем путь к профилю
            profile_path_ = browser::getProfilePathMacOS("Library/Application Support/Google/Chrome/Default");
        }
    }
    catch (const std::exception& e) {
        LogError("Error initializing Chrome analyzer: " + std::string(e.what()));
    }
}

bool ChromeAnalyzer::isInstalled() {
    return is_installed_;
}

BrowserType ChromeAnalyzer::getType() {
    return BrowserType::CHROME;
}

std::string ChromeAnalyzer::getName() {
    return "Chrome";
}

std::string ChromeAnalyzer::getVersion() {
    return version_;
}

std::string ChromeAnalyzer::getExecutablePath() {
    return executable_path_;
}

std::string ChromeAnalyzer::getProfilePath() {
    return profile_path_;
}

std::map<std::string, std::string> ChromeAnalyzer::collectData() {
    std::map<std::string, std::string> data;
    
    if (!isInstalled()) {
        LogWarning("Chrome is not installed, cannot collect data");
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
        LogError("Error collecting Chrome data: " + std::string(e.what()));
    }
    
    return data;
}

std::vector<Bookmark> ChromeAnalyzer::getBookmarks() {
    std::vector<Bookmark> bookmarks;
    
    try {
        // Путь к файлу закладок
        std::string bookmarks_path = profile_path_ + "/Bookmarks";
        
        if (!std::filesystem::exists(bookmarks_path)) {
            LogWarning("Chrome bookmarks file not found: " + bookmarks_path);
            return bookmarks;
        }
        
        // Читаем файл закладок
        std::ifstream file(bookmarks_path);
        if (!file.is_open()) {
            LogError("Failed to open Chrome bookmarks file: " + bookmarks_path);
            return bookmarks;
        }
        
        // Разбираем JSON
        nlohmann::json j;
        file >> j;
        file.close();
        
        // Заглушка: здесь должен быть код для разбора закладок Chrome из JSON
        // В реальном приложении здесь будет парсинг структуры закладок Chrome
        
        LogInfo("Successfully read Chrome bookmarks file");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Chrome bookmarks: " + std::string(e.what()));
    }
    
    return bookmarks;
}

std::vector<HistoryEntry> ChromeAnalyzer::getHistory(int limit) {
    std::vector<HistoryEntry> history;
    
    try {
        // Путь к файлу истории
        std::string history_path = profile_path_ + "/History";
        
        if (!std::filesystem::exists(history_path)) {
            LogWarning("Chrome history file not found: " + history_path);
            return history;
        }
        
        // Заглушка: здесь должен быть код для чтения истории Chrome из SQLite
        // В реальном приложении здесь будет подключение к базе SQLite и выполнение запроса
        
        LogInfo("Successfully read Chrome history file");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Chrome history: " + std::string(e.what()));
    }
    
    return history;
}

std::vector<Extension> ChromeAnalyzer::getExtensions() {
    std::vector<Extension> extensions;
    
    try {
        // Путь к директории расширений
        std::string extensions_path = profile_path_ + "/Extensions";
        
        if (!std::filesystem::exists(extensions_path)) {
            LogWarning("Chrome extensions directory not found: " + extensions_path);
            return extensions;
        }
        
        // Заглушка: здесь должен быть код для чтения расширений Chrome
        // В реальном приложении здесь будет обход директорий расширений и парсинг manifest.json
        
        LogInfo("Successfully read Chrome extensions");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Chrome extensions: " + std::string(e.what()));
    }
    
    return extensions;
}

} // namespace mceas